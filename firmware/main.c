#include "stm32f10x.h"
#include "recorder.h"

static Recorder trace;
static uint32_t epoch;
static volatile unsigned injection_step;

/* The vendor reset handler calls this before C runtime initialization.
   Keep the reset clock tree: HSI at 8 MHz with all bus dividers at one. */
void SystemInit(void) {}

static void pins_and_uart_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN;
    (void)RCC->APB1ENR;

    /* F1 GPIO config nibbles: 8 = input pull, A = AF push-pull 2 MHz,
       4 = floating input. ODR0 low selects a pull-down on the capture pin. */
    GPIOA->BRR = 1u;
    GPIOA->CRL = (GPIOA->CRL & ~((15u << 24) | 15u)) | (10u << 24) | 8u;
    GPIOA->CRH = (GPIOA->CRH & ~0xFF0u) | (10u << 4) | (4u << 8);
    USART1->BRR = 208u; /* round(8 MHz / 38400) */
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void timers_init(void)
{
    TIM2->PSC = 7u;
    TIM2->ARR = 0xFFFFu;
    TIM2->CCMR1 = TIM_CCMR1_CC1S_0; /* rising edge on TI1 */
    TIM2->CCER = TIM_CCER_CC1E;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0;
    TIM2->DIER = TIM_DIER_UIE | TIM_DIER_CC1IE;
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->CR1 = TIM_CR1_CEN;

    TIM3->PSC = 7u;
    TIM3->CR1 = TIM_CR1_ARPE;
    TIM3->ARR = 999u;
    TIM3->CCR1 = 500u;
    TIM3->CCMR1 = TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = 0;
    TIM3->DIER = TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CCER = TIM_CCER_CC1E;
    TIM3->CR1 |= TIM_CR1_CEN;
}

void TIM2_IRQHandler(void)
{
    uint32_t flags = TIM2->SR;
    bool update = (flags & TIM_SR_UIF) != 0;
    uint32_t base = epoch;
    if (update) {
        TIM2->SR = (uint16_t)~TIM_SR_UIF;
        epoch += 0x10000u;
    }
    if (flags & TIM_SR_CC1IF) {
        uint16_t captured = (uint16_t)TIM2->CCR1; /* acknowledges CC1IF */
        /* A second edge may have replaced CCR1 between SR and CCR1 reads. */
        if ((flags | TIM2->SR) & TIM_SR_CC1OF) {
            TIM2->SR = (uint16_t)~TIM_SR_CC1OF;
            recorder_lost(&trace);
        } else {
            recorder_edge(&trace, capture_timestamp(base, captured, update));
        }
    }
}

void TIM3_IRQHandler(void)
{
    TIM3->SR = (uint16_t)~TIM_SR_UIF;
    /* Preload transfers on the next update. Restore after the long period
       has entered the active register, before its ending edge. */
    if (injection_step == 1u) {
        TIM3->ARR = 1499u;
        injection_step = 2u;
    } else if (injection_step == 2u) {
        TIM3->ARR = 999u;
        injection_step = 0;
    }
}

static void put_char(char c)
{
    while (!(USART1->SR & USART_SR_TXE)) {}
    USART1->DR = (uint8_t)c;
}

static void put_text(const char *s)
{
    while (*s)
        put_char(*s++);
}

static void put_uint(uint32_t value)
{
    char digits[10];
    unsigned n = 0;
    do {
        digits[n++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value);
    while (n)
        put_char(digits[--n]);
}

static const char *state_name(CaptureState state)
{
    static const char *const names[] = {"ARMED", "POST", "FROZEN", "LOST"};
    return names[state];
}

static void command(char c)
{
    if (c == 'a') {
        __disable_irq();
        recorder_arm(&trace);
        __enable_irq();
        put_text("ARMED\r\n");
    } else if (c == 'g') {
        __disable_irq();
        bool ready = trace.state == ARMED && trace.count == PRE_PERIODS && !injection_step;
        if (ready)
            injection_step = 1u;
        __enable_irq();
        put_text(ready ? "INJECT 1500 us\r\n" : "NOT READY (check s)\r\n");
    } else if (c == 's' || c == 'd') {
        __disable_irq();
        CaptureState state = trace.state;
        unsigned count = trace.count;
        __enable_irq();
        if (c == 's' || state != FROZEN) {
            put_text(state_name(state));
            put_text(" periods=");
            put_uint(count);
            put_text("\r\n");
            return;
        }
        /* A frozen window is immutable until main processes 'a'. All UART
           transmission runs with interrupts enabled. */
        put_text("relative_index,period_us\r\n");
        for (unsigned i = 0; i < count; ++i) {
            if (i < trace.pre_count) {
                put_char('-');
                put_uint(trace.pre_count - i);
            } else {
                put_uint(i - trace.pre_count);
            }
            put_char(',');
            put_uint(recorder_period(&trace, i));
            put_text("\r\n");
        }
    } else if (c != '\n' && c != '\r') {
        put_text("s status | g glitch | d dump | a arm\r\n");
    }
}

int main(void)
{
    recorder_arm(&trace);
    pins_and_uart_init();
    timers_init();
    put_text("Pulse recorder 38400 8N1 | ? help\r\n");
    for (;;) {
        if (USART1->SR & USART_SR_RXNE)
            command((char)USART1->DR);
    }
}

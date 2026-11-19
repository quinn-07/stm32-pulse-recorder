param(
    [Parameter(Mandatory=$true)][string]$KeilRoot,
    [string]$PackRoot = (Join-Path $KeilRoot 'ARM/PACK'),
    [string]$DevicePack = '2.2.0',
    [string]$CmsisPack = '5.0.1'
)
$ErrorActionPreference = 'Stop'
$bin = Join-Path $KeilRoot 'ARM/ARMCLANG/bin'
$device = Join-Path $PackRoot "Keil/STM32F1xx_DFP/$DevicePack/Device"
$core = Join-Path $PackRoot "ARM/CMSIS/$CmsisPack/CMSIS/Include"
function Invoke-ArmTool([string]$name, [string[]]$arguments) {
    & (Join-Path $bin "$name.exe") @arguments
    if ($LASTEXITCODE -ne 0) { throw "$name failed ($LASTEXITCODE)" }
}
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Path build -Force | Out-Null
    $options = @('--target=arm-arm-none-eabi','-mcpu=cortex-m3','-mthumb','-std=c11',
        '-Os','-g','-Wall','-Wextra','-DSTM32F10X_MD','-Icore',
        "-I$device/Include","-I$core",'-c')
    Invoke-ArmTool armclang ($options + @('firmware/main.c','-o','build/main.o'))
    Invoke-ArmTool armclang ($options + @('core/recorder.c','-o','build/recorder.o'))
    Invoke-ArmTool armasm @('--cpu','Cortex-M3','--pd','__MICROLIB SETA 1',
        "$device/Source/ARM/startup_stm32f10x_md.s",'-o','build/startup.o')
    Invoke-ArmTool armlink @('--cpu','Cortex-M3','--library_type=microlib','--entry','Reset_Handler',
        '--scatter','firmware/flash.sct','--summary_stderr','--info=sizes',
        '--list','build/firmware.map','build/startup.o','build/main.o','build/recorder.o',
        '-o','build/firmware.axf')
    Invoke-ArmTool fromelf @('--i32combined','--output','build/pulse-recorder.hex','build/firmware.axf')
    Write-Output 'Firmware ready in build/pulse-recorder.hex'
} finally {
    Pop-Location
}

$ErrorActionPreference = 'Stop'
function Invoke-Checked([string]$name, [string[]]$arguments) {
    & $name @arguments
    if ($LASTEXITCODE -ne 0) { throw "$name failed ($LASTEXITCODE)" }
}

Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Path build -Force | Out-Null
    $options = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Icore','core/recorder.c')
    Invoke-Checked gcc ($options + @('tests/test_recorder.c','-o','build/test-recorder.exe'))
    Invoke-Checked gcc ($options + @('host/replay.c','-o','build/replay.exe'))
    Invoke-Checked './build/test-recorder.exe' @()
    $csv = @(Invoke-Checked './build/replay.exe' @())
    if (Compare-Object (Get-Content examples/replay.csv) $csv -SyncWindow 0) {
        throw 'Replay differs from examples/replay.csv'
    }
    $csv
} finally {
    Pop-Location
}

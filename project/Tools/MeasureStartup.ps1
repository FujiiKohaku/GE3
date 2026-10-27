param(
    [ValidateRange(1, 20)]
    [int]$runCount = 5
)

$ErrorActionPreference = 'Stop'
$projectPath = Split-Path -Parent $PSScriptRoot
$workspacePath = Split-Path -Parent $projectPath
$executablePath = Join-Path $workspacePath 'generated\outputs\Release\KohakuEngine.exe'
$reportPath = Join-Path $workspacePath 'generated\reports\startup-profiling'
New-Item -ItemType Directory -Path $reportPath -Force | Out-Null
$runRows = @()
$sampleRows = @()
$culture = [System.Globalization.CultureInfo]::InvariantCulture

for ($runIndex = 1; $runIndex -le $runCount; $runIndex += 1) {
    $startedAt = (Get-Date).AddSeconds(-1)
    $application = Start-Process -FilePath $executablePath -ArgumentList '--measure-startup','--exit-after-startup' -WorkingDirectory $projectPath -WindowStyle Hidden -PassThru
    try {
        if (-not $application.WaitForExit(90000)) { throw "Measurement run $runIndex timed out." }
        $application.Refresh()
        if ($application.ExitCode -ne 0) { throw "Measurement run $runIndex failed: $($application.ExitCode)" }
        $logFile = Get-ChildItem -LiteralPath (Join-Path $projectPath 'generated\logs') -Filter 'EngineLog_*.txt' -File |
            Where-Object { $_.LastWriteTime -ge $startedAt } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($null -eq $logFile) { throw 'Startup log was not found.' }
        $logLines = Get-Content -LiteralPath $logFile.FullName
        $logText = $logLines -join "`n"
        if ($logText -notmatch '\[StartupTime\] Application start to first frame presented: (\d+) ms') { throw 'First frame timing was not found.' }
        $startupMs = [int]$Matches[1]
        $logName = "measured-$runIndex.log"
        Copy-Item -LiteralPath $logFile.FullName -Destination (Join-Path $reportPath $logName) -Force
        $runSamples = @()
        foreach ($line in $logLines) {
            if ($line -match '\[StartupTiming\] name=(\S+) total_ms=([0-9.]+) calls=(\d+) max_ms=([0-9.]+)') {
                $runSamples += [PSCustomObject]@{
                    Run = $runIndex
                    Name = $Matches[1]
                    TotalMs = [double]::Parse($Matches[2], $culture)
                    Calls = [int]$Matches[3]
                    MaxMs = [double]::Parse($Matches[4], $culture)
                }
            }
        }
        if (@($runSamples | Where-Object { $_.Name -eq 'Main.Total' }).Count -ne 1) { throw 'Detailed startup timing was not found; rebuild Release first.' }
        $sampleRows += $runSamples
        $runRows += [PSCustomObject]@{Run=$runIndex;StartupMs=$startupMs;Log=$logName}
        Write-Output "Measured run $runIndex : $startupMs ms ($($runSamples.Count) categories)"
    } finally {
        if (-not $application.HasExited) { $application.Kill() }
    }
}

$sampleRows | Export-Csv -LiteralPath (Join-Path $reportPath 'samples.csv') -NoTypeInformation -Encoding utf8
$runRows | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $reportPath 'runs.json') -Encoding utf8
[PSCustomObject]@{
    Executable = $executablePath
    Sha256 = (Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash
    MeasuredAt = (Get-Date).ToString('o')
    Configuration = 'Release x64'
    WorkingDirectory = $projectPath
    RunCount = $runCount
    Timing = 'WinMain timer start to first frame presented; before profiler summary flush'
    Caveat = 'Sequential launches; OS and driver caches were not cleared.'
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $reportPath 'metadata.json') -Encoding utf8

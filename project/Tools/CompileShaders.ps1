param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot,

    [Parameter(Mandatory = $true)]
    [string]$DxcPath,

    [ValidateRange(1, 64)]
    [int]$MaxParallel = 12
)

$ErrorActionPreference = "Stop"

$projectRootPath = [System.IO.Path]::GetFullPath($ProjectRoot)
$resourcesPath = Join-Path $projectRootPath "resources"
$outputRootPath = Join-Path $resourcesPath "CompiledShaders"
$logDirectoryPath = Join-Path $projectRootPath "logs"
$logFilePath = Join-Path $logDirectoryPath "ShaderCompile.log"

New-Item -ItemType Directory -Path $outputRootPath -Force | Out-Null
New-Item -ItemType Directory -Path $logDirectoryPath -Force | Out-Null

function Write-ShaderLog {
    param(
        [string]$Level,
        [string]$Message
    )

    $line = "[{0}] [{1}] {2}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $Level, $Message
    Add-Content -LiteralPath $logFilePath -Value $line -Encoding utf8
    Write-Host $line
}

function Get-ShaderProfile {
    param([string]$FileName)

    if ($FileName -match "\.CS\.hlsl$") { return "cs_6_0" }
    if ($FileName -match "\.VS\.hlsl$") { return "vs_6_0" }
    if ($FileName -match "\.PS\.hlsl$") { return "ps_6_0" }
    if ($FileName -match "\.GS\.hlsl$") { return "gs_6_0" }
    return $null
}

if (-not (Test-Path -LiteralPath $DxcPath -PathType Leaf)) {
    Write-ShaderLog "Error" "dxc.exe was not found: $DxcPath"
    throw "dxc.exe was not found: $DxcPath"
}

$sourceRoots = @(
    Join-Path $resourcesPath "Shaders"
    Join-Path $resourcesPath "Effects"
)

$includeFiles = @()
foreach ($sourceRoot in $sourceRoots) {
    if (Test-Path -LiteralPath $sourceRoot -PathType Container) {
        $includeFiles += Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Filter "*.hlsli"
    }
}

$latestIncludeWriteTime = [System.DateTime]::MinValue
foreach ($includeFile in $includeFiles) {
    if ($includeFile.LastWriteTimeUtc -gt $latestIncludeWriteTime) {
        $latestIncludeWriteTime = $includeFile.LastWriteTimeUtc
    }
}

$compileJobs = New-Object System.Collections.ArrayList
$skippedCount = 0

foreach ($sourceRoot in $sourceRoots) {
    if (-not (Test-Path -LiteralPath $sourceRoot -PathType Container)) {
        continue
    }

    $shaderFiles = Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Filter "*.hlsl"
    foreach ($shaderFile in $shaderFiles) {
        $profile = Get-ShaderProfile $shaderFile.Name
        if ($null -eq $profile) {
            Write-ShaderLog "Warning" "Skipped shader with unknown stage suffix: $($shaderFile.FullName)"
            continue
        }

        $relativePath = $shaderFile.FullName.Substring($resourcesPath.TrimEnd([char[]]"\/").Length)
        $relativePath = $relativePath.TrimStart([char[]]"\/")
        $outputRelativePath = [System.IO.Path]::ChangeExtension($relativePath, ".dxil")
        $outputPath = Join-Path $outputRootPath $outputRelativePath
        $outputDirectory = Split-Path -Parent $outputPath

        $needsCompile = -not (Test-Path -LiteralPath $outputPath -PathType Leaf)
        if (-not $needsCompile) {
            $outputWriteTime = (Get-Item -LiteralPath $outputPath).LastWriteTimeUtc
            $needsCompile = $shaderFile.LastWriteTimeUtc -gt $outputWriteTime

            if (-not $needsCompile -and $latestIncludeWriteTime -gt $outputWriteTime) {
                $needsCompile = $true
            }
        }

        if (-not $needsCompile) {
            $skippedCount++
            continue
        }

        $compileJob = [PSCustomObject]@{
            SourcePath = $shaderFile.FullName
            RelativePath = $relativePath
            OutputPath = $outputPath
            OutputRelativePath = $outputRelativePath
            OutputDirectory = $outputDirectory
            Profile = $profile
        }
        $compileJobs.Add($compileJob) | Out-Null
    }
}

$compiledCount = 0
$failedShaderPaths = New-Object System.Collections.ArrayList
$runningJobs = New-Object System.Collections.ArrayList
$nextJobIndex = 0

Write-ShaderLog "Log" "Shader compile queue started. Pending: $($compileJobs.Count), Max parallel: $MaxParallel"

while ($nextJobIndex -lt $compileJobs.Count -or $runningJobs.Count -gt 0) {
    while ($nextJobIndex -lt $compileJobs.Count -and $runningJobs.Count -lt $MaxParallel) {
        $compileJob = $compileJobs[$nextJobIndex]
        $nextJobIndex++

        New-Item -ItemType Directory -Path $compileJob.OutputDirectory -Force | Out-Null

        $processArguments = '-E main -T {0} -O3 -Zpr -Fo "{1}" "{2}"' -f `
            $compileJob.Profile, `
            $compileJob.OutputPath, `
            $compileJob.SourcePath

        $processStartInfo = New-Object System.Diagnostics.ProcessStartInfo
        $processStartInfo.FileName = $DxcPath
        $processStartInfo.Arguments = $processArguments
        $processStartInfo.UseShellExecute = $false
        $processStartInfo.CreateNoWindow = $true
        $processStartInfo.RedirectStandardOutput = $true
        $processStartInfo.RedirectStandardError = $true

        $compilerProcess = New-Object System.Diagnostics.Process
        $compilerProcess.StartInfo = $processStartInfo

        Write-ShaderLog "Log" "Compiling $($compileJob.RelativePath) -> $($compileJob.OutputRelativePath) ($($compileJob.Profile))"
        if (-not $compilerProcess.Start()) {
            throw "Failed to start dxc.exe: $($compileJob.RelativePath)"
        }

        $runningJob = [PSCustomObject]@{
            CompileJob = $compileJob
            Process = $compilerProcess
        }
        $runningJobs.Add($runningJob) | Out-Null
    }

    for ($runningJobIndex = $runningJobs.Count - 1; $runningJobIndex -ge 0; $runningJobIndex--) {
        $runningJob = $runningJobs[$runningJobIndex]
        if (-not $runningJob.Process.HasExited) {
            continue
        }

        $standardOutput = $runningJob.Process.StandardOutput.ReadToEnd()
        $standardError = $runningJob.Process.StandardError.ReadToEnd()
        $runningJob.Process.WaitForExit()
        $exitCode = $runningJob.Process.ExitCode
        $runningJob.Process.Dispose()

        $compilerOutput = @()
        if (-not [string]::IsNullOrWhiteSpace($standardOutput)) {
            $compilerOutput += $standardOutput -split "\r?\n"
        }
        if (-not [string]::IsNullOrWhiteSpace($standardError)) {
            $compilerOutput += $standardError -split "\r?\n"
        }

        if ($exitCode -ne 0) {
            foreach ($outputLine in $compilerOutput) {
                if (-not [string]::IsNullOrWhiteSpace($outputLine)) {
                    Write-ShaderLog "Error" ([string]$outputLine)
                }
            }

            if (Test-Path -LiteralPath $runningJob.CompileJob.OutputPath -PathType Leaf) {
                Remove-Item -LiteralPath $runningJob.CompileJob.OutputPath -Force
            }

            $failedShaderPaths.Add($runningJob.CompileJob.RelativePath) | Out-Null
        } else {
            foreach ($outputLine in $compilerOutput) {
                if (-not [string]::IsNullOrWhiteSpace($outputLine)) {
                    Write-ShaderLog "Warning" ([string]$outputLine)
                }
            }

            $compiledCount++
        }

        $runningJobs.RemoveAt($runningJobIndex)
    }

    if ($runningJobs.Count -gt 0) {
        Start-Sleep -Milliseconds 20
    }
}

if ($failedShaderPaths.Count -gt 0) {
    $failedShaderList = $failedShaderPaths -join ", "
    throw "Shader compilation failed: $failedShaderList"
}

Write-ShaderLog "Log" "Shader build completed. Compiled: $compiledCount, Up-to-date: $skippedCount"

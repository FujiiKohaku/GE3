param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot,

    [Parameter(Mandatory = $true)]
    [string]$DxcPath
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

$compiledCount = 0
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

        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

        $arguments = @(
            "-E", "main",
            "-T", $profile,
            "-O3",
            "-Zpr",
            "-Fo", $outputPath,
            $shaderFile.FullName
        )

        Write-ShaderLog "Log" "Compiling $relativePath -> $outputRelativePath ($profile)"
        $compilerOutput = & $DxcPath @arguments 2>&1
        if ($LASTEXITCODE -ne 0) {
            foreach ($outputLine in $compilerOutput) {
                Write-ShaderLog "Error" ([string]$outputLine)
            }

            if (Test-Path -LiteralPath $outputPath -PathType Leaf) {
                Remove-Item -LiteralPath $outputPath -Force
            }

            throw "Shader compilation failed: $relativePath"
        }

        foreach ($outputLine in $compilerOutput) {
            Write-ShaderLog "Warning" ([string]$outputLine)
        }

        $compiledCount++
    }
}

Write-ShaderLog "Log" "Shader build completed. Compiled: $compiledCount, Up-to-date: $skippedCount"

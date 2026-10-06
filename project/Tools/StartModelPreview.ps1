param(
    [ValidateSet("Debug", "Release", "Development")]
    [string]$Configuration = "Release",
    [switch]$Build
)

$ErrorActionPreference = "Stop"
$previewProjectRoot = Split-Path -Parent $PSScriptRoot
$previewRepositoryRoot = Split-Path -Parent $previewProjectRoot
$previewExecutable = Join-Path $previewRepositoryRoot "generated/outputs/$Configuration/ModelPreview.exe"

if ($Build -or -not (Test-Path -LiteralPath $previewExecutable)) {
    $previewVswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
    $previewMsbuild = & $previewVswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild/**/Bin/MSBuild.exe" | Select-Object -First 1
    if (-not $previewMsbuild) { throw "Visual Studio MSBuild was not found" }
    $previewSolution = Join-Path $previewProjectRoot "KohakuEngine.sln"
    $previewSolutionConfiguration = $Configuration
    if ($Configuration -eq "Development") { $previewSolutionConfiguration = "Develop" }
    & $previewMsbuild $previewSolution /t:ModelPreview /m "/p:Configuration=$previewSolutionConfiguration" /p:Platform=x64 /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { throw "ModelPreview build failed" }
}

Start-Process -FilePath $previewExecutable -WorkingDirectory $previewProjectRoot -WindowStyle Hidden

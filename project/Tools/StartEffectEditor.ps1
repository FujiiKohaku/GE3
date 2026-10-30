param(
    [ValidateSet("Debug", "Release", "Development")]
    [string]$Configuration = "Release",
    [switch]$Build
)
$ErrorActionPreference = "Stop"
$editorProjectRoot = Split-Path -Parent $PSScriptRoot
$editorRepositoryRoot = Split-Path -Parent $editorProjectRoot
$editorExecutable = Join-Path $editorRepositoryRoot "generated/outputs/$Configuration/EffectEditor.exe"
if ($Build -or -not (Test-Path -LiteralPath $editorExecutable)) {
    $editorVswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
    $editorMsbuild = & $editorVswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild/**/Bin/MSBuild.exe" | Select-Object -First 1
    if (-not $editorMsbuild) { throw "Visual Studio MSBuild was not found" }
    & $editorMsbuild (Join-Path $editorProjectRoot "Tools/EffectEditor/EffectEditor.vcxproj") /m "/p:Configuration=$Configuration" /p:Platform=x64 /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { throw "EffectEditor build failed" }
}
Start-Process -FilePath $editorExecutable -WorkingDirectory $editorProjectRoot -WindowStyle Normal

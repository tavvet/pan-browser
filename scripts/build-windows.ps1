[CmdletBinding()]
param(
    [string]$QtRoot = $env:QT_ROOT,
    [string]$BuildDir,
    [string]$DistDir,
    [string]$Architecture = "x64"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectDir = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
    $BuildDir = Join-Path $ProjectDir "build-windows"
}
if (-not $DistDir) {
    $DistDir = Join-Path $ProjectDir "dist"
}

foreach ($CommandName in @("cmake", "ctest", "ninja")) {
    if (-not (Get-Command $CommandName -ErrorAction SilentlyContinue)) {
        throw "Required command not found: $CommandName"
    }
}

$ConfigureArguments = @(
    "-S", $ProjectDir,
    "-B", $BuildDir,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release"
)
if ($QtRoot) {
    $ConfigureArguments += "-DCMAKE_PREFIX_PATH=$QtRoot"
}

& cmake @ConfigureArguments
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
& cmake --build $BuildDir --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed" }
& ctest --test-dir $BuildDir --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Tests failed" }

$PackageName = "PanBrowser-windows-$Architecture"
$PackageDir = Join-Path $DistDir $PackageName
$Archive = Join-Path $DistDir "$PackageName.zip"
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
& cmake -E remove_directory $PackageDir
if ($LASTEXITCODE -ne 0) { throw "Cannot clean the package directory" }
& cmake -E rm -f $Archive
if ($LASTEXITCODE -ne 0) { throw "Cannot remove the previous archive" }
& cmake --install $BuildDir --prefix $PackageDir
if ($LASTEXITCODE -ne 0) { throw "Deployment failed" }

$Executable = Join-Path $PackageDir "bin\PanBrowser.exe"
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Deployment is missing the PanBrowser executable: $Executable"
}
if (-not (Get-ChildItem -LiteralPath $PackageDir -Recurse -File -Filter "QtWebEngineProcess.exe")) {
    throw "Deployment is missing QtWebEngineProcess.exe"
}
if (-not (Get-ChildItem -LiteralPath $PackageDir -Recurse -File -Filter "qtwebengine_resources.pak")) {
    throw "Deployment is missing Qt WebEngine resources"
}
$WebEngineLocales = Get-ChildItem -LiteralPath $PackageDir -Recurse -File -Filter "*.pak" | Where-Object {
    $_.Directory.Name -eq "qtwebengine_locales"
}
if (-not $WebEngineLocales) {
    throw "Deployment is missing Qt WebEngine locales"
}

Compress-Archive -LiteralPath $PackageDir -DestinationPath $Archive -CompressionLevel Optimal
Write-Output $Archive

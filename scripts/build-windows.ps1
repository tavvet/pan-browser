[CmdletBinding()]
param(
    [string]$QtRoot = $env:QT_ROOT,
    [string]$BuildDir,
    [string]$DistDir,
    [string]$Architecture = "x64",
    [string]$Generator = "Ninja",
    [switch]$OptimizeBundle,
    [switch]$KeepBaselineArchive,
    [string[]]$WebEngineLocales = @("en-US", "ru")
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
if ($KeepBaselineArchive -and -not $OptimizeBundle) {
    throw "-KeepBaselineArchive requires -OptimizeBundle"
}

foreach ($CommandName in @("cmake", "ctest")) {
    if (-not (Get-Command $CommandName -ErrorAction SilentlyContinue)) {
        throw "Required command not found: $CommandName"
    }
}
if ($Generator -eq "Ninja" -and -not (Get-Command "ninja" -ErrorAction SilentlyContinue)) {
    throw "Required command not found: ninja"
}

if ($QtRoot) {
    $QtRoot = [System.IO.Path]::GetFullPath($QtRoot)
    $QtBin = Join-Path $QtRoot "bin"
    if (-not (Test-Path -LiteralPath $QtBin -PathType Container)) {
        throw "Qt bin directory does not exist: $QtBin"
    }
    $env:PATH = "$QtBin$([System.IO.Path]::PathSeparator)$env:PATH"
}

$UsesVisualStudioGenerator = $Generator.StartsWith(
    "Visual Studio",
    [System.StringComparison]::OrdinalIgnoreCase
)

$ConfigureArguments = @(
    "-S", $ProjectDir,
    "-B", $BuildDir,
    "-G", $Generator
)
if ($UsesVisualStudioGenerator) {
    $ConfigureArguments += @("-A", $Architecture)
} else {
    $ConfigureArguments += "-DCMAKE_BUILD_TYPE=Release"
}
if ($QtRoot) {
    $ConfigureArguments += "-DCMAKE_PREFIX_PATH=$QtRoot"
}

& cmake @ConfigureArguments
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
$BuildArguments = @("--build", $BuildDir, "--parallel")
if ($UsesVisualStudioGenerator) {
    $BuildArguments += @("--config", "Release")
}
& cmake @BuildArguments
if ($LASTEXITCODE -ne 0) { throw "Build failed" }
$TestArguments = @("--test-dir", $BuildDir, "--output-on-failure")
if ($UsesVisualStudioGenerator) {
    $TestArguments += @("-C", "Release")
}
& ctest @TestArguments
if ($LASTEXITCODE -ne 0) { throw "Tests failed" }

$PackageName = "PanBrowser-windows-$Architecture"
$PackageDir = Join-Path $DistDir $PackageName
$Archive = Join-Path $DistDir "$PackageName.zip"
$BaselineArchive = Join-Path $DistDir "$PackageName-baseline.zip"
$BaselineReportPrefix = Join-Path $DistDir "$PackageName-baseline"
$BundleReportPrefix = Join-Path $DistDir "$PackageName-bundle"
$AuditScript = Join-Path $PSScriptRoot "audit-bundle.cmake"
$PruneLocalesScript = Join-Path $PSScriptRoot "prune-webengine-locales.cmake"

function Invoke-BundleAudit {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$ReportPrefix,
        [Parameter(Mandatory = $true)][string]$Label
    )

    & cmake `
        "-DBUNDLE_ROOT=$Root" `
        "-DOUTPUT_JSON=$ReportPrefix.json" `
        "-DOUTPUT_MARKDOWN=$ReportPrefix.md" `
        "-DBUNDLE_LABEL=$Label" `
        -P $AuditScript
    if ($LASTEXITCODE -ne 0) { throw "Bundle audit failed: $Label" }
}

function Compress-Package {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    & cmake -E rm -f $Destination
    if ($LASTEXITCODE -ne 0) { throw "Cannot remove the previous archive: $Destination" }
    Compress-Archive `
        -LiteralPath $Root `
        -DestinationPath $Destination `
        -CompressionLevel Optimal
    if (-not (Test-Path -LiteralPath $Destination -PathType Leaf)) {
        throw "Archive creation did not produce the expected file: $Destination"
    }
}

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
& cmake -E remove_directory $PackageDir
if ($LASTEXITCODE -ne 0) { throw "Cannot clean the package directory" }
foreach ($OldOutput in @(
    $Archive,
    $BaselineArchive,
    "$BaselineReportPrefix.json",
    "$BaselineReportPrefix.md",
    "$BundleReportPrefix.json",
    "$BundleReportPrefix.md"
)) {
    & cmake -E rm -f $OldOutput
    if ($LASTEXITCODE -ne 0) { throw "Cannot remove the previous output: $OldOutput" }
}
$InstallArguments = @("--install", $BuildDir, "--prefix", $PackageDir)
if ($UsesVisualStudioGenerator) {
    $InstallArguments += @("--config", "Release")
}
& cmake @InstallArguments
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
$DeployedWebEngineLocales = Get-ChildItem -LiteralPath $PackageDir -Recurse -File -Filter "*.pak" | Where-Object {
    $_.Directory.Name -eq "qtwebengine_locales"
}
if (-not $DeployedWebEngineLocales) {
    throw "Deployment is missing Qt WebEngine locales"
}

if ($OptimizeBundle) {
    Invoke-BundleAudit `
        -Root $PackageDir `
        -ReportPrefix $BaselineReportPrefix `
        -Label "$PackageName baseline"

    if ($KeepBaselineArchive) {
        Compress-Package -Root $PackageDir -Destination $BaselineArchive
    }

    $LocaleList = $WebEngineLocales -join ";"
    & cmake `
        "-DBUNDLE_ROOT=$PackageDir" `
        "-DKEEP_WEBENGINE_LOCALES=$LocaleList" `
        -P $PruneLocalesScript
    if ($LASTEXITCODE -ne 0) { throw "WebEngine locale pruning failed" }
}

$BundleLabel = if ($OptimizeBundle) {
    "$PackageName optimized"
} else {
    "$PackageName"
}
Invoke-BundleAudit `
    -Root $PackageDir `
    -ReportPrefix $BundleReportPrefix `
    -Label $BundleLabel
Compress-Package -Root $PackageDir -Destination $Archive

Write-Output $Archive
if ($KeepBaselineArchive) {
    Write-Output $BaselineArchive
}
Write-Output "$BundleReportPrefix.json"

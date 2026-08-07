[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Destination,
    [string]$QtVersion = "6.11.1"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($QtVersion -ne "6.11.1") {
    throw "This package manifest is pinned to Qt 6.11.1; requested: $QtVersion"
}

$BaseRevision = "6.11.1-0-202605090529"
$WebEngineRevision = "6.11.1-0-202605090527"

$Destination = [System.IO.Path]::GetFullPath($Destination)
$DestinationRoot = [System.IO.Path]::GetPathRoot($Destination)
if ($Destination -eq $DestinationRoot) {
    throw "Refusing to install Qt into a filesystem root: $Destination"
}

$RequiredFiles = @(
    "bin\Qt6Core.dll",
    "bin\qt.conf",
    "bin\windeployqt.exe",
    "lib\cmake\Qt6\Qt6Config.cmake",
    "lib\cmake\Qt6WebEngineWidgets\Qt6WebEngineWidgetsConfig.cmake",
    "plugins\platforms\qwindows.dll"
)
$Sentinel = Join-Path $Destination ".panbrowser-qt-install.json"
$InstallationComplete = $false
if (Test-Path -LiteralPath $Sentinel -PathType Leaf) {
    try {
        $InstallMetadata = Get-Content -LiteralPath $Sentinel -Raw | ConvertFrom-Json
        $InstallationComplete = (
            $InstallMetadata.qtVersion -eq $QtVersion -and
            $InstallMetadata.architecture -eq "msvc2022_64" -and
            $InstallMetadata.baseRevision -eq $BaseRevision -and
            $InstallMetadata.webEngineRevision -eq $WebEngineRevision
        )
    }
    catch {
        $InstallationComplete = $false
    }
}
foreach ($RelativePath in $RequiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $Destination $RelativePath) -PathType Leaf)) {
        $InstallationComplete = $false
    }
}
if ($InstallationComplete) {
    Write-Output "Using cached Qt $QtVersion installation: $Destination"
    return
}
if (Test-Path -LiteralPath $Destination) {
    if (-not (Test-Path -LiteralPath $Destination -PathType Container)) {
        throw "Qt destination exists but is not a directory: $Destination"
    }
    if (Get-ChildItem -LiteralPath $Destination -Force | Select-Object -First 1) {
        throw (
            "Qt destination exists but does not contain a complete matching installation: " +
            "$Destination. Choose an empty directory."
        )
    }
}

$SevenZip = Get-Command "7z.exe" -ErrorAction SilentlyContinue
if (-not $SevenZip) {
    throw "7z.exe is required to extract the official Qt archives"
}
if (-not (Get-Command "curl.exe" -ErrorAction SilentlyContinue)) {
    throw "curl.exe is required to download the official Qt archives"
}

$VersionCode = $QtVersion.Replace(".", "")
$PlatformSuffix = "-Windows-Windows_11_24H2-MSVC2022-Windows-Windows_11_24H2-X86_64.7z"
$DesktopRepository = (
    "https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/" +
    "qt6_$VersionCode/qt6_${VersionCode}_msvc2022_64"
)
$BasePackage = "qt.qt6.$VersionCode.win64_msvc2022_64"

$Packages = @(
    [pscustomobject]@{
        Repository = "$DesktopRepository/$BasePackage"
        Revision = $BaseRevision
        Archive = "qtbase$PlatformSuffix"
        Sha1 = "6f554628540ab947d48294e208cc3caae7f023d2"
        ExtractTo = "."
    },
    [pscustomobject]@{
        Repository = "$DesktopRepository/$BasePackage"
        Revision = $BaseRevision
        Archive = "qtsvg$PlatformSuffix"
        Sha1 = "353d70f58e5cd55d4217e3ec806a4b5dad4d320b"
        ExtractTo = "."
    },
    [pscustomobject]@{
        Repository = "$DesktopRepository/$BasePackage"
        Revision = $BaseRevision
        Archive = "qtdeclarative$PlatformSuffix"
        Sha1 = "7fb3f0f07ddcdb491c39f5dd4e42e0f0f07d2d72"
        ExtractTo = "."
    },
    [pscustomobject]@{
        Repository = "$DesktopRepository/$BasePackage"
        Revision = $BaseRevision
        Archive = "qttools$PlatformSuffix"
        Sha1 = "f8f33d511bbf38b85aa701425b41455b3512b36d"
        ExtractTo = "."
    },
    [pscustomobject]@{
        Repository = "$DesktopRepository/$BasePackage"
        Revision = $BaseRevision
        Archive = "qttranslations$PlatformSuffix"
        Sha1 = "0ed00fbd45f905f27ad18f35c0e5410857846209"
        ExtractTo = "."
    },
    [pscustomobject]@{
        Repository = "$DesktopRepository/$BasePackage"
        Revision = $BaseRevision
        Archive = "d3dcompiler_47-x64.7z"
        Sha1 = "e37f2611dc8b34bb3765e4866e6940bb3cd4306c"
        ExtractTo = "bin"
    },
    [pscustomobject]@{
        Repository = "$DesktopRepository/$BasePackage"
        Revision = $BaseRevision
        Archive = "opengl32sw-64-mesa_11_2_2-signed_sha256.7z"
        Sha1 = "fc005a470fec64edf464f517af8a4b7715f46fd2"
        ExtractTo = "bin"
    },
    [pscustomobject]@{
        Repository = "$DesktopRepository/qt.qt6.$VersionCode.addons.qtpositioning.win64_msvc2022_64"
        Revision = $BaseRevision
        Archive = "qtpositioning$PlatformSuffix"
        Sha1 = "1a2eb5e5bdf884905c6ecaa16407e6853c158148"
        ExtractTo = "."
    },
    [pscustomobject]@{
        Repository = "$DesktopRepository/qt.qt6.$VersionCode.addons.qtwebchannel.win64_msvc2022_64"
        Revision = $BaseRevision
        Archive = "qtwebchannel$PlatformSuffix"
        Sha1 = "524a335622b47105c6df64173b3e8664a118cc32"
        ExtractTo = "."
    },
    [pscustomobject]@{
        Repository = (
            "https://download.qt.io/online/qtsdkrepository/windows_x86/extensions/" +
            "qtwebengine/$VersionCode/msvc2022_64/" +
            "extensions.qtwebengine.$VersionCode.win64_msvc2022_64"
        )
        Revision = $WebEngineRevision
        Archive = "qtwebengine$PlatformSuffix"
        Sha1 = "6d4c6046b86a7495ec06e345d8fd4d24424270fc"
        ExtractTo = "."
    }
)

function Invoke-VerifiedDownload {
    param(
        [Parameter(Mandatory = $true)][string]$Uri,
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [Parameter(Mandatory = $true)][string]$ExpectedSha1
    )

    if ($ExpectedSha1 -notmatch "^[0-9a-fA-F]{40}$") {
        throw "Invalid pinned SHA-1 value for $Uri"
    }
    $ExpectedHash = $ExpectedSha1.ToLowerInvariant()

    & curl.exe `
        --fail `
        --location `
        --retry 5 `
        --retry-all-errors `
        --silent `
        --show-error `
        --output $OutputPath `
        $Uri
    if ($LASTEXITCODE -ne 0) {
        throw "Cannot download the Qt archive: $Uri"
    }

    $ActualHash = (Get-FileHash -LiteralPath $OutputPath -Algorithm SHA1).Hash.ToLowerInvariant()
    if ($ActualHash -ne $ExpectedHash) {
        throw "SHA-1 mismatch for $Uri (expected $ExpectedHash, got $ActualHash)"
    }
}

$DownloadDirectory = Join-Path (
    [System.IO.Path]::GetTempPath()
) "panbrowser-qt-$([System.Guid]::NewGuid().ToString('N'))"

try {
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    New-Item -ItemType Directory -Force -Path $DownloadDirectory | Out-Null

    foreach ($Package in $Packages) {
        $RemoteName = "$($Package.Revision)$($Package.Archive)"
        $Uri = "$($Package.Repository)/$RemoteName"
        $ArchivePath = Join-Path $DownloadDirectory $RemoteName
        $ExtractDirectory = Join-Path $Destination $Package.ExtractTo

        Write-Output "Downloading $($Package.Archive)"
        Invoke-VerifiedDownload `
            -Uri $Uri `
            -OutputPath $ArchivePath `
            -ExpectedSha1 $Package.Sha1
        New-Item -ItemType Directory -Force -Path $ExtractDirectory | Out-Null

        & $SevenZip.Source x $ArchivePath "-o$ExtractDirectory" -y -bd
        if ($LASTEXITCODE -ne 0) {
            throw "Cannot extract the Qt archive: $ArchivePath"
        }
    }

    @(
        "[Paths]",
        "Prefix=.."
    ) | Set-Content -LiteralPath (Join-Path $Destination "bin\qt.conf") -Encoding ascii

    foreach ($RelativePath in $RequiredFiles) {
        $RequiredPath = Join-Path $Destination $RelativePath
        if (-not (Test-Path -LiteralPath $RequiredPath -PathType Leaf)) {
            throw "Qt installation is incomplete; missing: $RequiredPath"
        }
    }

    [pscustomobject]@{
        schemaVersion = 1
        qtVersion = $QtVersion
        architecture = "msvc2022_64"
        baseRevision = $BaseRevision
        webEngineRevision = $WebEngineRevision
        archives = @($Packages | ForEach-Object {
            [pscustomobject]@{
                name = "$($_.Revision)$($_.Archive)"
                sha1 = $_.Sha1
            }
        })
    } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $Sentinel -Encoding utf8

    Write-Output "Installed Qt $QtVersion into $Destination"
}
finally {
    if (Test-Path -LiteralPath $DownloadDirectory) {
        Remove-Item -LiteralPath $DownloadDirectory -Recurse -Force
    }
}

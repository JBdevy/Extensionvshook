param(
    [Parameter(Mandatory = $true)]
    [string]$Destination
)

$ErrorActionPreference = 'Stop'
$version = '8.1.2'
$expectedHash = '464beb5e7bf0c311e68b45ae2f04e9cc2af88851abb4082231742a74d97b524c'
$destinationDir = [IO.Path]::GetFullPath($Destination)
$archive = Join-Path $destinationDir "ffmpeg-$version.tar.xz"
$sourceDir = Join-Path $destinationDir "ffmpeg-$version"
New-Item -ItemType Directory -Path $destinationDir -Force | Out-Null

if (-not (Test-Path -LiteralPath $archive)) {
    # Avoid the HTTP/2 reset seen with CMake's Schannel downloader.
    # Use --output so curl can discard partial data before retrying.
    Write-Host "Baixando os headers do FFmpeg $version por HTTP/1.1..."
    & curl.exe --http1.1 --fail --location --silent --show-error `
        --retry 5 --retry-all-errors --retry-delay 3 --retry-max-time 300 `
        --connect-timeout 30 --max-time 120 `
        --output "$archive.partial" `
        "https://ffmpeg.org/releases/ffmpeg-$version.tar.xz"
    if ($LASTEXITCODE -ne 0) {
        throw "Download dos headers do FFmpeg falhou (curl: $LASTEXITCODE)."
    }
    $downloadHash = (Get-FileHash -LiteralPath "$archive.partial" -Algorithm SHA256).Hash
    if ($downloadHash -ne $expectedHash) {
        throw 'O SHA-256 do download do FFmpeg nao confere.'
    }
    Move-Item -LiteralPath "$archive.partial" -Destination $archive -Force
}

if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) {
    throw 'O SHA-256 do arquivo FFmpeg em cache nao confere.'
}
& tar.exe -xf $archive -C $destinationDir
if ($LASTEXITCODE -ne 0) { throw 'Falha ao extrair os headers do FFmpeg.' }
if (-not (Test-Path -LiteralPath (Join-Path $sourceDir 'libavcodec/avcodec.h'))) {
    throw 'O arquivo extraido nao contem os headers esperados do FFmpeg.'
}

$env:VSHOOK_FFMPEG_SOURCE = $sourceDir
if ($env:GITHUB_ENV) {
    "VSHOOK_FFMPEG_SOURCE=$sourceDir" | Add-Content -LiteralPath $env:GITHUB_ENV -Encoding utf8
}
Write-Host "FFmpeg $version pronto em $sourceDir"

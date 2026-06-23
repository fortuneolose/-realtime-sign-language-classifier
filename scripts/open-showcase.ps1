$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$port = 8765
$url = "http://127.0.0.1:$port/showcase/"

$listener = Get-NetTCPConnection -LocalPort $port -ErrorAction SilentlyContinue
if (-not $listener) {
    Start-Process -FilePath "C:\Python313\python.exe" `
        -ArgumentList @("-m", "http.server", "$port", "--bind", "127.0.0.1") `
        -WorkingDirectory $projectRoot `
        -WindowStyle Hidden
    Start-Sleep -Seconds 2
}

Start-Process $url
Write-Host "Opened $url"

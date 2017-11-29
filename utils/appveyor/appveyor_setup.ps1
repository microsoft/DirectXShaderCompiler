$ErrorActionPreference = "Stop"
# Install ninja binaries.
$http="https://github.com/ninja-build/ninja/releases/download/v1.8.2/ninja-win.zip"
$tempFile="$env:TEMP\ninja.zip"
Write-Host "Downloading ninja to $tempFile"
Invoke-WebRequest $http -OutFile $tempFile

$ninjaDir = "C:\ninja"
Write-Host "Extracting ninja to $ninjaDir"
Expand-Archive -Path $tempFile -DestinationPath $ninjaDir -Force

Write-Host "ninja setup completed successfully"

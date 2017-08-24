$ErrorActionPreference = "Stop"
# Install TAEF binaries.
$http="https://github.com/Microsoft/WinObjC/raw/develop/deps/prebuilt/nuget/taef.redist.wlk.1.0.170206001-nativetargets.nupkg"
$tempFile="$env:TEMP\taef.zip"
Write-Host "Downloading TAEF to $tempFile"
Invoke-WebRequest $http -OutFile $tempFile

$taefDir = "$env:HLSL_SRC_DIR\external\taef"
Write-Host "Extracting TAEF to $taefDir"
Expand-Archive -Path $tempFile -DestinationPath $taefDir -Force

Write-Host "TAEF setup completed successfully"
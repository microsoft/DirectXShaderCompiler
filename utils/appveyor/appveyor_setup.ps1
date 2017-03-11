# Install the Windows Driver Kit (WDK). Only needed for TAEF.
$http="https://go.microsoft.com/fwlink/p/?LinkId=526733"
$path="$env:TEMP\wdk-installer.exe"
Write-Host "Downloading WDK to $path"
Invoke-WebRequest $http -OutFile $path

Write-Host "Installing WDK"
$sw =  [Diagnostics.Stopwatch]::StartNew()
Start-Process -FilePath $path -Wait -Args "/q /ceip off /features OptionId.WindowsDriverKitComplete"
$sw.Stop()
Write-Host ("Installation completed in {0:c}" -f $sw.Elapsed)
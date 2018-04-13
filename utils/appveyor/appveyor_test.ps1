function ConvertTo-AppveyorTestOutcome([string]$taefResult)
{
    switch($taefResult) {
        "Pass"    {"Passed"}
        "Fail"    {"Failed"}
        "Skip"    {"Skipped"}
        "Blocked" {"NotRunnable"}
        default   {"Inconclusive"}
    }
}

function ConvertTo-AppveyorTest([System.Xml.XmlNode] $endTest) {
    New-Object PSObject -Property @{
        testName = $endTest.Title
        testFramework = "TAEF"
        fileName = "clang-hlsl-tests.dll"
        outcome = ConvertTo-AppveyorTestOutcome($endTest.Result)
        durationMilliseconds = ""
        ErrorMessage = ""
        ErrorStackTrace = ""
        StdOut = ""
        StdErr = ""
    }
}

function Get-TaefResults($logfile) {
    [xml]$results = Get-Content $logfile
    $endTests = Select-Xml -Xml $results -XPath "/WTT-Logger/EndTest" | ForEach-Object {
        $test = $_.Node
        ConvertTo-AppveyorTest($test)
    }
    return ,@($endTests)
}

function Invoke-AppveyorTestsRestMethod($appveyorTests) {
    $uri = $env:APPVEYOR_API_URL + "/api/tests/batch"
    $json = ConvertTo-Json $appveyorTests
    Invoke-RestMethod -Uri $uri -Method Post -Body $json -ContentType "application/json"
}

function Invoke-TE($logfile) {
    $testdll = "$env:HLSL_BLD_DIR\Release\bin\clang-hlsl-tests.dll"
    $p = Start-Process "te.exe" -Args "$testdll /logOutput:Low /logFile:$logfile /enableWttLogging /p:HlslDataDir=%HLSL_SRC_DIR%\tools\clang\test\HLSL /labMode /miniDumpOnCrash" -Wait -NoNewWindow -PassThru
    return $p.ExitCode
}

$logfile = "$env:HLSL_BLD_DIR\testresults.xml"
Write-Host "Running taef tests"
$teExitCode = Invoke-TE $logfile
Write-Host "Parsing taef tests"
$testResults = Get-TaefResults $logfile
Write-Host "Uploading results to AppVeyor"
Invoke-AppveyorTestsRestMethod $testResults
Write-Host "Done"

exit $teExitCode
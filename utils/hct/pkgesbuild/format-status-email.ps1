param(
    [parameter(Mandatory=$true)]  [System.IO.FileInfo]$BuildLogs,
    [parameter(Mandatory=$false)] [System.IO.FileInfo]$EmailBodyFile = "email-body.html",
    [parameter(Mandatory=$false)] [string[]]$StatusItems = @("Build", "Tests"),
    [parameter(Mandatory=$false)] [string[]]$StatusItemsCanFail = @("Tests")
)

$platforms = "x64", "x86", "arm64"
$flavors = "Debug", "Release"

$overall_status =  "Succeeded"

function Get-TDStyle {
    param([string]$status)

    switch ($status) {
        "Succeeded"           { return "background-color:#BBFFBB" }
        "SucceededWithIssues" { return "background-color:#FFEE88" }
        "Failed"              { return "background-color:#FF8888" }
        default               { return "" }
    }
}

$status_tables=""
foreach ($si in $StatusItems) {
    $table_rows=""
    foreach ($flavor in $flavors) {
        foreach ($platform in $platforms) {
            $loc = "$BuildLogs\$flavor\$platform\$si.result"
            if (Test-Path $loc) { # if file exists
                # add a line to the status table
                $item_status = (Get-Content -Path $loc).Trim();
                $tdstyle = Get-TDStyle($item_status)
                $tr = "<tr><td width='100px'>$platform $flavor</td><td style='$tdstyle'>$item_status</td></tr>`n"
                $table_rows += $tr
                # adjust overall result 
                if ($overall_status.StartsWith("Succeeded") -and $item_status -ne "Succeeded") {
                    if ($si -in $StatusItemsCanFail) {
                        $overall_status="Succeeded with issues"
                    }
                    else {
                        $overall_status="Failed"
                    }
                    
                }
            }
        }
    }
    if ($table_rows -ne "") {
        $header = $si.Replace("_", " ")
        $status_tables += "<h3>$header</h3>`n<table>`n$table_rows</table>`n"
    }
}

if ($status_tables -eq "") {
    $overall_status = "Failed"
}

# find email template at the same location as the script
$script_root = Split-Path -Parent -Path $MyInvocation.MyCommand.Definition
$email_template_file = $script_root + "\status-email-template.html"

#create email body
$email_template = Get-Content $email_template_file
$email_body = $email_template.Replace("{status_tables}", $status_tables)

Set-Content -Path $EmailBodyFile -Value $email_body

# Set Azure pipelines output variable OverallStatus - to be used in the email subject
Write-Host "##vso[task.setvariable variable=OverallStatus;isOutput=true]$overall_status"

Write-Host $email_body`n
Write-Host $overall_status
    
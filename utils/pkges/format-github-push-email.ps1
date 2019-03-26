param(
    [parameter(Mandatory=$true)] $PushResult, # $(Push.Result)
    [parameter(Mandatory=$true)] $ShaOld,     # $(Push.ShaOld)
    [parameter(Mandatory=$true)] $ShaNew      # $(Push.ShaNew)
)

# Output VSO variables:
# ---------------------
# Status    - status of the integration to be put in email subject (Succeeded, Failed, Skipped, Conflicts)
# EmailBody

$agent_status="$(Agent.JobStatus)"

if ($agent_status -eq "Succeeded") {
    if ("$ShaOld" -eq "$ShaNew") {
        $status="Skipped"
        $emailbody="<p>No changes detected. Integration skipped.</p>"
    }
    else {
        $status="Succeeded"
        $changes=Invoke-Expression "git log $ShaOld..$ShaNew --oneline --no-decorate"
        $changes=$changes -join "</br>"
        $emailbody="<p><b>Changes integrated:</b></p><p>$changes</p>";
    }
} 
else {
    $status="Failed"
    $emailbody="<p>Integration failed.<br/>" + 
               "Summary: https://microsoft.visualstudio.com/Xbox/Xbox%20Team/_build/index?buildId=$(Build.BuildId)&_a=summary</p>"
} 

Write-host $status
Write-host $emailbody
        
Write-host "##vso[task.setvariable variable=Status;isOutput=true]$status"
Write-host "##vso[task.setvariable variable=EmailBody;isOutput=true]$emailbody"

param(
    [parameter(Mandatory=$true)] [string]$UserAlias, 
    [parameter(Mandatory=$true)] [string]$PAT,       
    [parameter(Mandatory=$false)] [string]$GithubRemoteName = "origin",
    [parameter(Mandatory=$false)] [string]$SCDXCRemoteName
)

# Output VSO variables:
# ---------------------
# ShaOld     - SHA of SC.DXC.master before the push
# ShaNew     - SHA of GitHub master; equal to SC.DXC.master after successfull push
# Result     - operation result (Succeeded, Failed, Skipped)

[string]$remote_url=git remote get-url $GithubRemoteName
if ($remote_url -ne "https://github.com/Microsoft/DirectXShaderCompiler.git") {
    Write-host "Error: this script needs to run in DirectXShaderCompiler GitHub enlistment."
    exit 1
}

$push_remote=$SCDXCRemoteName
if ($push_remote -eq "") {
    #set up authenticated remote for the push
    $push_remote_url="https://$UserName" + ":$PAT@microsoft.visualstudio.com/Xbox/_git/Xbox.ShaderCompiler.DXC"
    $push_remote="sc.dxc"
    git remote add $push_remote $push_remote_url
}

git checkout master
$sha_new=git rev-parse HEAD
Write-host "Github master commit: $sha_new"

git fetch sc.dxc master
$sha_old=git rev-parse refs/remotes/sc.dxc/master
Write-host "SC.DXC master commit: $sha_old"

if ($sha_old -eq $sha_new) {
    Write-host "No new changes detected."
    $result="Skipped"
}
else {
    Write-Host "Pushing changes from Github master to SC.DXC.Master"
    git push sc.dxc master

    # check the latest sha matches
    $sha_after=git rev-parse refs/remotes/sc.dxc/master
    if ($sha_after -eq $sha_new) {
        Write-Host "Done!"
        $result="Succeeded"
    }
    else {
        Write-Host "Something went wrong. Commit SHA after integration is $sha_after, expected $sha_new."
        $result="Failed"
        $exitcode=1
    }
}

if ($SCDXCRemoteName -eq "") {
    git remote remove $push_remote 
}

echo "##vso[task.setvariable variable=ShaOld;isOutput=true]$sha_old"
echo "##vso[task.setvariable variable=ShaNew;isOutput=true]$sha_new"
echo "##vso[task.setvariable variable=Result;isOutput=true]$result"

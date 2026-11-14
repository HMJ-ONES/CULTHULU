[CmdletBinding()]
param([string]$SourceCheckpoint = '')

$ErrorActionPreference = 'Stop'
$taskProject = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if ((Split-Path $taskProject -Leaf) -ne 'unreal 5.8') { throw 'Use the canonical Unreal project tools directory.' }
$taskStage = Join-Path $taskProject 'Saved/StagedBuilds/Windows'
$taskExe = Join-Path $taskStage 'CultUlhu/Binaries/Win64/CultUlhu.exe'
if (-not (Test-Path -LiteralPath $taskExe -PathType Leaf)) { throw 'Cook/stage the Development Windows game before this check.' }
$taskLog = Join-Path $taskProject 'Saved/Logs/HumanMeshStandalone.log'
$taskReport = Join-Path $taskProject 'Saved/Reports/HumanMeshStandalone.json'
New-Item -ItemType Directory -Force -Path (Split-Path $taskLog),(Split-Path $taskReport) | Out-Null
if (Test-Path -LiteralPath $taskLog) { Remove-Item -LiteralPath $taskLog }
$taskTests = @('CultUlhu.Runtime.HumanMeshMerge','CultUlhu.Runtime.HumanMeshProfile','CultUlhu.Runtime.MergedHumanPoses')
$taskArgs = @('-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-unattended','-nosound','-nosplash',
    ('-ExecCmds="Automation RunTests ' + ($taskTests -join '+') + '"'),
    '-TestExit="Automation Test Queue Empty"',('-abslog="' + $taskLog + '"'))
$taskStarted = (Get-Date).ToUniversalTime().ToString('o')
$taskProcess = Start-Process -FilePath $taskExe -ArgumentList $taskArgs -WorkingDirectory $taskStage -WindowStyle Hidden -PassThru
Write-Output ('Standalone verification PID: ' + $taskProcess.Id)
$taskProcess.WaitForExit()
$taskText = Get-Content -LiteralPath $taskLog -Raw
$taskResults = foreach ($taskTest in $taskTests) {
    $taskPattern = 'Test Completed\. Result=\{(Success|Fail)\}.*Path=\{' + [regex]::Escape($taskTest) + '\}'
    $taskMatches = [regex]::Matches($taskText,$taskPattern)
    [pscustomobject]@{ test=$taskTest; result=if($taskMatches.Count -eq 1) {$taskMatches[0].Groups[1].Value} else {'MissingOrRepeated'} }
}
$taskErrors = @($taskText -split '\r?\n' | Where-Object { $_ -match 'LogAutomationController: Error:' })
$taskPassed = $taskProcess.ExitCode -eq 0 -and @($taskResults | Where-Object result -ne 'Success').Count -eq 0 -and $taskErrors.Count -eq 0
[pscustomobject]@{
    started_utc=$taskStarted; source_checkpoint=$SourceCheckpoint; exe=$taskExe
    exe_sha256=(Get-FileHash -LiteralPath $taskExe -Algorithm SHA256).Hash.ToLowerInvariant()
    arguments=$taskArgs; source_project_argument=$false; exit_code=$taskProcess.ExitCode
    log=$taskLog; log_sha256=(Get-FileHash -LiteralPath $taskLog -Algorithm SHA256).Hash.ToLowerInvariant()
    results=@($taskResults); automation_errors=$taskErrors; passed=$taskPassed
    scope='Independent Development executable: original mesh data, live Free Roam comparison, original activity poses. Not a foreground FPS or human fun test.'
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $taskReport -Encoding UTF8
$taskResults | Format-Table -AutoSize
Write-Output ('Verification report: ' + $taskReport)
if (-not $taskPassed) { throw 'Standalone humanoid verification failed; inspect the saved log and report.' }

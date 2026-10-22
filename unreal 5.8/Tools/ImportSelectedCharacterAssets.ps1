[CmdletBinding()]
param([switch]$StageDownloads,[switch]$Portraits)
$ErrorActionPreference='Stop'
$taskProject=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if((Split-Path $taskProject -Leaf) -ne 'unreal 5.8') { throw 'Canonical project required.' }
$taskEngine='C:\Program Files\Epic Games\UE_5.8\Engine'
if($StageDownloads) { & "$taskEngine\Binaries\ThirdParty\Python3\Win64\python.exe" "$PSScriptRoot\StageApprovedCharacterDownloads.py"; if($LASTEXITCODE -ne 0) { throw 'Staging failed; original files preserved.' } }
$taskScript=if($Portraits){'ImportCharacterPortraits.py'}else{'ImportSelectedCharacterAssets.py'}
& "$taskEngine\Binaries\Win64\UnrealEditor-Cmd.exe" "$taskProject\CultUlhu.uproject" "-ExecutePythonScript=$PSScriptRoot\$taskScript" -unattended -nosound -nosplash '-DisablePlugins=ModelContextProtocol' "-abslog=$taskProject\Saved\Logs\ApprovedCharacterImport.log"
if($LASTEXITCODE -ne 0) { throw 'Editor import failed. See Saved/Logs/ApprovedCharacterImport.log.' }
if(Select-String -LiteralPath "$taskProject\Saved\Logs\ApprovedCharacterImport.log" -Pattern 'LogPython: Error:' -Quiet) { throw 'Python import reported errors. See Saved/Logs/ApprovedCharacterImport.log; earlier assets are preserved.' }
if(-not $Portraits) { Get-Content -LiteralPath "$taskProject\Saved\Reports\CharacterModelImport.json" }

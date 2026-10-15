<# SetupCultUlhu.ps1 — CULT-ULHU one-shot Windows setup.
#
# Does everything automatable BEFORE opening the Unreal Editor:
#   1. Validates paths and prerequisites (UE 5.8, project, git, python)
#   2. Validates the canonical project layout (unreal 5.8/, no legacy unreal/)
#   3. Decodes the binary assets (assets/decode_assets.py)
#   4. Generates Visual Studio project files (UnrealBuildTool)
#   5. Compiles the CultUlhu C++ module (Development Editor, Win64)
#
# RUN: right-click -> "Run with PowerShell" (or in PowerShell: .\SetupCultUlhu.ps1)
# Run it from anywhere; paths below point at the standard install layout.
# After it succeeds: open the editor with
#   "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Users\H\Documents\CULTHULU\unreal 5.8\CultUlhu.uproject"
# then Tools -> Execute Python Script -> unreal 5.8/Tools/BuildAll.py

$ErrorActionPreference = "Stop"

# ---------------- configuration ----------------
$UeDir       = "C:\Program Files\Epic Games\UE_5.8"
$RepoDir     = "C:\Users\H\Documents\CULTHULU"
$ProjectPath = "C:\Users\H\Documents\CULTHULU\unreal 5.8\CultUlhu.uproject"
$Ubt         = Join-Path $UeDir "Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe"

function Fail($msg) { Write-Host ""; Write-Host "FAILED: $msg" -ForegroundColor Red; exit 1 }
function Ok($msg)   { Write-Host "  [OK] $msg" -ForegroundColor Green }
function Step($msg) { Write-Host ""; Write-Host "== $msg" -ForegroundColor Cyan }

# ---------------- 1. prerequisites ----------------
Step "1/5 validating prerequisites"
if (-not (Test-Path $UeDir))       { Fail "UE 5.8 not found at $UeDir" }
Ok "Unreal Engine 5.8 at $UeDir"
if (-not (Test-Path $ProjectPath)) { Fail ".uproject not found at $ProjectPath" }
Ok "project at $ProjectPath"
if (-not (Test-Path $Ubt))         { Fail "UnrealBuildTool not found at $Ubt" }
Ok "UnrealBuildTool present"
$git = Get-Command git -ErrorAction SilentlyContinue
if (-not $git) { Fail "git not found — install Git for Windows first" }
Ok "git present"
$py = Get-Command python -ErrorAction SilentlyContinue
if (-not $py) { Fail "python not found — install Python 3 first" }
Ok "python present"

# ---------------- 2. validate canonical layout ----------------
Step "2/5 validating canonical project layout"
$validate = Join-Path $RepoDir "unreal 5.8\Tools\ValidateCultUlhu.py"
if (-not (Test-Path $validate)) { Fail "validator missing: $validate (did you git pull game-core?)" }
& python "$validate"
if ($LASTEXITCODE -ne 0) { Fail "project validation failed — fix the layout before building" }
Ok "canonical layout validated"

# ---------------- 3. decode assets ----------------
Step "3/5 decoding binary assets"
$decode = Join-Path $RepoDir "assets\decode_assets.py"
if (-not (Test-Path $decode)) { Fail "decode script missing: $decode (did you git pull game-core?)" }
Push-Location $RepoDir
try {
    & python assets/decode_assets.py
    if ($LASTEXITCODE -ne 0) { Fail "decode_assets.py exited with code $LASTEXITCODE" }
} finally { Pop-Location }
Ok "assets decoded"

# ---------------- 3. project files ----------------
Step "4/5 generating project files (this can take a few minutes)"
& $Ubt -projectfiles -project="$ProjectPath" -game -rocket -progress
if ($LASTEXITCODE -ne 0) { Fail "project file generation failed (code $LASTEXITCODE)" }
Ok "project files generated"

# ---------------- 4. compile the module ----------------
Step "5/5 compiling CultUlhuEditor (Win64 Development — several minutes)"
& $Ubt CultUlhuEditor Win64 Development -Project="$ProjectPath" -WaitMutex
if ($LASTEXITCODE -ne 0) {
    Fail ("compile failed (code $LASTEXITCODE). " +
          "Open the .sln in Visual Studio, build CultUlhuEditor, and send the first error back.")
}
Ok "CultUlhu module compiled"

Write-Host ""
Write-Host "SUCCESS — the project is editor-ready." -ForegroundColor Green
Write-Host "NEXT:"
Write-Host '  1. Open the editor:'
Write-Host '     "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Users\H\Documents\CULTHULU\unreal 5.8\CultUlhu.uproject"'
Write-Host "  2. In the editor: Tools -> Execute Python Script -> unreal 5.8/Tools/BuildAll.py"
Write-Host "  3. With L_MainMenu open, press Play — the home screen should appear."

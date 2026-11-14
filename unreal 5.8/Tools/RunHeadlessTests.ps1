param([string]$CMake='C:\Program Files\CMake\bin\cmake.exe',
 [string]$Python='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$repoRoot=Split-Path $projectRoot -Parent
& $Python (Join-Path $PSScriptRoot 'ValidateCultUlhu.py')
if($LASTEXITCODE) { throw 'Canonical project validation failed; no build launched.' }
$buildRoot=Join-Path $projectRoot 'Intermediate/CanonicalCore'
# Keep generated files in the canonical project's existing build directory.
& $CMake -S $repoRoot -B $buildRoot -G 'Visual Studio 17 2022' -A x64
if($LASTEXITCODE) { throw 'Headless configuration failed; no tests launched.' }
& $CMake --build $buildRoot --config Release --parallel 2
if($LASTEXITCODE) { throw 'Headless build failed; no stale test binaries launched.' }
& (Join-Path (Split-Path $CMake) 'ctest.exe') --test-dir $buildRoot -C Release --output-on-failure --interactive-debug-mode 0 --timeout 60
if($LASTEXITCODE) { throw 'Headless regression failed. Inspect the CTest output.' }

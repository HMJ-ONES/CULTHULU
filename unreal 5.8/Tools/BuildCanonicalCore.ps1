param([string]$CMake = 'C:\Program Files\CMake\bin\cmake.exe', [switch]$Test)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildRoot = Join-Path $projectRoot 'Intermediate/EngineCore'
& $CMake -S (Join-Path $PSScriptRoot 'CanonicalCore') -B $buildRoot -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE) { throw 'Core configuration failed' }
& $CMake --build $buildRoot --config Release --parallel 2
if ($LASTEXITCODE) { throw 'Core build failed' }
$libRoot = Join-Path $projectRoot 'lib/Win64'
New-Item -ItemType Directory -Force $libRoot | Out-Null
Copy-Item -LiteralPath (Join-Path $buildRoot 'Release/cultulhu.lib') -Destination (Join-Path $libRoot 'cultulhu.lib')
if ($Test) {
 & (Join-Path (Split-Path $CMake) 'ctest.exe') --test-dir $buildRoot -C Release --output-on-failure
 if ($LASTEXITCODE) { throw 'Core regression failed' }
}

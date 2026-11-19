param(
    [ValidateSet("Mac", "Windows", "Linux")]
    [string]$Platform = "Windows",

    [ValidateSet("debug", "release")]
    [string]$Variant = "debug",

    [string]$Filter = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$ctest = "ctest"
$programFilesCtest = Join-Path $env:ProgramFiles "CMake\bin\ctest.exe"

if (Test-Path $programFilesCtest) {
    $ctest = $programFilesCtest
}

switch ($Platform) {
    "Windows" {
        $configuration = if ($Variant -eq "debug") { "Debug" } else { "Release" }
        $buildName = "complex-$Variant-windows"
        $buildDir = Join-Path $repoRoot "build\$buildName"
        $ctestArgs = @(
            "--test-dir", $buildDir,
            "-C", $configuration,
            "--output-on-failure"
        )
    }
    "Mac" {
        $buildDir = Join-Path $repoRoot "build\complex-$Variant-macos"
        $manifest = Join-Path $buildDir "vulkan\icd.d\libkosmickrisp_icd.json"
        if (Test-Path $manifest) {
            $env:VK_ICD_FILENAMES = $manifest
        }
        $ctestArgs = @(
            "--test-dir", $buildDir,
            "--output-on-failure"
        )
    }
    "Linux" {
        $buildName = "complex-$Variant-linux"
        $buildDir = Join-Path $repoRoot "build\$buildName"
        $ctestArgs = @(
            "--test-dir", $buildDir,
            "--output-on-failure"
        )
    }
}

if ($Filter -ne "") {
    $ctestArgs += @("-R", $Filter)
}

& $ctest @ctestArgs
exit $LASTEXITCODE

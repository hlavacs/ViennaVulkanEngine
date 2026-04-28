param(
    [ValidateSet("Mac", "Windows", "Linux")]
    [string]$Platform = "Windows",

    [ValidateSet("Debug", "Release")]
    [string]$Variant = "Debug",

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
        $buildName = if ($Variant -eq "Debug") { "debug-windows" } else { "release-windows" }
        $buildDir = Join-Path $repoRoot "build\$buildName"
        $ctestArgs = @(
            "--test-dir", $buildDir,
            "-C", $Variant,
            "--output-on-failure"
        )
    }
    "Mac" {
        $buildDir = Join-Path $repoRoot "build\vscode-$Variant"
        $manifest = Join-Path $buildDir "vulkan\icd.d\MoltenVK_icd.json"
        if (Test-Path $manifest) {
            $env:VK_ICD_FILENAMES = $manifest
        }
        $ctestArgs = @(
            "--test-dir", $buildDir,
            "--output-on-failure"
        )
    }
    "Linux" {
        $buildName = if ($Variant -eq "Debug") { "debug-linux" } else { "release-linux" }
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

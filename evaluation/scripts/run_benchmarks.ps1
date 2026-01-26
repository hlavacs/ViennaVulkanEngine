# run_benchmarks.ps1
# Automated benchmark script for gaussian splatting IBL evaluation
# Tests 48 configurations: 4 scenes × 3 resolutions × 2 filter modes × 2 intervals
#
# AI-generated code for ViennaVulkanEngine gaussian splatting integration

param(
    [string]$BuildDir = "build",
    [int]$DurationSeconds = 30,
    [string]$ResultsDir = "evaluation\results"
)

$DemoExeName = "gaussian-splatting-cubemap-lighting-demo.exe"
$HeaderFile = "include\VERendererGaussian.h"
$DemoFile = "examples\gaussian-splatting-cubemap-lighting-demo\gaussian-splatting-cubemap-lighting-demo.cpp"

# Scene configurations: gaussian + mesh combinations
$Scenes = @(
    @{
        Name = "Interior_Sphere"
        GaussianPath = "assets/gaussian-splatting/InteriorDesign.ply"
        MeshPath = "assets/standard/sphere.obj"
        MeshScale = "0.03f"
    },
    @{
        Name = "Interior_Viking"
        GaussianPath = "assets/gaussian-splatting/InteriorDesign.ply"
        MeshPath = "assets/viking_room/viking_room.obj"
        MeshScale = "1.0f"
    },
    @{
        Name = "Bicycle_Sphere"
        GaussianPath = "assets/gaussian-splatting/bicycle/point_cloud/iteration_30000/point_cloud.ply"
        MeshPath = "assets/standard/sphere.obj"
        MeshScale = "0.03f"
    },
    @{
        Name = "Bicycle_Viking"
        GaussianPath = "assets/gaussian-splatting/bicycle/point_cloud/iteration_30000/point_cloud.ply"
        MeshPath = "assets/viking_room/viking_room.obj"
        MeshScale = "1.0f"
    }
)

# Rendering configurations: resolution × filter × interval
$RenderConfigs = @(
    @{ Name = "Low_Box_1";      CubemapRes = 256;  IrradianceRes = 16;  UseConvolution = $false; Interval = 1  },
    @{ Name = "Low_Box_10";     CubemapRes = 256;  IrradianceRes = 16;  UseConvolution = $false; Interval = 10 },
    @{ Name = "Low_Conv_1";     CubemapRes = 256;  IrradianceRes = 16;  UseConvolution = $true;  Interval = 1  },
    @{ Name = "Low_Conv_10";    CubemapRes = 256;  IrradianceRes = 16;  UseConvolution = $true;  Interval = 10 },
    @{ Name = "Medium_Box_1";   CubemapRes = 512;  IrradianceRes = 32;  UseConvolution = $false; Interval = 1  },
    @{ Name = "Medium_Box_10";  CubemapRes = 512;  IrradianceRes = 32;  UseConvolution = $false; Interval = 10 },
    @{ Name = "Medium_Conv_1";  CubemapRes = 512;  IrradianceRes = 32;  UseConvolution = $true;  Interval = 1  },
    @{ Name = "Medium_Conv_10"; CubemapRes = 512;  IrradianceRes = 32;  UseConvolution = $true;  Interval = 10 },
    @{ Name = "High_Box_1";     CubemapRes = 1024; IrradianceRes = 64;  UseConvolution = $false; Interval = 1  },
    @{ Name = "High_Box_10";    CubemapRes = 1024; IrradianceRes = 64;  UseConvolution = $false; Interval = 10 },
    @{ Name = "High_Conv_1";    CubemapRes = 1024; IrradianceRes = 64;  UseConvolution = $true;  Interval = 1  },
    @{ Name = "High_Conv_10";   CubemapRes = 1024; IrradianceRes = 64;  UseConvolution = $true;  Interval = 10 }
)

# Build full configuration matrix: 4 scenes × 12 render configs = 48 total
$Configurations = @()
foreach ($scene in $Scenes) {
    foreach ($render in $RenderConfigs) {
        $Configurations += @{
            Name           = "$($scene.Name)_$($render.Name)"
            GaussianPath   = $scene.GaussianPath
            MeshPath       = $scene.MeshPath
            MeshScale      = $scene.MeshScale
            CubemapRes     = $render.CubemapRes
            IrradianceRes  = $render.IrradianceRes
            UseConvolution = $render.UseConvolution
            Interval       = $render.Interval
        }
    }
}

$ScriptRoot = $PSScriptRoot
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $ScriptRoot)
$ResultsPath = Join-Path $ProjectRoot $ResultsDir

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Gaussian IBL Benchmark Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Project root: $ProjectRoot"
Write-Host "Duration per config: $DurationSeconds seconds"
Write-Host "Configurations to test: $($Configurations.Count)"
Write-Host ""

# Create results directory
if (-not (Test-Path $ResultsPath)) {
    New-Item -ItemType Directory -Path $ResultsPath -Force | Out-Null
    Write-Host "Created results directory: $ResultsPath"
}

# Modify header and demo source files
function Set-Configuration {
    param($Config)

    $HeaderPath = Join-Path $ProjectRoot $HeaderFile
    $Content = Get-Content $HeaderPath -Raw
    $Content = $Content -replace 'm_cubemapResolution = \d+', "m_cubemapResolution = $($Config.CubemapRes)"
    $Content = $Content -replace 'm_irradianceResolution = \d+', "m_irradianceResolution = $($Config.IrradianceRes)"
    $ConvValue = if ($Config.UseConvolution) { "true" } else { "false" }
    $Content = $Content -replace 'm_useConvolutionFilter = (true|false)', "m_useConvolutionFilter = $ConvValue"
    Set-Content $HeaderPath $Content

    $DemoPath = Join-Path $ProjectRoot $DemoFile
    $DemoContent = Get-Content $DemoPath -Raw
    $DemoContent = $DemoContent -replace 'int iblUpdateInterval = \d+;', "int iblUpdateInterval = $($Config.Interval);"
    $DemoContent = $DemoContent -replace 'const char\* gaussianPath = "[^"]+";', "const char* gaussianPath = ""$($Config.GaussianPath)"";"
    $DemoContent = $DemoContent -replace 'const char\* meshPath = "[^"]+";', "const char* meshPath = ""$($Config.MeshPath)"";"
    $ScaleVal = $Config.MeshScale
    $DemoContent = $DemoContent -replace 'glm::vec3 meshScale = glm::vec3\([^)]+\);', "glm::vec3 meshScale = glm::vec3($ScaleVal, $ScaleVal, $ScaleVal);"
    Set-Content $DemoPath $DemoContent

    Write-Host "  Gaussian: $($Config.GaussianPath | Split-Path -Leaf)" -ForegroundColor Yellow
    Write-Host "  Mesh: $($Config.MeshPath | Split-Path -Leaf) (scale: $ScaleVal)" -ForegroundColor Yellow
    Write-Host "  Cubemap: $($Config.CubemapRes), Irradiance: $($Config.IrradianceRes), Conv: $ConvValue, Interval: $($Config.Interval)" -ForegroundColor Yellow
}

# Build project
function Build-Project {
    param($ConfigName)
    Write-Host "  Building..." -ForegroundColor Gray
    $BuildPath = Join-Path $ProjectRoot $BuildDir
    $BuildOutput = cmake --build $BuildPath --config Release 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  BUILD FAILED!" -ForegroundColor Red
        $LogFile = Join-Path $ResultsPath "$ConfigName.build_error.log"
        $BuildOutput | Out-File $LogFile
        Write-Host "  Build log: $LogFile" -ForegroundColor Red
        # Show last 10 lines of error
        $errorLines = $BuildOutput | Select-Object -Last 10
        foreach ($line in $errorLines) {
            Write-Host "    $line" -ForegroundColor DarkRed
        }
        return $false
    }
    Write-Host "  Build successful" -ForegroundColor Green
    return $true
}

# Run demo and capture output
function Run-Demo {
    param($ConfigName, $Duration)

    $ExePath = Get-ChildItem -Path (Join-Path $ProjectRoot $BuildDir) -Recurse -Filter $DemoExeName |
               Where-Object { $_.DirectoryName -match "Release" } |
               Select-Object -First 1

    if (-not $ExePath) {
        Write-Host "  Demo executable not found: $DemoExeName" -ForegroundColor Red
        return $null
    }

    Write-Host "  Running: $($ExePath.Name)" -ForegroundColor Gray
    Write-Host "  Working dir: $ProjectRoot" -ForegroundColor Gray
    $OutputFile = Join-Path $ResultsPath "$ConfigName.log"

    # Run from project root (shaders/assets are relative to project root)
    # Use cmd /c to capture console output, redirect stderr to stdout
    $cmdArgs = "/c `"cd /d `"$ProjectRoot`" && `"$($ExePath.FullName)`" 2>&1`""

    $Process = Start-Process -FilePath "cmd.exe" `
                            -ArgumentList $cmdArgs `
                            -PassThru `
                            -RedirectStandardOutput $OutputFile `
                            -NoNewWindow

    Write-Host "  Waiting $Duration seconds..." -ForegroundColor Gray

    $Waited = 0
    while ($Waited -lt $Duration -and -not $Process.HasExited) {
        Start-Sleep -Seconds 5
        $Waited += 5
        Write-Host "    $Waited / $Duration sec" -ForegroundColor DarkGray
    }

    if (-not $Process.HasExited) {
        # Kill the demo process and all child processes
        try {
            # Get all child processes of cmd.exe and kill them
            $children = Get-CimInstance Win32_Process | Where-Object { $_.ParentProcessId -eq $Process.Id }
            foreach ($child in $children) {
                Write-Host "  Killing child process: $($child.Name) (PID: $($child.ProcessId))" -ForegroundColor DarkGray
                Stop-Process -Id $child.ProcessId -Force -ErrorAction SilentlyContinue
            }
            # Then kill cmd.exe
            $Process.Kill()
            $Process.WaitForExit(5000)
        } catch {}
        Write-Host "  Terminated after $Duration seconds" -ForegroundColor Gray
    } else {
        Write-Host "  Process exited early (code: $($Process.ExitCode))" -ForegroundColor Yellow
    }

    # Always show output file path
    Write-Host "  Output saved: $OutputFile" -ForegroundColor Gray

    if (Test-Path $OutputFile) {
        $content = Get-Content $OutputFile -Raw
        # Show last few lines for debugging
        $lines = $content -split "`n" | Select-Object -Last 5
        Write-Host "  Last output:" -ForegroundColor Gray
        foreach ($line in $lines) {
            Write-Host "    $line" -ForegroundColor DarkGray
        }
        return $content
    }
    return $null
}

# Parse timing output from demo console
# Expected format:
#   FPS: 60.0 (16.666 ms/frame)
#   Rank: 0.123 ms, Sort: 0.456 ms, Projection: 0.789 ms, Render: 0.012 ms
#   Cubemap: 0.123 ms, Irradiance: 0.456 ms
function Parse-Timings {
    param($Output)

    $Timings = @{
        FPS = "N/A"
        RankTime = "N/A"
        SortTime = "N/A"
        ProjectionTime = "N/A"
        RenderTime = "N/A"
        CubemapTime = "N/A"
        IrradianceTime = "N/A"
    }

    if (-not $Output) { return $Timings }

    # Get last occurrence of each metric (most recent reading)
    $lines = $Output -split "`n"
    foreach ($line in $lines) {
        if ($line -match "^FPS:\s*([\d.]+)") { $Timings.FPS = $Matches[1] }
        if ($line -match "Rank:\s*([\d.]+)\s*ms") { $Timings.RankTime = $Matches[1] }
        if ($line -match "Sort:\s*([\d.]+)\s*ms") { $Timings.SortTime = $Matches[1] }
        if ($line -match "Projection:\s*([\d.]+)\s*ms") { $Timings.ProjectionTime = $Matches[1] }
        if ($line -match "Render:\s*([\d.]+)\s*ms") { $Timings.RenderTime = $Matches[1] }
        if ($line -match "Cubemap:\s*([\d.]+)\s*ms") { $Timings.CubemapTime = $Matches[1] }
        if ($line -match "Irradiance:\s*([\d.]+)\s*ms") { $Timings.IrradianceTime = $Matches[1] }
    }

    return $Timings
}

# Main benchmark loop
$Results = @()
$Timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"

Write-Host ""
Write-Host "Starting benchmark at $Timestamp" -ForegroundColor Cyan
Write-Host ""

foreach ($Config in $Configurations) {
    Write-Host "----------------------------------------" -ForegroundColor White
    Write-Host "[$($Configurations.IndexOf($Config) + 1)/$($Configurations.Count)] $($Config.Name)" -ForegroundColor Cyan
    Write-Host "----------------------------------------" -ForegroundColor White

    Set-Configuration -Config $Config

    if (-not (Build-Project -ConfigName $Config.Name)) {
        Write-Host "  Skipping due to build failure" -ForegroundColor Red
        continue
    }

    $Output = Run-Demo -ConfigName $Config.Name -Duration $DurationSeconds
    $Timings = Parse-Timings -Output $Output

    $GaussianName = if ($Config.GaussianPath -match "InteriorDesign") { "Interior" } else { "Bicycle" }
    $MeshName = if ($Config.MeshPath -match "sphere") { "Sphere" } else { "Viking" }

    $Results += [PSCustomObject]@{
        Configuration  = $Config.Name
        Gaussian       = $GaussianName
        Mesh           = $MeshName
        CubemapRes     = $Config.CubemapRes
        IrradianceRes  = $Config.IrradianceRes
        FilterMode     = if ($Config.UseConvolution) { "Conv" } else { "Box" }
        IBLInterval    = $Config.Interval
        FPS            = $Timings.FPS
        RankMs         = $Timings.RankTime
        SortMs         = $Timings.SortTime
        ProjectionMs   = $Timings.ProjectionTime
        RenderMs       = $Timings.RenderTime
        CubemapMs      = $Timings.CubemapTime
        IrradianceMs   = $Timings.IrradianceTime
    }

    Write-Host "  Result: FPS=$($Timings.FPS), Cubemap=$($Timings.CubemapTime)ms, Irradiance=$($Timings.IrradianceTime)ms" -ForegroundColor Green
    Write-Host ""
}

# Export results
$CsvPath = Join-Path $ResultsPath "benchmark_$Timestamp.csv"
$Results | Export-Csv -Path $CsvPath -NoTypeInformation

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Benchmark complete!" -ForegroundColor Cyan
Write-Host "Results: $CsvPath" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
$Results | Format-Table -AutoSize

# Restore defaults
Write-Host "Restoring defaults..." -ForegroundColor Gray
Set-Configuration -Config @{
    GaussianPath = "assets/gaussian-splatting/InteriorDesign.ply"
    MeshPath = "assets/standard/sphere.obj"
    MeshScale = "0.03f"
    CubemapRes = 512
    IrradianceRes = 32
    UseConvolution = $true
    Interval = 1
}

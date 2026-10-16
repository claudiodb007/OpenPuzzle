param([string]$ReportRoot = '')
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$baseCommit = '3eb164164aede1991ad258b8e7306191684197a6'
$transcribing = $false
$report = $null
function Invoke-Native {
    param([string]$Tool, [string[]]$ToolArguments)
    & $Tool @ToolArguments | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Native command failed ($LASTEXITCODE): $Tool" }
}
function Write-Utf8 {
    param([string]$Path, [string]$Text)
    [System.IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.UTF8Encoding($false)))
}
function Get-CMakeVersion {
    param([object]$Capabilities)
    $invalid = 'CMake returned invalid version components. Send validation.log and cmake-capabilities.json.'
    if ($null -eq $Capabilities) { throw $invalid }
    $versionProperty = $Capabilities.PSObject.Properties['version']
    if ($null -eq $versionProperty -or $null -eq $versionProperty.Value) { throw $invalid }
    $parts = @()
    foreach ($name in @('major', 'minor', 'patch')) {
        $property = $versionProperty.Value.PSObject.Properties[$name]
        if ($null -eq $property) { throw $invalid }
        $value = $property.Value
        if ($value -isnot [int] -and $value -isnot [long]) { throw $invalid }
        if ($value -lt 0 -or $value -gt [int]::MaxValue) { throw $invalid }
        $parts += [int]$value
    }
    # CMake's display string may contain a vendor or release-candidate suffix.
    # Compare the documented numeric components instead of casting that string.
    return [version]($parts -join '.')
}
function Get-VisualStudioInstallation {
    param([string]$Json)
    $missing = 'No C++ x64 toolchain was found. Open Visual Studio Installer, choose Modify, and install Desktop development with C++, MSVC x64/x86, Windows SDK and C++ CMake tools for Windows; then rerun.'
    if ([string]::IsNullOrWhiteSpace($Json)) { throw $missing }
    try { $parsed = ConvertFrom-Json -InputObject $Json -ErrorAction Stop }
    catch { throw 'vswhere returned invalid JSON. Repair or update Visual Studio Installer, then rerun.' }
    # Windows PowerShell 5.1 can emit an empty JSON array as one pipeline object.
    # Enumerate the parsed value explicitly before counting/selecting instances.
    $instances = @()
    foreach ($item in $parsed) {
        if ($null -ne $item) { $instances += $item }
    }
    if ($instances.Count -eq 0) { throw $missing }
    if ($instances.Count -ne 1) { throw 'vswhere returned multiple instances for the latest toolchain. Send validation.log and vswhere-selection.json.' }
    $candidate = $instances[0]
    $pathProperty = $candidate.PSObject.Properties['installationPath']
    $versionProperty = $candidate.PSObject.Properties['installationVersion']
    if ($null -eq $pathProperty -or $null -eq $versionProperty -or
        [string]::IsNullOrWhiteSpace([string]$pathProperty.Value)) {
        throw 'vswhere did not return a valid installation path and version. Send validation.log and vswhere-selection.json.'
    }
    $parsedVersion = $null
    if (-not [version]::TryParse([string]$versionProperty.Value, [ref]$parsedVersion)) {
        throw 'vswhere returned an invalid installation version. Send validation.log and vswhere-selection.json.'
    }
    return [pscustomobject]@{
        installationPath = [string]$pathProperty.Value
        installationVersion = $parsedVersion
    }
}
try {
    if ([Environment]::OSVersion.Platform -ne [System.PlatformID]::Win32NT -or -not [Environment]::Is64BitOperatingSystem) {
        throw 'Run this script on native Windows x64, not WSL.'
    }
    if (-not $ReportRoot) { $ReportRoot = Join-Path $env:USERPROFILE 'op' }
    $null = New-Item -ItemType Directory -Force -Path $ReportRoot
    $report = Join-Path $ReportRoot ('platform-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
    $null = New-Item -ItemType Directory -Path $report
    Start-Transcript -Path (Join-Path $report 'validation.log') | Out-Null
    $transcribing = $true
    Write-Host '===== WINDOWS TOOLCHAIN DETECTION ====='
    Write-Host "PowerShell: $($PSVersionTable.PSVersion)"
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'Install Visual Studio Build Tools with Desktop development with C++, Windows SDK and CMake tools for Windows; then rerun.'
    }
    $installationJson = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | Out-String
    if ($LASTEXITCODE -ne 0) { throw 'vswhere failed.' }
    Write-Utf8 -Path (Join-Path $report 'vswhere-selection.json') -Text $installationJson
    $installation = Get-VisualStudioInstallation -Json $installationJson
    if (-not (Test-Path -LiteralPath $installation.installationPath -PathType Container)) {
        throw 'The Visual Studio installation directory does not exist. Repair the installation, then rerun.'
    }
    $cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($cmakeCommand) { $cmake = $cmakeCommand.Source }
    else { $cmake = Join-Path $installation.installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' }
    if (-not (Test-Path -LiteralPath $cmake)) { throw 'CMake missing: add C++ CMake tools for Windows in Visual Studio Installer.' }
    $ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
    if (-not (Test-Path -LiteralPath $ctest)) { throw 'CTest missing beside CMake.' }
    $capabilitiesJson = & $cmake -E capabilities | Out-String
    if ($LASTEXITCODE -ne 0) { throw 'Cannot query CMake capabilities.' }
    Write-Utf8 -Path (Join-Path $report 'cmake-capabilities.json') -Text $capabilitiesJson
    try { $capabilities = ConvertFrom-Json -InputObject $capabilitiesJson -ErrorAction Stop }
    catch { throw 'CMake returned invalid capabilities JSON. Send validation.log and cmake-capabilities.json.' }
    $cmakeVersion = Get-CMakeVersion -Capabilities $capabilities
    Write-Host "Visual Studio: $($installation.installationVersion)"
    Write-Host "CMake: $cmake | Numeric version: $cmakeVersion"
    if ($cmakeVersion -lt [version]'3.22.0') { throw 'CMake 3.22 or later is required.' }
    $major = ([version]$installation.installationVersion).Major
    $generators = @($capabilities.generators | Where-Object { $_.name -match "^Visual Studio $major " })
    if ($generators.Count -ne 1) { throw 'CMake does not support this installed Visual Studio version. Update CMake or select a supported C++ toolchain.' }
    $generator = $generators[0].name
    Write-Host '===== WINDOWS NATIVE UI ====='
    Write-Host "Linux source base: $baseCommit"
    Write-Host "Generator: $generator | Architecture: x64"
    $boost = Join-Path (Split-Path -Parent $project) 'third-party\include'
    if (-not (Test-Path -LiteralPath (Join-Path $boost 'boost\property_tree\json_parser.hpp'))) { throw 'Bundled Boost headers are missing; extract the complete Windows telemetry ZIP.' }
    $build = Join-Path $report 'build'
    Invoke-Native -Tool $cmake -ToolArguments @('-S', $project, '-B', $build, '-G', $generator, '-A', 'x64', "-DCMAKE_GENERATOR_INSTANCE=$($installation.installationPath)", '-DOPENPUZZLE_BUILD_PLATFORM_TESTS_ONLY=ON', "-DOPENPUZZLE_PLATFORM_BOOST_INCLUDE_DIR=$boost", "-DOPENPUZZLE_NATIVE_UI_LOGO=$(Join-Path (Split-Path -Parent $project) 'native-ui-assets\openpuzzle-logo-header.png')")
    Invoke-Native -Tool $cmake -ToolArguments @('--build', $build, '--config', 'Release', '--parallel', '4')
    $registrationJson = & $ctest --test-dir $build -C Release --show-only=json-v1 | Out-String
    if ($LASTEXITCODE -ne 0) { throw 'CTest registration failed.' }
    Write-Utf8 -Path (Join-Path $report 'registered-tests.json') -Text $registrationJson
    $registration = $registrationJson | ConvertFrom-Json
    $expected = @('HttpRangeClientTests', 'HttpRequestPolicyTests', 'WindowsHttpTransportTests', 'PortableStorageTests', 'WindowsEngineFormatTests', 'WindowsEngineLifecycleTests', 'WindowsProcessIdentityTests', 'WindowsProcessRunnerTests', 'WindowsRuntimeControlTests', 'WindowsRuntimeRecordTests', 'WindowsStorageIdentityTests', 'WindowsExecutionStateTests', 'WindowsExecutionSyncTests', 'ClientRuntimeLoopTests', 'PortableSolutionExporterTests', 'WindowsRuntimeSessionTests', 'RunCommandOptionsTests', 'RunAssignmentPlanTests', 'WindowsRunSessionTests', 'WindowsCliApplicationTests', 'WindowsGpuDiscoveryTests', 'WindowsEngineRangeTests', 'WindowsEngineBindingTests', 'WindowsKeyHuntProtocolTests', 'WindowsRunProfileTests', 'KeyHuntSolutionRecordTests', 'WindowsNativeAssignmentTests', 'WindowsAssignmentRecoveryTests', 'WindowsTelemetryModelTests', 'WindowsGpuTelemetryTests', 'WindowsThermalCliTests', 'WindowsThermalRuntimeTests', 'WindowsGpuProfileBindingTests', 'WindowsProfileBindingCliTests', 'WindowsUiModelTests', 'WindowsUiProcessTests', 'WindowsUiWindowTests', 'WindowsUiServerTests', 'WindowsUiLanguageTests')
    $registered = @($registration.tests | ForEach-Object { $_.name } | Sort-Object)
    if ($registered.Count -ne 39 -or @(Compare-Object $expected $registered).Count -ne 0) {
        throw 'CTest must register exactly the thirty-eight Windows platform and hardware suites.'
    }
    $junit = Join-Path $report 'ctest-results.xml'
    Invoke-Native -Tool $ctest -ToolArguments @('--test-dir', $build, '-C', 'Release', '--output-on-failure', '--no-tests=error', '--output-junit', $junit)
    [xml]$result = Get-Content -LiteralPath $junit -Raw
    $cases = @($result.SelectNodes('//testcase'))
    $names = @($cases | ForEach-Object { $_.GetAttribute('name') } | Sort-Object)
    if ($cases.Count -ne 39 -or @(Compare-Object $expected $names).Count -ne 0) {
        throw 'JUnit does not prove execution of all thirty-eight registered Windows suites.'
    }
    foreach ($case in $cases) {
        if (@($case.SelectNodes('failure|error|skipped')).Count -ne 0 -or $case.GetAttribute('status') -notin @('', 'run', 'pass', 'passed')) {
            throw 'JUnit contains a failed or skipped test.'
        }
    }
    $sourceHashes = @(Get-ChildItem -LiteralPath $project -File -Recurse | Where-Object { $_.Extension -in @('.cpp','.hpp','.ps1','.txt') } | Sort-Object FullName | ForEach-Object {
        $relative = $_.FullName.Substring($project.Length).TrimStart('\')
        (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $relative
    })
    Write-Utf8 -Path (Join-Path $report 'SOURCE_SHA256SUMS.txt') -Text ($sourceHashes -join "`n")
    Write-Host '===== LOCAL CPU AND GPU INVENTORY ====='
    $cli = Join-Path $build 'platform\Release\openpuzzle.exe'
    $inventory = & $cli devices | Out-String
    $inventoryExit = $LASTEXITCODE
    Write-Utf8 -Path (Join-Path $report 'hardware-inventory.txt') -Text $inventory
    Write-Host $inventory
    if ($inventoryExit -notin @(0,2)) { throw "Hardware inventory command failed ($inventoryExit)." }
    $summary = @(
        'Windows UI benchmark and scrolling: native fixture validation passed',
        "Linux source base: $baseCommit",
        "Generator: $generator",
        'Architecture: x64',
        'Native Windows tests: 39/39 passed',
        "Real hardware diagnostic exit: $inventoryExit (0: enumeration completed; 2: hardware query unavailable)",
        ('Windows CLI executable: ' + (Join-Path $build 'platform\Release\openpuzzle.exe')),
        ('Windows UI executable: ' + (Join-Path $build 'platform\Release\openpuzzle-ui.exe')),
        'Native Windows panel: four languages, Benchmark, Safe Stop and buffered scrolling; visual review pending',
        'Full Windows application: not yet ported',
        'Only local processes, read-only driver enumeration and a loopback HTTP fixture were used; no production assignments requested.',
        "Report: $report"
    ) -join "`n"
    Write-Utf8 -Path (Join-Path $report 'SUMMARY.txt') -Text $summary
    Write-Host "`n===== WINDOWS PLATFORM VALIDATED ====="
    Write-Host $summary
    Stop-Transcript | Out-Null
    $transcribing = $false
    exit 0
} catch {
    Write-Host "`nValidation interrupted: $($_.Exception.Message)"
    if ($report) { Write-Host "Report: $report" }
    if ($transcribing) { Stop-Transcript | Out-Null }
    exit 1
}

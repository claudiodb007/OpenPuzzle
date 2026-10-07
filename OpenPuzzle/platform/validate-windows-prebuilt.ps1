param(
    [string]$BinaryDirectory = '',
    [string]$ReportRoot = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$report = $null
try {
    if ([Environment]::OSVersion.Platform -ne [System.PlatformID]::Win32NT -or -not [Environment]::Is64BitOperatingSystem) {
        throw 'Run on native Windows x64, not WSL.'
    }
    if (-not $BinaryDirectory) { $BinaryDirectory = Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'bin\windows-x64' }
    if (-not $ReportRoot) { $ReportRoot = Join-Path $env:USERPROFILE 'OpenPuzzle\relatorios\windows-platform' }
    $manifest = Get-Content -LiteralPath (Join-Path $BinaryDirectory 'BUILD_MANIFEST.json') -Raw | ConvertFrom-Json
    if ($manifest.source_base -ne '129ada590c4e7591f24251aa4c7fc89e7bfb21db' -or $manifest.architecture -ne 'x86_64') {
        throw 'Unexpected binary build identity.'
    }
    $expected = @('WindowsProcessFixture.exe','WindowsProcessIdentityTests.exe','WindowsProcessRunnerTests.exe')
    $fileNames = @($manifest.files | ForEach-Object { $_.name } | Sort-Object)
    if ($fileNames.Count -ne 3 -or @(Compare-Object $expected $fileNames).Count -ne 0) { throw 'Unexpected binary manifest files.' }
    foreach ($file in $manifest.files) {
        $path = Join-Path $BinaryDirectory $file.name
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.sha256) { throw "Binary checksum mismatch: $($file.name)" }
    }
    $null = New-Item -ItemType Directory -Force -Path $ReportRoot
    $report = Join-Path $ReportRoot ('native-prebuilt-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
    $null = New-Item -ItemType Directory -Path $report
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    $document = New-Object System.Xml.XmlDocument
    $suite = $document.CreateElement('testsuite')
    $suite.SetAttribute('name','WindowsProcessFoundation')
    $suite.SetAttribute('tests','2')
    $null = $document.AppendChild($suite)
    $failed = 0
    $fixture = (Resolve-Path -LiteralPath (Join-Path $BinaryDirectory 'WindowsProcessFixture.exe')).Path
    foreach ($name in @('WindowsProcessRunnerTests','WindowsProcessIdentityTests')) {
        Write-Host "===== $name ====="
        $start = New-Object System.Diagnostics.ProcessStartInfo
        $start.FileName = (Resolve-Path -LiteralPath (Join-Path $BinaryDirectory ($name + '.exe'))).Path
        $start.Arguments = '"' + $fixture + '"'
        $start.UseShellExecute = $false
        $start.CreateNoWindow = $true
        $start.RedirectStandardOutput = $true
        $start.RedirectStandardError = $true
        $process = New-Object System.Diagnostics.Process
        $process.StartInfo = $start
        $watch = [Diagnostics.Stopwatch]::StartNew()
        $exitCode = -1
        $timedOut = $false
        try {
            if (-not $process.Start()) { throw "Cannot start $name" }
            $stdout = $process.StandardOutput.ReadToEndAsync()
            $stderr = $process.StandardError.ReadToEndAsync()
            if (-not $process.WaitForExit(45000)) {
                $timedOut = $true
                $process.Kill()
                $null = $process.WaitForExit(5000)
            } else { $exitCode = $process.ExitCode }
            $outputComplete = $stdout.Wait(5000) -and $stderr.Wait(5000)
            if ($outputComplete) { $log = $stdout.Result + $stderr.Result }
            else { $log = 'Output collection did not finish.'; $exitCode = -1 }
            if ($timedOut) { $log += "`nTest exceeded 45 seconds." }
        } finally { $process.Dispose(); $watch.Stop() }
        [System.IO.File]::WriteAllText((Join-Path $report ($name + '.log')), $log, $utf8)
        Write-Host $log
        $case = $document.CreateElement('testcase')
        $case.SetAttribute('name',$name)
        $case.SetAttribute('status','run')
        $case.SetAttribute('time',$watch.Elapsed.TotalSeconds.ToString('F3',[Globalization.CultureInfo]::InvariantCulture))
        if ($exitCode -ne 0) {
            $failed++
            $failure = $document.CreateElement('failure')
            $failure.SetAttribute('message',"Exit code $exitCode")
            $failure.InnerText = $log
            $null = $case.AppendChild($failure)
        }
        $null = $suite.AppendChild($case)
    }
    $suite.SetAttribute('failures',[string]$failed)
    $document.Save((Join-Path $report 'test-results.xml'))
    Copy-Item -LiteralPath (Join-Path $BinaryDirectory 'BUILD_MANIFEST.json') -Destination $report
    if ($failed -ne 0) { throw "$failed Windows test suite(s) failed. Send test-results.xml and the two logs." }
    $summary = @(
        'Windows process foundation: native execution validation passed',
        "Linux source base: $($manifest.source_base)",
        "Port patch SHA-256: $($manifest.patch_sha256)",
        "Compiler: $($manifest.compiler) (cross-build)",
        'Architecture: x64',
        'Native Windows executable tests: 2/2 passed',
        'MSVC build validation: pending; use validate-windows.ps1',
        'Full Windows application: not yet ported',
        'Only local fixture processes were launched; no assignments requested.',
        "Report: $report"
    ) -join "`n"
    [System.IO.File]::WriteAllText((Join-Path $report 'SUMMARY.txt'),$summary,$utf8)
    Write-Host "`n===== WINDOWS PLATFORM VALIDATED ====="
    Write-Host $summary
    exit 0
} catch {
    Write-Host "Validation interrupted: $($_.Exception.Message)"
    if ($report) { Write-Host "Report: $report" }
    exit 1
}

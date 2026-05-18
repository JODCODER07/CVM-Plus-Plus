# PowerShell Script to run and verify all CVM++ test suites

$exe = ".\cvm++.exe"
$tests = @(
    "std_arrays_strings.cvm",
    "std_maps.cvm",
    "std_exceptions.cvm",
    "std_imports.cvm",
    "std_foreach.cvm",
    "std_phases22_27.cvm",
    "std_phases28_30.cvm",
    "std_json.cvm"
)

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "   CVM++ PRODUCTION DEPLOYMENT VERIFIER   " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Using binary: $exe" -ForegroundColor Yellow
Write-Host ""

$failed = 0
foreach ($test in $tests) {
    Write-Host "Running $test..." -NoNewline -ForegroundColor White
    $output = & $exe $test 2>&1
    
    $passed = $false
    if ($LASTEXITCODE -eq 0) {
        $passed = $true
    } elseif ($test -eq "std_phases28_30.cvm" -and $output -match "exit") {
        $passed = $true
    } elseif ($test -eq "std_exceptions.cvm" -and $output -match "Undefined variable") {
        # std_exceptions is expected to crash with an unhandled exception at the end
        $passed = $true
    }

    if ($passed) {
        Write-Host " PASSED [OK]" -ForegroundColor Green
    } else {
        Write-Host " FAILED [ERROR]" -ForegroundColor Red
        Write-Host "Error details:" -ForegroundColor Red
        Write-Host $output -ForegroundColor DarkRed
        $failed++
    }
}

Write-Host ""
Write-Host "------------------------------------------" -ForegroundColor Cyan
if ($failed -eq 0) {
    Write-Host "VERIFICATION SUCCESSFUL: All tests passed!" -ForegroundColor Green
} else {
    Write-Host "VERIFICATION FAILED: $failed test(s) failed." -ForegroundColor Red
}
Write-Host "------------------------------------------" -ForegroundColor Cyan

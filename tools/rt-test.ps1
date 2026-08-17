<#
    rt-test.ps1 — прогон тестового набора с фиксированным зерном и сверка с эталоном.

    План требует, чтобы каждая стадия завершалась только тогда, когда некарантинная
    часть тестов совпадает с эталоном S0b. Апстрим регистрирует тесты в CTest с
    `--rng-seed time`, то есть со случайным зерном — сравнивать такие прогоны между
    собой нельзя. Поэтому здесь фиксированное зерно и лексический порядок.

    Usage:
        pwsh tools/rt-test.ps1 -Baseline     # записать эталон (делается один раз, на S0b)
        pwsh tools/rt-test.ps1               # прогнать и сравнить с эталоном
        pwsh tools/rt-test.ps1 -Filter "[melee]"
#>
param(
    [string]$Config   = "RelWithDebInfo",
    [string]$Seed     = "1",
    [string]$Filter   = "",
    [switch]$Baseline
)

# Per-test assertion counts are written alongside the XML. Catch2 reports assertion
# counts per section and per run but not per test case, so when a run's total moves
# there is otherwise no way to find out which test moved it - and the count is a
# measure of how much work the game did, so it moves when behaviour changes even
# though every test still passes. Compare two of these with Compare-AssertionStats.

$ErrorActionPreference = "Stop"

$RepoRoot     = Split-Path -Parent $PSScriptRoot
$BuildDir     = Join-Path $RepoRoot "out\build\rt-msvc"
$TestExe      = Join-Path $BuildDir "tests\$Config\cata_test-tiles.exe"
$ResultsDir   = Join-Path $RepoRoot "rt-test-results"
$BaselineXml  = Join-Path $ResultsDir "baseline-s0b.xml"
$BaselineTxt  = Join-Path $ResultsDir "baseline-s0b.txt"

if (-not (Test-Path $TestExe)) {
    throw "Test executable not found: $TestExe`nBuild it first: tools\rt-build.ps1 -Target cata_test-tiles"
}
New-Item -ItemType Directory -Force -Path $ResultsDir | Out-Null

$stamp    = Get-Date -Format "yyyyMMdd-HHmmss"
$outXml   = if ($Baseline) { $BaselineXml } else { Join-Path $ResultsDir "run-$stamp.xml" }
$outTxt   = if ($Baseline) { $BaselineTxt } else { Join-Path $ResultsDir "run-$stamp.txt" }
$outStats = [System.IO.Path]::ChangeExtension($outXml, ".stats.tsv")
$userDir  = Join-Path $env:TEMP "cdda-rt-test-user\"

# Wiped before the run, not after: the game leaves its test world behind on purpose
# so a failure can be inspected, and that is worth keeping - but only until the next
# run. A world that survives into the next run is state the tests did not create,
# and tests that generate or load map chunks then do a different amount of work
# depending on what an earlier run happened to leave lying around.
if (Test-Path $userDir) { Remove-Item -Recurse -Force $userDir }

# Catch2 must run with the repo root as the working directory so the game finds data/.
Push-Location $RepoRoot
try {
    # Catch2 v2.13.7: one reporter per run, `--reporter <name> --out <file>`.
    # The `reporter::out=` form is Catch2 v3 syntax and is rejected here.
    $testArgs = @(
        "--rng-seed", $Seed,
        "--order", "lex",
        "--user-dir=$userDir",
        "--rt-assertion-stats", $outStats,
        "--reporter", "xml",
        "--out", $outXml
    )
    if ($Filter) { $testArgs += $Filter }

    Write-Host "exe:      $TestExe"
    Write-Host "seed:     $Seed (fixed)"
    Write-Host "xml:      $outXml"
    Write-Host ""

    & $TestExe @testArgs
    $testExit = $LASTEXITCODE
} finally {
    Pop-Location
}

# A run that never produced a report is a harness failure, not a test result.
# Do not let it masquerade as a clean baseline.
if (-not (Test-Path $outXml)) {
    throw "No report was written to $outXml (test process exited $testExit). The run did not happen."
}

# ------------------------------------------------------------- comparison ----
function Get-TestCases([string]$xmlPath) {
    [xml]$doc = Get-Content $xmlPath
    $doc.SelectNodes("//TestCase")
}
function Get-FailedTestNames([string]$xmlPath) {
    if (-not (Test-Path $xmlPath)) { return @() }
    Get-TestCases $xmlPath |
        Where-Object { $_.OverallResult.success -eq "false" } |
        ForEach-Object { $_.name } |
        Sort-Object
}

$allCases = @(Get-TestCases $outXml)
if ($allCases.Count -eq 0) {
    throw "$outXml contains no test cases (test process exited $testExit). The run did not happen."
}

Write-Host ""
Write-Host "--- summary ---"
Write-Host "test cases run: $($allCases.Count)"

# Mirror the report as readable text so the results directory is browsable.
# WriteAllLines emits UTF-8 without a BOM; Set-Content -Encoding utf8 would prepend
# one under PowerShell 5.1 and break line-oriented tooling on the first entry.
$lines = Get-TestCases $outXml |
    ForEach-Object { "{0,-8} {1}" -f $(if ($_.OverallResult.success -eq "true") { "ok" } else { "FAILED" }), $_.name }
[System.IO.File]::WriteAllLines($outTxt, $lines)

# Reports which tests executed a different number of assertions than a previous run.
# Same set of tests, same seed, same source: a difference means the game did a
# different amount of work inside that test, which a green run does not otherwise show.
function Compare-AssertionStats([string]$oldTsv, [string]$newTsv) {
    if (-not (Test-Path $oldTsv) -or -not (Test-Path $newTsv)) { return }

    function Read-Stats([string]$path) {
        $map = @{}
        foreach ($line in Get-Content $path) {
            $parts = $line -split "`t", 3
            if ($parts.Count -eq 3) { $map[$parts[2]] = [long]$parts[0] }
        }
        return $map
    }

    $old = Read-Stats $oldTsv
    $new = Read-Stats $newTsv
    $moved = foreach ($name in $new.Keys) {
        if ($old.ContainsKey($name) -and $old[$name] -ne $new[$name]) {
            [PSCustomObject]@{ Test = $name; Was = $old[$name]; Now = $new[$name]; Delta = $new[$name] - $old[$name] }
        }
    }
    $moved = @($moved | Sort-Object { [math]::Abs($_.Delta) } -Descending)

    Write-Host ""
    Write-Host "--- assertion counts vs $(Split-Path -Leaf $oldTsv) ---"
    if (-not $moved.Count) {
        Write-Host "Every test shared with that run executed the same number of assertions."
        return
    }
    Write-Host "$($moved.Count) test(s) did a different amount of work (total $(($moved | Measure-Object Delta -Sum).Sum | ForEach-Object { '{0:+#;-#;0}' -f $_ })):"
    $moved | Select-Object -First 20 | ForEach-Object {
        Write-Host ("  {0,+12} {1}  ({2} -> {3})" -f ('{0:+#;-#;0}' -f $_.Delta), $_.Test, $_.Was, $_.Now)
    }
}

if ($Baseline) {
    $failed = @(Get-FailedTestNames $BaselineXml)
    Write-Host ""
    Write-Host "Baseline written: $BaselineXml"
    Write-Host "Baseline failures recorded: $($failed.Count) of $($allCases.Count)"
    $failed | ForEach-Object { Write-Host "  - $_" }
    exit 0
}

if (-not (Test-Path $BaselineXml)) {
    Write-Warning "No baseline at $BaselineXml. Run with -Baseline first."
    exit $testExit
}

$baseFailed = @(Get-FailedTestNames $BaselineXml)
$nowFailed  = @(Get-FailedTestNames $outXml)

$regressions = @($nowFailed  | Where-Object { $baseFailed -notcontains $_ })
$fixed       = @($baseFailed | Where-Object { $nowFailed  -notcontains $_ })

Write-Host ""
Write-Host "--- vs baseline ---"
Write-Host "baseline failures: $($baseFailed.Count)   this run: $($nowFailed.Count)"

if ($fixed.Count) {
    Write-Host "no longer failing ($($fixed.Count)):" -ForegroundColor Green
    $fixed | ForEach-Object { Write-Host "  + $_" -ForegroundColor Green }
}

# Against the most recent earlier run that carries per-test counts. The S0b baseline
# predates them, so there is nothing to compare against until two such runs exist.
$previousStats = @(Get-ChildItem $ResultsDir -Filter "*.stats.tsv" |
        Where-Object { $_.FullName -ne $outStats } | Sort-Object LastWriteTime -Descending)
if ($previousStats.Count) {
    Compare-AssertionStats $previousStats[0].FullName $outStats
}

if ($regressions.Count) {
    Write-Host "REGRESSIONS ($($regressions.Count)):" -ForegroundColor Red
    $regressions | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}

Write-Host "No regressions against the S0b baseline." -ForegroundColor Green
exit 0

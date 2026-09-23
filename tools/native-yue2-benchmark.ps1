[CmdletBinding()]
param(
    [string]$CliPath,
    [string]$ModelDirectory,
    [string]$OutputPath,
    [string]$Text,
    [string]$Style = 'English indie pop, warm lead vocal, acoustic guitar, bass, light drums',
    [int]$Seed = 20260917,
    [int]$Steps = 8,
    [int]$Threads = 8,
    [string]$Backend = 'cuda',
    [string]$ReportDirectory,
    [int]$PollSeconds = 2,
    [switch]$HashModels,
    [switch]$Submit
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Windows PowerShell 5.1 evaluates parameter defaults before $PSScriptRoot is
# populated, so resolve script-relative defaults after the param block.
if ([string]::IsNullOrWhiteSpace($CliPath)) {
    $CliPath = Join-Path (Get-Location) 'build\windows-cuda-release\bin\audiocpp_cli.exe'
}
if ([string]::IsNullOrWhiteSpace($ModelDirectory)) {
    $ModelDirectory = Join-Path (Get-Location) 'models\yue2-q4'
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $OutputPath = Join-Path (Get-Location) "yue2-q4-12gb-test-$stamp.wav"
}
if ([string]::IsNullOrWhiteSpace($ReportDirectory)) {
    $ReportDirectory = Join-Path $PSScriptRoot '..\benchmarks\yue2\reports'
}
if ([string]::IsNullOrWhiteSpace($Text)) {
    $Text = "[Verse]`nMorning light arrives, a new melody begins.`n[Chorus]`nCarry the rhythm home, bright and clear."
}

# Pins copied from docs/native-yue2-12gb-acceptance-test.md. The report records
# them so a result is always tied to an exact runtime and model package.
$profileId = 'native-yue2-q4-nvidia-v1'
$audioCppRepo = 'https://github.com/0xShug0/audio.cpp'
$audioCppBranch = 'dev'
$modelPackageRepo = 'audio-cpp/Yue2-3B-GGUF'
$modelPackageRevision = 'eb116220931de5f373d024d48800338178c7de51'
$mainModel = 'yue2-3b-q4_0.gguf'
$decoderModel = 'yue2-vae-f16.gguf'
$sidecars = @(
    'sidecars\yue2-model-config.json', 'sidecars\yue2-generation-config.json',
    'sidecars\yue2-qwen.tiktoken', 'sidecars\yue2-vae-config.json'
)

function Get-GpuSample {
    $lines = & nvidia-smi --query-gpu=name,driver_version,temperature.gpu,memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $lines) {
        return @()
    }

    return @($lines | ForEach-Object {
        $fields = $_ -split ',\s*'
        [ordered]@{
            name = $fields[0]
            driverVersion = $fields[1]
            temperatureC = [double]$fields[2]
            memoryUsedMiB = [double]$fields[3]
            memoryTotalMiB = [double]$fields[4]
            utilizationPercent = [double]$fields[5]
        }
    })
}

function Add-CudaRuntimeToPath {
    # The audio.cpp CUDA build links cudart/cublas/cufft dynamically; those DLLs
    # live under the CUDA Toolkit (in bin\x64), not next to the CLI. Prepend the
    # detected CUDA runtime directories so the child process can load them.
    $candidates = @()
    if ($env:CUDA_PATH) {
        $candidates += (Join-Path $env:CUDA_PATH 'bin\x64')
        $candidates += (Join-Path $env:CUDA_PATH 'bin')
    }
    $roots = Get-ChildItem 'C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v*' -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending
    foreach ($root in $roots) {
        $candidates += (Join-Path $root.FullName 'bin\x64')
        $candidates += (Join-Path $root.FullName 'bin')
    }

    $added = @()
    foreach ($path in $candidates) {
        if ((Test-Path $path) -and (($env:PATH -split [IO.Path]::PathSeparator) -notcontains $path)) {
            $env:PATH = $path + [IO.Path]::PathSeparator + $env:PATH
            $added += $path
        }
    }
    return $added
}

function Get-WavInfo {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 44) {
        return $null
    }
    $riff = [System.Text.Encoding]::ASCII.GetString($bytes, 0, 4)
    $wave = [System.Text.Encoding]::ASCII.GetString($bytes, 8, 4)
    if ($riff -ne 'RIFF' -or $wave -ne 'WAVE') {
        return $null
    }

    $offset = 12
    $sampleRate = 0
    $channels = 0
    $bitsPerSample = 0
    $blockAlign = 0
    $dataBytes = 0
    while ($offset + 8 -le $bytes.Length) {
        $chunkId = [System.Text.Encoding]::ASCII.GetString($bytes, $offset, 4)
        $chunkSize = [BitConverter]::ToUInt32($bytes, $offset + 4)
        if ($chunkId -eq 'fmt ') {
            $channels = [BitConverter]::ToUInt16($bytes, $offset + 10)
            $sampleRate = [BitConverter]::ToUInt32($bytes, $offset + 12)
            $blockAlign = [BitConverter]::ToUInt16($bytes, $offset + 20)
            $bitsPerSample = [BitConverter]::ToUInt16($bytes, $offset + 22)
        } elseif ($chunkId -eq 'data') {
            $dataBytes = $chunkSize
        }
        $offset += 8 + $chunkSize + ($chunkSize % 2)
    }
    if ($sampleRate -le 0 -or $blockAlign -le 0) {
        return $null
    }

    return [ordered]@{
        bytes = $bytes.Length
        sampleRate = $sampleRate
        channels = $channels
        bitsPerSample = $bitsPerSample
        durationSeconds = [math]::Round($dataBytes / ($sampleRate * $blockAlign), 3)
    }
}

function ConvertTo-QuotedArgument {
    param([string]$Value)

    if ([string]::IsNullOrEmpty($Value)) {
        return '""'
    }
    if ($Value -notmatch '[\s"]') {
        return $Value
    }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Get-ModelFileInfo {
    param([Parameter(Mandatory = $true)][string]$Path)

    $info = [ordered]@{ name = (Split-Path -Leaf $Path); path = $Path; exists = (Test-Path -LiteralPath $Path) }
    if ($info.exists) {
        $file = Get-Item -LiteralPath $Path
        $info.bytes = $file.Length
        if ($HashModels) {
            $info.sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
        }
    }
    return $info
}

function Write-NativeReport {
    param([Parameter(Mandatory = $true)]$Report)

    New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $path = Join-Path $ReportDirectory "native-yue2-$stamp.json"
    $Report | ConvertTo-Json -Depth 16 | Set-Content -NoNewline -Encoding utf8 -Path $path
    Write-Output "Report: $path"
}

if ($PollSeconds -lt 1) {
    throw 'PollSeconds must be at least 1.'
}

$cliFullPath = $null
$resolvedCli = Resolve-Path -LiteralPath $CliPath -ErrorAction SilentlyContinue
if ($resolvedCli) {
    $cliFullPath = $resolvedCli.Path
}
$modelFullPath = $null
$resolvedModel = Resolve-Path -LiteralPath $ModelDirectory -ErrorAction SilentlyContinue
if ($resolvedModel) {
    $modelFullPath = $resolvedModel.Path
}

$startedAt = Get-Date
$gpuSamples = @(Get-GpuSample)
$modelFiles = @()
foreach ($relative in @($mainModel, $decoderModel) + $sidecars) {
    $modelFiles += Get-ModelFileInfo -Path (Join-Path $ModelDirectory $relative)
}

$report = [ordered]@{
    profileId = $profileId
    runMode = $(if ($Submit) { 'submitted' } else { 'preflight-only' })
    startedAtUtc = $startedAt.ToUniversalTime().ToString('o')
    runtime = [ordered]@{
        repo = $audioCppRepo
        branch = $audioCppBranch
        cliPath = $cliFullPath
        backend = $Backend
    }
    model = [ordered]@{
        repo = $modelPackageRepo
        revision = $modelPackageRevision
        directory = $modelFullPath
        mainModel = $mainModel
        decoder = $decoderModel
        files = $modelFiles
    }
    request = [ordered]@{
        threads = $Threads
        steps = $Steps
        seed = $Seed
        backend = $Backend
        style = $Style
        text = $Text
    }
    gpuSamples = $gpuSamples
    command = $null
    logPath = $null
    errorLogPath = $null
    exitCode = $null
    elapsedSeconds = $null
    output = $null
    terminalStatus = $null
    notes = @()
}

$preflightProblems = @()
if (-not $cliFullPath) {
    $preflightProblems += "CLI not found: $CliPath"
}
if (-not $modelFullPath) {
    $preflightProblems += "Model directory not found: $ModelDirectory"
} else {
    foreach ($file in $modelFiles) {
        if (-not $file.exists) {
            $preflightProblems += "Missing model file: $($file.path)"
        }
    }
}
if ($gpuSamples.Count -eq 0) {
    $preflightProblems += 'nvidia-smi did not report a GPU; the CUDA backend cannot be verified.'
}

if ($preflightProblems.Count -gt 0) {
    $report.terminalStatus = 'preflight-failed'
    $report.notes = $preflightProblems
    $report.elapsedSeconds = [math]::Round(((Get-Date) - $startedAt).TotalSeconds, 3)
    Write-NativeReport -Report $report
    throw "Preflight failed:`n - $($preflightProblems -join "`n - ")"
}

if (-not $Submit) {
    $report.terminalStatus = 'preflight-passed'
    $report.elapsedSeconds = [math]::Round(((Get-Date) - $startedAt).TotalSeconds, 3)
    Write-NativeReport -Report $report
    Write-Output 'Preflight passed. Re-run with -Submit to run the acceptance generation.'
    return
}

if (Test-Path -LiteralPath $OutputPath) {
    Remove-Item -LiteralPath $OutputPath -Force
}
$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
}

$logDirectory = Join-Path $ReportDirectory 'logs'
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath = Join-Path $logDirectory "native-yue2-$stamp.out.log"
$errorLogPath = Join-Path $logDirectory "native-yue2-$stamp.err.log"

$arguments = @(
    '--task', 'gen',
    '--family', 'yue2',
    '--model', $ModelDirectory,
    '--backend', $Backend,
    '--threads', [string]$Threads,
    '--text', $Text,
    '--request-option', "style=$Style",
    '--request-option', 'cot=full',
    '--request-option', "seed=$Seed",
    '--request-option', "num_inference_steps=$Steps",
    '--session-option', "yue2.model_gguf=$mainModel",
    '--session-option', "yue2.vae_gguf=$decoderModel",
    '--out', $OutputPath,
    '--log'
)
$commandLine = ($arguments | ForEach-Object { ConvertTo-QuotedArgument -Value $_ }) -join ' '
$report.command = "$CliPath $commandLine"
$report.logPath = $logPath
$report.errorLogPath = $errorLogPath

$cudaRuntimeDirs = @(Add-CudaRuntimeToPath)
$report.cudaRuntimeDirs = $cudaRuntimeDirs
Write-Output "Running native YuE2 acceptance generation..."
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$process = Start-Process -FilePath $cliFullPath -ArgumentList $commandLine -PassThru -NoNewWindow `
    -RedirectStandardOutput $logPath -RedirectStandardError $errorLogPath

while (-not $process.HasExited) {
    Start-Sleep -Seconds $PollSeconds
    $report.gpuSamples += @(Get-GpuSample)
}
$process.WaitForExit()
$process.Refresh()
$stopwatch.Stop()

# Start-Process -PassThru can leave ExitCode null until the process object is
# refreshed; fall back to the output WAV when Windows still reports nothing.
$exitCode = $process.ExitCode
$report.exitCode = if ($null -ne $exitCode) { $exitCode } else { 'unknown' }
$report.elapsedSeconds = [math]::Round($stopwatch.Elapsed.TotalSeconds, 3)

if (Test-Path -LiteralPath $OutputPath) {
    $report.output = Get-WavInfo -Path $OutputPath
}

if ($null -eq $exitCode) {
    if ($null -eq $report.output) {
        $report.terminalStatus = 'process-failed'
        Write-NativeReport -Report $report
        throw "audiocpp_cli ended without an exit code and produced no valid WAV. See $errorLogPath."
    }
} elseif ($exitCode -ne 0) {
    $report.terminalStatus = 'process-failed'
    Write-NativeReport -Report $report
    $hint = if ($exitCode -eq -1073741515) { ' (STATUS_DLL_NOT_FOUND: a required runtime DLL is missing from PATH)' } else { '' }
    throw "audiocpp_cli exited with code $exitCode$hint. See $errorLogPath."
}
if (-not $report.output) {
    $report.terminalStatus = 'output-invalid'
    Write-NativeReport -Report $report
    throw "The run completed but no valid WAV was produced at $OutputPath."
}

$report.terminalStatus = 'success'
Write-NativeReport -Report $report
Write-Output "Acceptance generation complete in $($report.elapsedSeconds) seconds."
Write-Output ("Output: {0} ({1:N1} s, {2} Hz, {3} ch, {4} bytes)" -f $OutputPath,
    $report.output.durationSeconds, $report.output.sampleRate, $report.output.channels, $report.output.bytes)
Write-Output 'Next: listen for dropouts/distortion, then import the WAV into a disposable Audacity project.'
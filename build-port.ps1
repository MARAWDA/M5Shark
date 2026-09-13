param(
  [ValidateSet('M5SHARK_V8', 'HOSYOND_35', 'WAVESHARE_C5_28')]
  [string]$Target = 'M5SHARK_V8',
  [string]$Port = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$config = Join-Path $root 'arduino-cli-esp32-3.3.4.yaml'
$cli = Get-Command arduino-cli -ErrorAction SilentlyContinue
if (-not $cli) { throw 'arduino-cli was not found in PATH.' }

switch ($Target) {
  'HOSYOND_35' {
    $define = 'MARAUDER_HOSYOND_35'
    $fqbn = 'esp32:esp32:esp32'
    $profile = 'Hosyond 3.5in ST7796U + XPT2046/resistive touch'
  }
  'WAVESHARE_C5_28' {
    $define = 'MARAUDER_WAVESHARE_C5_28'
    $fqbn = 'esp32:esp32:esp32c5:FlashSize=32M,PSRAM=enabled'
    $profile = 'Waveshare C5 2.8in ST7789 + CST3530 capacitive touch'
  }
  default {
    $define = 'MARAUDER_V8'
    $fqbn = 'esp32:esp32:esp32c5:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=enabled'
    $profile = 'M5SHARK v8'
  }
}

Write-Host "Building $profile"
$build = Join-Path $root ("build/" + $Target.ToLowerInvariant())
New-Item -ItemType Directory -Path $build -Force | Out-Null
$args = @(
  'compile', '--config-file', $config, '--clean', '--jobs', '1',
  '--fqbn', $fqbn, '--libraries', (Join-Path $root '.build-libraries'),
  '--output-dir', $build,
  '--build-property', "compiler.cpp.extra_flags=-D$define",
  (Join-Path $root 'm5shark')
)
& $cli.Source @args
if ($LASTEXITCODE -ne 0) { throw "Build failed for $Target." }
if ($Port) {
  & $cli.Source 'upload' '--config-file' $config '--fqbn' $fqbn '--port' $Port (Join-Path $root 'm5shark')
  if ($LASTEXITCODE -ne 0) { throw "Upload failed for $Target on $Port." }
}

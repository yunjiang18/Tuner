$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "build_cm7_lvgl.ps1")
if ($LASTEXITCODE -ne 0) {
  throw "Build failed"
}

& (Join-Path $PSScriptRoot "flash_cm7_lvgl.ps1")
if ($LASTEXITCODE -ne 0) {
  throw "Flash failed"
}

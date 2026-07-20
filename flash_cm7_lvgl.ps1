$ErrorActionPreference = "Stop"

$Programmer = "D:\ST\STM32CubeIDE_1.16.1\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.1.400.202404281720\tools\bin\STM32_Programmer_CLI.exe"
$HexFile = Join-Path (Get-Location) "build\1_CM7\1.hex"

if (-not (Test-Path -LiteralPath $HexFile)) {
  throw "HEX file not found: $HexFile. Run build first."
}

& $Programmer -c port=SWD mode=UR -d $HexFile -v -rst
if ($LASTEXITCODE -ne 0) {
  throw "Flash failed"
}

Write-Host "CM7 LVGL flash complete: $HexFile"

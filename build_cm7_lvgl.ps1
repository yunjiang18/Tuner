$ErrorActionPreference = "Stop"

$Armcc = "D:\Kiel\ARM\ARMCC\bin\armcc.exe"
$Armasm = "D:\Kiel\ARM\ARMCC\bin\armasm.exe"
$Armlink = "D:\Kiel\ARM\ARMCC\bin\armlink.exe"
$Fromelf = "D:\Kiel\ARM\ARMCC\bin\fromelf.exe"

$CommonArgs = @(
  "--c99", "--gnu", "-c",
  "--cpu", "Cortex-M7.fp.dp",
  "-g", "-O3",
  "--apcs=interwork",
  "--split_sections",
  "-I", "CM7\Core\Inc",
  "-I", "Drivers\STM32H7xx_HAL_Driver\Inc",
  "-I", "Drivers\STM32H7xx_HAL_Driver\Inc\Legacy",
  "-I", "Drivers\CMSIS\Device\ST\STM32H7xx\Include",
  "-I", "Drivers\CMSIS\Include",
  "-I", ".cmsis\include",
  "-I", "MDK-ARM\RTE\_1_CM7",
  "-I", ".",
  "-I", "Middlewares\LVGL",
  "-DLV_CONF_INCLUDE_SIMPLE",
  "-DCORE_CM7",
  "-DUSE_HAL_DRIVER",
  "-DSTM32H745xx",
  "-DUSE_PWR_SMPS_1V8_SUPPLIES_LDO"
)

function Compile-CFile {
  param([string]$Source)

  $Object = "build\1_CM7\.obj\" + ($Source -replace "\.c$", ".o")
  New-Item -ItemType Directory -Path (Split-Path $Object) -Force | Out-Null
  & $Armcc @CommonArgs -o $Object $Source
  if ($LASTEXITCODE -ne 0) {
    throw "Compile failed: $Source"
  }
}

$ProjectSources = @(
  "CM7\Core\Src\main.c",
  "CM7\Core\Src\stm32h7xx_hal_msp.c",
  "CM7\Core\Src\stm32h7xx_it.c",
  "CM7\Core\Src\lcd_ili9341.c",
  "CM7\Core\Src\lv_port_disp.c",
  "CM7\Core\Src\xpt2046_touch.c",
  "CM7\Core\Src\tuner_ui.c",
  "CM7\Core\Src\audio_capture.c",
  "CM7\Core\Src\pitch_detector.c",
  "Common\Src\system_stm32h7xx_dualcore_boot_cm4_cm7.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_adc.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_adc_ex.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_cortex.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_dma.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_dma_ex.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_exti.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_flash.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_flash_ex.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_gpio.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_hsem.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_i2c.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_i2c_ex.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_mdma.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_pwr.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_pwr_ex.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_rcc.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_rcc_ex.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_spi.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_tim.c",
  "Drivers\STM32H7xx_HAL_Driver\Src\stm32h7xx_hal_tim_ex.c"
)

foreach ($Source in $ProjectSources) {
  Compile-CFile $Source
}

$LvglSources = Get-ChildItem -Path "Middlewares\LVGL\src" -Recurse -Filter "*.c" | ForEach-Object {
  $_.FullName.Substring((Get-Location).Path.Length + 1)
}

foreach ($Source in $LvglSources) {
  Compile-CFile $Source
}

$StartupObject = "build\1_CM7\.obj\MDK-ARM\startup_stm32h745xx_CM7.o"
New-Item -ItemType Directory -Path (Split-Path $StartupObject) -Force | Out-Null
& $Armasm --cpu Cortex-M7.fp.dp -g --apcs=interwork -I "MDK-ARM\RTE\_1_CM7" -o $StartupObject "MDK-ARM\startup_stm32h745xx_CM7.s"
if ($LASTEXITCODE -ne 0) {
  throw "Compile failed: MDK-ARM\startup_stm32h745xx_CM7.s"
}

$Objects = Get-ChildItem -Path "build\1_CM7\.obj\CM7", "build\1_CM7\.obj\Common", "build\1_CM7\.obj\Drivers", "build\1_CM7\.obj\Middlewares" -Recurse -Filter "*.o" |
  ForEach-Object { ".\" + $_.FullName.Substring((Get-Location).Path.Length + 1) }
$Objects += ".\$StartupObject"

$LinkResponse = @(
  "--cpu Cortex-M7.fp.dp --scatter $((Join-Path $PSScriptRoot 'MDK-ARM/stm32h745xx_flash_CM7.sct') -replace '\\', '/') --strict --summary_stderr --info summarysizes --map --xref --callgraph --symbols --info sizes --info totals --info unused --info veneers",
  "--list .\build\1_CM7\1.map"
) + $Objects + @("-o .\build\1_CM7\1.axf")

Set-Content -Path "build\1_CM7\1_lvgl.lnp" -Value $LinkResponse -Encoding ASCII
& $Armlink --via "build\1_CM7\1_lvgl.lnp"
if ($LASTEXITCODE -ne 0) {
  throw "Link failed"
}

& $Fromelf --i32combined --output="build\1_CM7\1.hex" "build\1_CM7\1.axf"
if ($LASTEXITCODE -ne 0) {
  throw "HEX export failed"
}

Write-Host "CM7 LVGL build complete: build\1_CM7\1.hex"

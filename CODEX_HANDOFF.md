# AutoTuner_Core 项目交接说明

更新时间：2026-07-17  
项目路径：`D:\XingHai\7.13.2\UI2\AutoTuner_Core`  
目标平台：STM32H745ZIT6 双核核心板（CM7 + CM4）  
当前阶段：ILI9341/LVGL UI 已跑通；音频采样、电机驱动和自动调弦算法尚未接入。

---

## 1. 给下一次 Codex 的首条指令

新建 Codex 项目后，可以直接发送下面这段话：

> 请先完整阅读 `D:\XingHai\7.13.2\UI2\AutoTuner_Core\CODEX_HANDOFF.md`，再检查实际代码。当前可用基线是 STM32H745ZIT6、25 MHz HSE、CM7 200 MHz、VOS3、`USE_PWR_SMPS_1V8_SUPPLIES_LDO`、ILI9341 硬件 SPI1 和 LVGL 8.3.11。不要未经确认修改时钟、供电模式、Option Bytes、屏幕初始化序列或背光极性。每次只做一个可验证改动，先编译，再说明是否需要烧录。保留现有文件和用户改动，不要使用破坏性 Git 操作。音频 ADC、音高算法和电机控制目前都属于待开发功能，不要假定已经实现。

---

## 2. 当前已验证基线

### 2.1 MCU与晶振

- MCU：STM32H745ZIT6。
- 外部高速晶振 HSE：25 MHz，原理图中 Y1，负载电容 C17/C18 为 22 pF。
- 外部低速晶振 LSE：32.768 kHz，原理图中 Y2，负载电容 C15/C16 为 10 pF。
- 当前系统时钟使用 HSE，不是 HSE bypass。
- `HSE_VALUE` 已设为 25 MHz。

### 2.2 当前系统时钟

配置位置：`CM7/Core/Src/main.c` 的 `SystemClock_Config()`。

```text
HSE             = 25 MHz
PLL1M           = 5
PLL1输入         = 5 MHz
PLL1N           = 80
PLL1 VCO        = 400 MHz
PLL1P           = 2
CM7 SYSCLK      = 200 MHz
AHB分频          = /2
HCLK/AXI/AHB    = 100 MHz
APB1/2/3/4      = 100 MHz（各APB分频均为 /1）
Flash latency   = 2 WS
电压等级         = VOS3
```

时钟配置失败时，代码会退回 HSI 64 MHz，使诊断UI仍有机会运行。

调试变量：

```text
SystemClock_UsingPLL      地址 0x20000000
  1 = 正在使用 HSE/PLL
  0 = 已退回 HSI

SystemClock_FailureStage  地址 0x20000004
  0 = 无错误
  2 = VOSRDY等待失败
  3 = HSE/PLL初始化失败
  4 = 时钟切换失败
```

以上地址来自当前 `build/1_CM7/1.map`；重新链接后仍应优先通过符号名查看，不能永久依赖固定地址。

### 2.3 当前供电模式

编译宏：

```text
USE_PWR_SMPS_1V8_SUPPLIES_LDO
```

含义：内部 SMPS 输出 1.8 V，再给内部 LDO 供电。供电配置在启动文件的 `ExitRun0Mode()` 中完成，不是在 `main()` 中重复调用。

对应文件：

```text
Common/Src/system_stm32h7xx_dualcore_boot_cm4_cm7.c
```

原理图中观察到的相关连接：

- `VLXSMPS -> L1 2.2 uH -> VDD_SDC`。
- `VFBSMPS` 通过 R11（0 Ω）反馈到 `VDD_SDC`。
- `VDD_SDC` 通过 SB8 接到 `VDD_LDO`。
- SB9 是 VCC 到 VDD_LDO 的备选连接。
- 当前软件配置与“SMPS 1.8 V 给 LDO”路径相符。

用户已经决定暂时不继续排查供电硬件，除非用户再次明确要求，不要主动改焊桥或改供电模式。

### 2.4 双核状态

- CM7和CM4工程中都保留了 `DUAL_CORE_BOOT_SYNC_SEQUENCE`。
- CM7用HSEM 0唤醒CM4。
- CM4当前只是进入STOP、等待CM7唤醒，随后运行空循环，没有音频算法。
- 最后一次已知Option Bytes状态：BCM7和BCM4均启用。
- 曾临时禁用CM4做对照实验，冷启动结果没有改变，因此CM4不是此前UI消失的根因。
- CM7构建脚本当前只编译、链接和烧录CM7；不要误认为它同时更新了CM4镜像。

---

## 3. 屏幕和UI

### 3.1 屏幕硬件

- 控制器：ILI9341。
- 分辨率：240 × 320，竖屏。
- 接口：硬件 SPI1，仅发送，不读取MISO。
- SPI模式：Mode 0，即 CPOL=0、CPHA=第1边沿。
- 数据宽度：8 bit，MSB first。

引脚：

| 功能 | MCU引脚 | 配置 |
|---|---|---|
| SPI1_SCK | PA5 | AF5 |
| SPI1_MOSI | PA7 | AF5 |
| LCD_CS | PB6 | GPIO输出，低有效 |
| LCD_RST | PB7 | GPIO输出，低有效 |
| LCD_DC | PB8 | GPIO输出 |
| LCD_BL | PA8 | GPIO输出，高有效 |

SPI时钟来源：

```text
优先：HSE 25 MHz -> CKPER/CLKP -> SPI123 -> /8 = 3.125 MHz
HSE失败回退：HSI 64 MHz -> CKPER/CLKP -> SPI123 -> /8 = 8 MHz
```

驱动文件：

```text
CM7/Core/Src/lcd_ili9341.c
CM7/Core/Inc/lcd_ili9341.h
```

### 3.2 LVGL

- LVGL版本：8.3.11。
- 色深：RGB565，16 bit。
- LVGL内存：32 KiB。
- Tick来源：`HAL_GetTick()`。
- 显示缓冲：240 × 20行，单缓冲。
- 当前UI是演示数据轮播，不是真实音频测量。

主要文件：

```text
lv_conf.h
CM7/Core/Src/lv_port_disp.c
CM7/Core/Src/tuner_ui.c
CM7/Core/Inc/tuner_ui.h
```

当前UI每约900 ms轮换古筝、二胡、吉他演示数据。后续接入算法时，应把 `demo_frames[]` 替换成线程安全的真实调音结果接口。

### 3.3 背光结论

背光电路：

```text
PA8/TFT_BL -> 1 kΩ -> S8050基极
S8050发射极 -> GND
S8050集电极 -> 10 Ω -> LEDK
```

- 背光高电平有效。
- PA8从启动早期就被强制拉高，主循环中也持续写高。
- PA8持续高电平已经是100%占空比；PWM只能降低平均亮度，不能超过当前亮度。
- 曾实验发送 ILI9341 的 `0x51/0x53/0x55` 亮度相关命令，屏幕反而更暗，随后已完全删除。
- 不要重新加入上述命令。
- 若仍嫌暗，应检查LED供电、电流、LEDK压降和S8050饱和情况；不能通过占空比提高到100%以上，也不要未经屏幕规格确认就减小10 Ω电阻。

---

## 4. 编译、烧录与调试

### 4.1 构建工具

当前脚本使用 Keil ARMCC 5工具链，固定路径：

```text
D:\Kiel\ARM\ARMCC\bin\armcc.exe
D:\Kiel\ARM\ARMCC\bin\armasm.exe
D:\Kiel\ARM\ARMCC\bin\armlink.exe
D:\Kiel\ARM\ARMCC\bin\fromelf.exe
```

编译命令，在项目目录运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build_cm7_lvgl.ps1
```

输出：

```text
build/1_CM7/1.axf
build/1_CM7/1.hex
build/1_CM7/1.map
```

当前已存在的基线HEX：

```text
D:\XingHai\7.13.2\UI2\AutoTuner_Core\build\1_CM7\1.hex
```

该文件最后一次构建时间为2026-07-14 11:54:47。任何新改动后都必须重新构建，不能把旧HEX当作新代码。

### 4.2 烧录

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\flash_cm7_lvgl.ps1
```

或者构建并烧录：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build_and_flash_cm7_lvgl.ps1
```

脚本调用STM32CubeProgrammer CLI：

```text
D:\ST\STM32CubeIDE_1.16.1\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.1.400.202404281720\tools\bin\STM32_Programmer_CLI.exe
```

连接方式：SWD，Under Reset，下载后校验并复位。

### 4.3 SWD日志和变量

- SWD本身不会自动显示 `printf()`。
- 编译、烧录日志看 VS Code Terminal 或 Codex命令输出。
- 断点、变量和寄存器看调试器的 Variables/Watch/Registers/Debug Console。
- 当前5针SWD座没有单独引出SWO，因此不能默认使用ITM/SWO日志。
- 若后续需要连续运行日志，优先选择SEGGER RTT或新增UART TX。

---

## 5. 已做过的时钟/供电实验

以下实验已经做过，不要无理由重复：

| 实验 | 结果 |
|---|---|
| HSE 25 MHz、VOS3、CM7 200 MHz | UI恢复，是当前基线 |
| VOS2、CM7 300 MHz | 无UI |
| VOS1、CM7 400 MHz | 早期配置曾无UI；在VOS2未稳定前不应继续追400 MHz |
| 临时禁用CM4后冷启动 | 结果未改善，CM4不是根因 |
| `HAL_PWREx_ConfigSupply()` 在 `HAL_Init()` 后再次调用 | 返回 `HAL_ERROR`，供电配置已在启动阶段锁定 |
| 把ACTVOSRDY等待改为永久等待 | 程序卡在启动阶段，UI不可能出现 |
| 恢复带超时的ACTVOSRDY等待 | 恢复当前可用启动行为 |

历史SWD观察值：

```text
PWR_CR3  = 0x00000056
PWR_CSR1 = 0x00004000
PWR_D3CR = 0x00006000
ACTVOSRDY观察为0
```

注意：虽然ACTVOSRDY的历史读数异常，当前200 MHz/VOS3版本可以显示UI。为了避免永久卡死，`WaitForActvosReady()` 当前保留有限超时；`SystemClock_Config()` 的VOSRDY等待也有100 ms超时和HSI回退。

### 时钟/供电修改原则

1. 先保留当前200 MHz版本作为可回退基线。
2. 一次只改一个变量，例如只改VOS或只改PLL。
3. 每次改动后完整断电冷启动，不只按复位键。
4. 同时观察 `SystemClock_UsingPLL`、`SystemClock_FailureStage`、PC和PWR/RCC寄存器。
5. 不要在 `main()` 里重复调用 `HAL_PWREx_ConfigSupply()`。
6. 不要把ACTVOSRDY改成永久等待后直接期待UI；若要实验，必须有可观察诊断手段。
7. 300/400 MHz再次测试前，应先明确硬件供电路径、Flash等待周期、VOS切换顺序和冷启动行为。

---

## 6. 音频采样硬件：当前状态与建议

### 6.1 当前状态

- 音频采样硬件尚未最终确定。
- ADC通道和具体MCU引脚尚未选择。
- DMA、定时器触发ADC和音频缓冲尚未写入工程。
- 压电陶瓷和驻极体咪头都处于实验阶段。

### 6.2 压电陶瓷传感器

测试模块图片对应三针：`GND / NC / Analog`，NC保持悬空。

示波器建议：

```text
输入阻抗：1 MΩ，不用50 Ω
探头：10X，示波器菜单也设10X
耦合：DC
带宽限制：20 MHz
时基：10～20 ms/div观察周期；100 ms/div观察衰减
触发：CH1上升沿、Single、约10～50 mV
FFT：0～2 kHz，Hann窗，不要用5 MHz中心/10 MHz跨度
```

压电片可能产生正负尖峰，不能直接接STM32 ADC。至少需要：

- 约1.65 V中点偏置。
- 输入限流。
- 0～3.3 V钳位保护。
- 低通抗混叠滤波。
- 必要时高输入阻抗缓冲/电荷放大器。

### 6.3 MAX4466驻极体咪头

原参考图使用5 V供电，输出直流中心约2.5 V，强声时可能超过STM32 ADC的3.3 V范围，因此不能按5 V版本直接连接ADC。

推荐：整套MAX4466及其偏置分压改用3.3 V，使OUT静态中心约1.65 V。MAX4466允许2.4～5.5 V供电。

建议ADC前端：

```text
MAX4466 OUT -> 1 kΩ -> ADC节点
ADC节点 -> 68 nF -> GND
可选：ADC节点用BAT54S钳位到3.3 V和GND
```

1 kΩ + 68 nF的截止频率约2.34 kHz，足以覆盖21弦古筝73～1175 Hz的基频。若要保留更多泛音，可改用33 nF，截止约4.8 kHz。

接入MCU前必须用示波器确认：

```text
安静时OUT约1.65 V
最强拨弦时仍在0～3.3 V以内
建议实际保持在约0.1～3.2 V
波形不能长期贴住0 V或3.3 V
```

不要依赖GPIO的“5 V tolerant”标识：ADC有效转换输入范围仍是0～VREF+。

---

## 7. 以21弦古筝为例的算法规划

### 7.1 常见D调目标表

默认约定：1号弦最高，21号弦最低；目标音表必须做成可配置，不能硬编码为唯一调式。

| 弦号 | 音名 | 频率/Hz |
|---:|---:|---:|
| 1 | D6 | 1174.66 |
| 2 | B5 | 987.77 |
| 3 | A5 | 880.00 |
| 4 | F#5 | 739.99 |
| 5 | E5 | 659.26 |
| 6 | D5 | 587.33 |
| 7 | B4 | 493.88 |
| 8 | A4 | 440.00 |
| 9 | F#4 | 369.99 |
| 10 | E4 | 329.63 |
| 11 | D4 | 293.66 |
| 12 | B3 | 246.94 |
| 13 | A3 | 220.00 |
| 14 | F#3 | 185.00 |
| 15 | E3 | 164.81 |
| 16 | D3 | 146.83 |
| 17 | B2 | 123.47 |
| 18 | A2 | 110.00 |
| 19 | F#2 | 92.50 |
| 20 | E2 | 82.41 |
| 21 | D2 | 73.42 |

### 7.2 推荐采样和检测

初版参数：

```text
采样率：16 kHz
DMA缓冲：4096点
分析时长：256 ms
有效基频：约55～1400 Hz
ADC：12位或16位，定时器触发，DMA循环双缓冲
```

推荐处理链：

```text
ADC/I2S + DMA
  -> 去直流偏置
  -> 限幅/能量检查
  -> 约50～1500 Hz带通或高低通组合
  -> 拨弦起音检测
  -> 跳过最初约30～60 ms强瞬态
  -> YIN / NSDF / 归一化自相关估计基频
  -> 谐波与置信度检查
  -> 连续3帧中值
  -> 音分误差
  -> 调弦控制器
```

不建议只取FFT最大峰，因为古筝的二次或三次泛音可能比基频更强，并且其他弦会产生共鸣。

音分公式：

```c
error_cents = 1200.0f * log2f(measured_hz / target_hz);
```

- `error_cents < 0`：音偏低，需要增加张力。
- `error_cents > 0`：音偏高，需要减小张力。

### 7.3 必须先指定弦号

古筝共鸣强，不能在所有21根弦中盲目选择“最近音”。UI或机构应先明确正在调整哪一根弦，再在目标附近搜索，例如限制到目标频率±2个半音。

### 7.4 建议状态机

```text
IDLE
 -> WAIT_PLUCK
 -> CAPTURE
 -> ESTIMATE
 -> DECIDE
 -> MOTOR_MOVE
 -> SETTLE
 -> WAIT_REPLUCK
 -> VERIFY
 -> FINISHED / FAULT
```

电机转动时不要采样：机械声、电磁噪声和结构振动会污染音频。采用“停止电机 -> 拨弦 -> 测量 -> 短促转动 -> 停止 -> 再拨弦”的闭环。

如果没有自动拨弦机构，第一版应做半自动：用户根据UI提示重复拨弦，系统控制电机。真正无人调弦需要额外的电磁铁、舵机或其他自动拨弦机构。

### 7.5 控制阈值初值

| 绝对误差 | 动作 |
|---|---|
| >30 cents | 粗调 |
| 8～30 cents | 中调 |
| 2～8 cents | 微调 |
| <=2 cents | 停止并复测 |
| 连续3次 <=2 cents | 完成 |

“每步对应多少音分”不能全弦共用固定值。每根弦需要独立标定方向、每步音分、最大单次步数和累计步数，并根据动作前后的音分变化在线更新。最终接近目标时最好从偏低方向逐渐拧紧，以减小机械回差。

### 7.6 安全条件

下列任一情况立即禁止或停止电机：

- 没有检测到有效拨弦。
- 音高置信度不足。
- 测得频率明显不属于目标弦。
- ADC削顶或输入过弱。
- 电机堵转/过流。
- 单次步数、累计步数或总时间超限。
- 音高超过目标安全上限。
- 电机动作后音高没有相应变化。

### 7.7 建议的软件模块

后续可以逐步新增：

```text
audio_capture.c/.h       ADC + 定时器 + DMA双缓冲
audio_frontend.c/.h      去直流、滤波、能量和削顶检测
pitch_detector.c/.h      YIN/NSDF与置信度
guzheng_tuning.c/.h      21弦目标表和音分计算
tuning_controller.c/.h   状态机和安全策略
motor_driver.c/.h        具体电机/驱动芯片接口
tuning_simulator.c/.h    无硬件时的合成音和虚拟电机
```

建议先在CM7单核跑通完整数据流；当UI和音频实时任务出现资源竞争后，再考虑CM4负责采样/音高检测、CM7负责UI/控制。不要一开始就为了双核而增加同步复杂度。

---

## 8. 下一阶段推荐顺序

1. 确定最终拾音方式：接触式压电或3.3 V MAX4466咪头。
2. 确定ADC引脚，并确认该引脚没有与SPI、SWD、晶振或板载功能冲突。
3. 示波器验证最弱和最强拨弦的直流中心、峰峰值和频谱。
4. 实现16 kHz定时器触发ADC + DMA双缓冲。
5. 先把原始ADC数据通过RTT/UART导出，验证采样率和波形。
6. 用合成信号和录音离线验证YIN/NSDF。
7. 在MCU上只做音高显示，暂不接电机。
8. 确定电机类型、驱动芯片、减速比、限位/电流检测和一机一弦或移动式机构。
9. 用虚拟电机实现调弦状态机。
10. 最后接真实电机，小步限幅测试，逐弦标定。

尚未确定、需要用户补充的关键参数：

- 选择压电还是咪头，或两者都保留。
- STM32 ADC具体通道/引脚。
- 电机类型（步进、直流减速、舵机等）。
- 电机驱动芯片型号。
- 电机与弦轴的机械连接和减速比。
- 一根弦一个电机，还是一个移动式执行机构。
- 是否有电流检测、编码器、限位和自动拨弦机构。

---

## 9. 重要文件索引

| 文件 | 用途 |
|---|---|
| `CM7/Core/Src/main.c` | CM7启动、时钟、GPIO、UI主循环 |
| `Common/Src/system_stm32h7xx_dualcore_boot_cm4_cm7.c` | 系统启动和供电模式配置 |
| `CM7/Core/Src/lcd_ili9341.c` | ILI9341与SPI1驱动 |
| `CM7/Core/Src/lv_port_disp.c` | LVGL显示端口 |
| `CM7/Core/Src/tuner_ui.c` | 当前演示UI |
| `lv_conf.h` | LVGL配置 |
| `CM4/Core/Src/main.c` | 当前CM4空循环和双核同步 |
| `build_cm7_lvgl.ps1` | CM7完整构建 |
| `flash_cm7_lvgl.ps1` | CM7 SWD烧录 |
| `build_and_flash_cm7_lvgl.ps1` | 构建并烧录 |
| `build/1_CM7/1.map` | 符号地址和链接信息 |
| `1.ioc` | CubeMX工程配置；修改前需确认不会覆盖手工代码 |

特别注意：屏幕、时钟和背光代码包含手工修改。若重新用CubeMX生成代码，必须先比较差异，避免覆盖 `USER CODE` 之外的手工实现。

---

## 10. 参考资料

- STM32H745数据手册：<https://www.st.com/resource/en/datasheet/stm32h745xi.pdf>
- STM32H745/755参考手册RM0399：<https://www.st.com/resource/en/reference_manual/rm0399-stm32h745755-and-stm32h747757-advanced-armbased-32bit-mcus-stmicroelectronics.pdf>
- STM32H745/755勘误ES0445：<https://www.st.com/resource/en/errata_sheet/es0445-stm32h745xig-stm32h755xi-stm32h747xig-stm32h757xi-device-errata-stmicroelectronics.pdf>
- STM32H74x/75x硬件入门AN4938：<https://www.st.com/resource/en/application_note/an4938-getting-started-with-stm32h74xig-and-stm32h75xig-hardware-development-stmicroelectronics.pdf>
- MAX4466官方资料：<https://www.analog.com/en/products/MAX4466.html>
- DFRobot DFR0052压电振动传感器：<https://wiki.dfrobot.com/dfr0052/>
- 21弦古筝声学与频率资料：<https://bioresources.cnr.ncsu.edu/resources/acoustics-of-the-guzheng-chinese-plucked-zither/>

---

## 11. 交接底线

- 当前200 MHz/VOS3/UI版本是可用基线，任何高频实验都必须能够回退。
- 不要把5 V模拟输出直接送入3.3 V ADC。
- 不要在电机运行时相信音高检测结果。
- 不要只用FFT最大峰判断古筝基频。
- 不要在没有限步、过流和音高上限保护时驱动真实弦轴。
- 不要把历史实验结论当成已解决的硬件原因；300/400 MHz无UI的根因尚未最终确定。

#include "lcd_ili9341.h"

#define LCD_CS_Pin GPIO_PIN_6
#define LCD_CS_GPIO_Port GPIOB
#define LCD_RST_Pin GPIO_PIN_7
#define LCD_RST_GPIO_Port GPIOB
#define LCD_DC_Pin GPIO_PIN_8
#define LCD_DC_GPIO_Port GPIOB
#define LCD_SCK_Pin GPIO_PIN_5
#define LCD_SCK_GPIO_Port GPIOA
#define LCD_MOSI_Pin GPIO_PIN_7
#define LCD_MOSI_GPIO_Port GPIOA
#define LCD_BL_Pin GPIO_PIN_8
#define LCD_BL_GPIO_Port GPIOA

static SPI_HandleTypeDef lcd_spi;

static void LCD_SPI_Init(void)
{
  /* Prefer HSE for the LCD. If system-clock setup fell back because HSE/PLL
     failed, use HSI so the diagnostic UI remains available. */
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) != RESET)
  {
    __HAL_RCC_CLKP_CONFIG(RCC_CLKPSOURCE_HSE);
  }
  else
  {
    __HAL_RCC_CLKP_CONFIG(RCC_CLKPSOURCE_HSI);
  }
  __HAL_RCC_SPI123_CONFIG(RCC_SPI123CLKSOURCE_CLKP);
  __HAL_RCC_SPI1_CLK_ENABLE();

  lcd_spi.Instance = SPI1;
  lcd_spi.Init.Mode = SPI_MODE_MASTER;
  lcd_spi.Init.Direction = SPI_DIRECTION_2LINES;
  lcd_spi.Init.DataSize = SPI_DATASIZE_8BIT;
  lcd_spi.Init.CLKPolarity = SPI_POLARITY_LOW;
  lcd_spi.Init.CLKPhase = SPI_PHASE_1EDGE;
  lcd_spi.Init.NSS = SPI_NSS_SOFT;
  /* HSE: 25 MHz / 8 = 3.125 MHz. HSI fallback: 64 MHz / 8 = 8 MHz. */
  lcd_spi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  lcd_spi.Init.FirstBit = SPI_FIRSTBIT_MSB;
  lcd_spi.Init.TIMode = SPI_TIMODE_DISABLE;
  lcd_spi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  lcd_spi.Init.CRCPolynomial = 0U;
  lcd_spi.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  lcd_spi.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  lcd_spi.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  lcd_spi.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  lcd_spi.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  lcd_spi.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  lcd_spi.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  lcd_spi.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  lcd_spi.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
  lcd_spi.Init.IOSwap = SPI_IO_SWAP_DISABLE;

  if (HAL_SPI_Init(&lcd_spi) != HAL_OK)
  {
    Error_Handler();
  }
}

HAL_StatusTypeDef LCD_SPI_TransmitReceive(const uint8_t *tx, uint8_t *rx,
                                          uint16_t length)
{
  return HAL_SPI_TransmitReceive(&lcd_spi, (uint8_t *)tx, rx, length,
                                 HAL_MAX_DELAY);
}

void LCD_SPI_SetTouchSpeed(uint8_t touch_mode)
{
  /* XPT2046 is specified for a much slower clock than the display.  Change
     only the master baud-rate field while SPI1 is idle.  /32 gives 0.78 MHz
     from HSE and 2 MHz from the HSI fallback. */
  __HAL_SPI_DISABLE(&lcd_spi);
  MODIFY_REG(lcd_spi.Instance->CFG1, SPI_CFG1_MBR,
             touch_mode ? SPI_BAUDRATEPRESCALER_32 : SPI_BAUDRATEPRESCALER_8);
  __HAL_SPI_ENABLE(&lcd_spi);
}

static void LCD_PinHigh(GPIO_TypeDef *port, uint16_t pin)
{
  port->BSRR = pin;
}

static void LCD_PinLow(GPIO_TypeDef *port, uint16_t pin)
{
  port->BSRR = ((uint32_t)pin << 16U);
}

static void LCD_WriteByte(uint8_t data)
{
  if (HAL_SPI_Transmit(&lcd_spi, &data, 1U, HAL_MAX_DELAY) != HAL_OK)
  {
    Error_Handler();
  }
}

static void LCD_WriteCommand(uint8_t command)
{
  LCD_PinLow(LCD_DC_GPIO_Port, LCD_DC_Pin);
  LCD_PinLow(LCD_CS_GPIO_Port, LCD_CS_Pin);
  LCD_WriteByte(command);
  LCD_PinHigh(LCD_CS_GPIO_Port, LCD_CS_Pin);
}

static void LCD_WriteCommandData(uint8_t command, const uint8_t *data, uint32_t length)
{
  LCD_WriteCommand(command);
  LCD_PinHigh(LCD_DC_GPIO_Port, LCD_DC_Pin);
  LCD_PinLow(LCD_CS_GPIO_Port, LCD_CS_Pin);
  while (length-- > 0U)
  {
    LCD_WriteByte(*data++);
  }
  LCD_PinHigh(LCD_CS_GPIO_Port, LCD_CS_Pin);
}

void LCD_BacklightOn(void)
{
  LCD_PinHigh(LCD_BL_GPIO_Port, LCD_BL_Pin);
}

static void LCD_Reset(void)
{
  LCD_BacklightOn();
  LCD_PinHigh(LCD_CS_GPIO_Port, LCD_CS_Pin);
  LCD_PinHigh(LCD_DC_GPIO_Port, LCD_DC_Pin);
  LCD_PinLow(LCD_RST_GPIO_Port, LCD_RST_Pin);
  HAL_Delay(20);
  LCD_PinHigh(LCD_RST_GPIO_Port, LCD_RST_Pin);
  HAL_Delay(120);
}

void LCD_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  uint8_t data[4];

  data[0] = (uint8_t)(x0 >> 8);
  data[1] = (uint8_t)x0;
  data[2] = (uint8_t)(x1 >> 8);
  data[3] = (uint8_t)x1;
  LCD_WriteCommandData(0x2AU, data, sizeof(data));

  data[0] = (uint8_t)(y0 >> 8);
  data[1] = (uint8_t)y0;
  data[2] = (uint8_t)(y1 >> 8);
  data[3] = (uint8_t)y1;
  LCD_WriteCommandData(0x2BU, data, sizeof(data));

  LCD_WriteCommand(0x2CU);
}

void LCD_WritePixels(const uint16_t *pixels, uint32_t count)
{
  LCD_PinHigh(LCD_DC_GPIO_Port, LCD_DC_Pin);
  LCD_PinLow(LCD_CS_GPIO_Port, LCD_CS_Pin);
  while (count-- > 0U)
  {
    uint16_t color = *pixels++;
    LCD_WriteByte((uint8_t)(color >> 8));
    LCD_WriteByte((uint8_t)color);
  }
  LCD_PinHigh(LCD_CS_GPIO_Port, LCD_CS_Pin);
}

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
  uint32_t pixels = (uint32_t)width * height;

  LCD_SetAddressWindow(x, y, (uint16_t)(x + width - 1U), (uint16_t)(y + height - 1U));
  LCD_PinHigh(LCD_DC_GPIO_Port, LCD_DC_Pin);
  LCD_PinLow(LCD_CS_GPIO_Port, LCD_CS_Pin);
  while (pixels-- > 0U)
  {
    LCD_WriteByte((uint8_t)(color >> 8));
    LCD_WriteByte((uint8_t)color);
  }
  LCD_PinHigh(LCD_CS_GPIO_Port, LCD_CS_Pin);
}

void LCD_FillScreen(uint16_t color)
{
  LCD_FillRect(0U, 0U, LCD_WIDTH, LCD_HEIGHT, color);
}

void LCD_Init(void)
{
  static const uint8_t power_b[] = {0x00U, 0xC1U, 0x30U};
  static const uint8_t power_seq[] = {0x64U, 0x03U, 0x12U, 0x81U};
  static const uint8_t driver_timing_a[] = {0x85U, 0x00U, 0x78U};
  static const uint8_t power_a[] = {0x39U, 0x2CU, 0x00U, 0x34U, 0x02U};
  static const uint8_t pump_ratio[] = {0x20U};
  static const uint8_t driver_timing_b[] = {0x00U, 0x00U};
  static const uint8_t power_control_1[] = {0x23U};
  static const uint8_t power_control_2[] = {0x10U};
  static const uint8_t vcom_control_1[] = {0x3EU, 0x28U};
  static const uint8_t vcom_control_2[] = {0x86U};
  static const uint8_t memory_access[] = {0x48U};
  static const uint8_t pixel_format[] = {0x55U};
  static const uint8_t frame_rate[] = {0x00U, 0x18U};
  static const uint8_t display_function[] = {0x08U, 0x82U, 0x27U};
  static const uint8_t gamma_function[] = {0x00U};
  static const uint8_t gamma_curve[] = {0x01U};
  static const uint8_t positive_gamma[] = {
    0x0FU, 0x31U, 0x2BU, 0x0CU, 0x0EU, 0x08U, 0x4EU, 0xF1U,
    0x37U, 0x07U, 0x10U, 0x03U, 0x0EU, 0x09U, 0x00U
  };
  static const uint8_t negative_gamma[] = {
    0x00U, 0x0EU, 0x14U, 0x03U, 0x11U, 0x07U, 0x31U, 0xC1U,
    0x48U, 0x08U, 0x0FU, 0x0CU, 0x31U, 0x36U, 0x0FU
  };

  LCD_SPI_Init();
  LCD_Reset();

  LCD_WriteCommand(0x01U);
  HAL_Delay(120);
  LCD_WriteCommand(0x28U);

  LCD_WriteCommandData(0xCFU, power_b, sizeof(power_b));
  LCD_WriteCommandData(0xEDU, power_seq, sizeof(power_seq));
  LCD_WriteCommandData(0xE8U, driver_timing_a, sizeof(driver_timing_a));
  LCD_WriteCommandData(0xCBU, power_a, sizeof(power_a));
  LCD_WriteCommandData(0xF7U, pump_ratio, sizeof(pump_ratio));
  LCD_WriteCommandData(0xEAU, driver_timing_b, sizeof(driver_timing_b));
  LCD_WriteCommandData(0xC0U, power_control_1, sizeof(power_control_1));
  LCD_WriteCommandData(0xC1U, power_control_2, sizeof(power_control_2));
  LCD_WriteCommandData(0xC5U, vcom_control_1, sizeof(vcom_control_1));
  LCD_WriteCommandData(0xC7U, vcom_control_2, sizeof(vcom_control_2));
  LCD_WriteCommandData(0x36U, memory_access, sizeof(memory_access));
  LCD_WriteCommandData(0x3AU, pixel_format, sizeof(pixel_format));
  LCD_WriteCommandData(0xB1U, frame_rate, sizeof(frame_rate));
  LCD_WriteCommandData(0xB6U, display_function, sizeof(display_function));
  LCD_WriteCommandData(0xF2U, gamma_function, sizeof(gamma_function));
  LCD_WriteCommandData(0x26U, gamma_curve, sizeof(gamma_curve));
  LCD_WriteCommandData(0xE0U, positive_gamma, sizeof(positive_gamma));
  LCD_WriteCommandData(0xE1U, negative_gamma, sizeof(negative_gamma));

  LCD_WriteCommand(0x11U);
  HAL_Delay(120);
  LCD_WriteCommand(0x29U);
  HAL_Delay(20);
}

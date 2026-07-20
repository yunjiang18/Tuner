#include "xpt2046_touch.h"

#include "lcd_ili9341.h"

#define TOUCH_CS_PORT   GPIOB
#define TOUCH_CS_PIN    GPIO_PIN_9
#define TOUCH_IRQ_PORT  GPIOB
#define TOUCH_IRQ_PIN   GPIO_PIN_10
#define LCD_CS_PORT     GPIOB
#define LCD_CS_PIN      GPIO_PIN_6
#define TOUCH_SCK_PORT  GPIOA
#define TOUCH_SCK_PIN   GPIO_PIN_5
#define TOUCH_MISO_PORT GPIOA
#define TOUCH_MISO_PIN  GPIO_PIN_6
#define TOUCH_MOSI_PORT GPIOA
#define TOUCH_MOSI_PIN  GPIO_PIN_7

#define XPT_CMD_X       0xD0U
#define XPT_CMD_Y       0x90U
#define XPT_CMD_Z1      0xB0U
#define XPT_SAMPLES     5U
#define XPT_PRESSURE_MIN 100U

static XPT2046_Diagnostics diagnostics;

static void PinHigh(GPIO_TypeDef *port, uint16_t pin)
{
  port->BSRR = pin;
}

static void PinLow(GPIO_TypeDef *port, uint16_t pin)
{
  port->BSRR = (uint32_t)pin << 16U;
}

static void BusDelay(void)
{
  volatile uint32_t delay;
  for (delay = 0U; delay < 12U; ++delay) { __NOP(); }
}

static void TouchBusEnter(void)
{
  GPIO_InitTypeDef gpio = {0};
  PinLow(TOUCH_SCK_PORT, TOUCH_SCK_PIN);
  PinLow(TOUCH_MOSI_PORT, TOUCH_MOSI_PIN);
  gpio.Pin = TOUCH_SCK_PIN | TOUCH_MOSI_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &gpio);
  gpio.Pin = TOUCH_MISO_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &gpio);
}

static void TouchBusLeave(void)
{
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = TOUCH_SCK_PIN | TOUCH_MISO_PIN | TOUCH_MOSI_PIN;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &gpio);
}

static uint8_t TransferByte(uint8_t transmit)
{
  uint8_t receive = 0U;
  uint32_t bit;
  for (bit = 0U; bit < 8U; ++bit)
  {
    if ((transmit & 0x80U) != 0U) PinHigh(TOUCH_MOSI_PORT, TOUCH_MOSI_PIN);
    else PinLow(TOUCH_MOSI_PORT, TOUCH_MOSI_PIN);
    transmit <<= 1U;
    BusDelay();
    PinHigh(TOUCH_SCK_PORT, TOUCH_SCK_PIN);
    BusDelay();
    receive <<= 1U;
    if ((TOUCH_MISO_PORT->IDR & TOUCH_MISO_PIN) != 0U) receive |= 1U;
    PinLow(TOUCH_SCK_PORT, TOUCH_SCK_PIN);
  }
  return receive;
}

static uint16_t ReadChannel(uint8_t command)
{
  uint16_t value;
  (void)TransferByte(command);
  value = (uint16_t)TransferByte(0U) << 8U;
  value |= TransferByte(0U);
  return value >> 3U;
}

static uint16_t Median5(uint16_t *values)
{
  uint32_t i;
  uint32_t j;
  for (i = 1U; i < XPT_SAMPLES; ++i)
  {
    uint16_t value = values[i];
    j = i;
    while ((j > 0U) && (values[j - 1U] > value))
    {
      values[j] = values[j - 1U];
      --j;
    }
    values[j] = value;
  }
  return values[2];
}

static uint16_t Scale(uint16_t raw, uint16_t minimum, uint16_t maximum,
                      uint16_t pixels)
{
  uint32_t value;
  if (raw <= minimum) return 0U;
  if (raw >= maximum) return (uint16_t)(pixels - 1U);
  value = ((uint32_t)(raw - minimum) * (pixels - 1U)) /
          (uint32_t)(maximum - minimum);
  return (uint16_t)value;
}

void XPT2046_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  PinHigh(TOUCH_CS_PORT, TOUCH_CS_PIN);
  gpio.Pin = TOUCH_CS_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TOUCH_CS_PORT, &gpio);

  gpio.Pin = TOUCH_IRQ_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(TOUCH_IRQ_PORT, &gpio);
  diagnostics.raw_x = 0U;
  diagnostics.raw_y = 0U;
  diagnostics.pressure = 0U;
  diagnostics.reads = 0U;
  diagnostics.irq_level = 1U;
}

uint8_t XPT2046_Read(uint16_t *x, uint16_t *y)
{
  uint16_t raw_x[XPT_SAMPLES];
  uint16_t raw_y[XPT_SAMPLES];
  uint16_t filtered_x;
  uint16_t filtered_y;
  uint32_t i;

  diagnostics.irq_level = (uint8_t)HAL_GPIO_ReadPin(TOUCH_IRQ_PORT,
                                                    TOUCH_IRQ_PIN);
  if ((x == NULL) || (y == NULL))
  {
    return 0U;
  }

  /* Poll pressure instead of depending on PENIRQ.  Some module revisions
     leave T_IRQ high even though coordinate conversion works. */
  PinHigh(LCD_CS_PORT, LCD_CS_PIN);
  TouchBusEnter();
  PinLow(TOUCH_CS_PORT, TOUCH_CS_PIN);
  diagnostics.pressure = ReadChannel(XPT_CMD_Z1);
  if (diagnostics.pressure < XPT_PRESSURE_MIN)
  {
    PinHigh(TOUCH_CS_PORT, TOUCH_CS_PIN);
    TouchBusLeave();
    return 0U;
  }
  (void)ReadChannel(XPT_CMD_X);
  (void)ReadChannel(XPT_CMD_Y);
  for (i = 0U; i < XPT_SAMPLES; ++i)
  {
    raw_x[i] = ReadChannel(XPT_CMD_X);
    raw_y[i] = ReadChannel(XPT_CMD_Y);
  }
  PinHigh(TOUCH_CS_PORT, TOUCH_CS_PIN);
  TouchBusLeave();

  filtered_x = Median5(raw_x);
  filtered_y = Median5(raw_y);
  diagnostics.raw_x = filtered_x;
  diagnostics.raw_y = filtered_y;
  ++diagnostics.reads;
  if ((filtered_x < 50U) || (filtered_y < 50U)) return 0U;

  /* ILI9341 MADCTL=0x48: portrait UI; panel X/Y axes are exchanged. */
  *x = (uint16_t)(LCD_WIDTH - 1U -
       Scale(filtered_y, XPT2046_RAW_Y_MIN, XPT2046_RAW_Y_MAX, LCD_WIDTH));
  *y = Scale(filtered_x, XPT2046_RAW_X_MIN, XPT2046_RAW_X_MAX, LCD_HEIGHT);
  return 1U;
}

const XPT2046_Diagnostics *XPT2046_GetDiagnostics(void)
{
  return &diagnostics;
}

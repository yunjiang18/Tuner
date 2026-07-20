#include "audio_capture.h"

#define AUDIO_ADC_MIDPOINT       32768U
#define AUDIO_CLIP_LOW           512U
#define AUDIO_CLIP_HIGH          65023U
#define AUDIO_WEAK_PEAK_TO_PEAK  256U

static ADC_HandleTypeDef hadc1;
static DMA_HandleTypeDef hdma_adc1;
static TIM_HandleTypeDef htim6;
/* DMA1 cannot access the CM7 DTCM at 0x20000000.  Keep the sample ring in
   AXI SRAM through the dedicated scatter section. */
static uint16_t samples[AUDIO_CAPTURE_BUFFER_SAMPLES]
  __attribute__((section(".audio_dma"), aligned(32)));
static volatile uint8_t ready_mask;
static volatile uint8_t acquired_block = 0xFFU;
static AudioCapture_Diagnostics diagnostics;

static void AnalyzeBlock(uint8_t block)
{
  const uint16_t *data = &samples[(uint32_t)block * AUDIO_CAPTURE_BLOCK_SAMPLES];
  uint16_t minimum = 0xFFFFU;
  uint16_t maximum = 0U;
  uint32_t clipped = 0U;
  uint32_t i;

  for (i = 0U; i < AUDIO_CAPTURE_BLOCK_SAMPLES; ++i)
  {
    uint16_t value = data[i];
    if (value < minimum) minimum = value;
    if (value > maximum) maximum = value;
    if ((value <= AUDIO_CLIP_LOW) || (value >= AUDIO_CLIP_HIGH)) ++clipped;
  }

  diagnostics.minimum = minimum;
  diagnostics.maximum = maximum;
  diagnostics.clipped_samples += clipped;
  if (((uint32_t)maximum - minimum) < AUDIO_WEAK_PEAK_TO_PEAK)
  {
    ++diagnostics.weak_blocks;
  }
}

static void PublishBlock(uint8_t block)
{
  uint8_t bit = (uint8_t)(1U << block);
  AnalyzeBlock(block);
  if ((ready_mask & bit) != 0U) ++diagnostics.dropped_blocks;
  ready_mask |= bit;
  ++diagnostics.completed_blocks;
}

AudioCapture_Error AudioCapture_Init(void)
{
  ADC_ChannelConfTypeDef channel = {0};
  TIM_MasterConfigTypeDef master = {0};
  uint32_t timer_clock = HAL_RCC_GetPCLK1Freq();

  ready_mask = 0U;
  acquired_block = 0xFFU;
  diagnostics.completed_blocks = 0U;
  diagnostics.dropped_blocks = 0U;
  diagnostics.dma_errors = 0U;
  diagnostics.minimum = 0U;
  diagnostics.maximum = 0U;
  diagnostics.clipped_samples = 0U;
  diagnostics.weak_blocks = 0U;
  diagnostics.last_error = AUDIO_CAPTURE_OK;

  if ((timer_clock == 0U) || ((timer_clock % AUDIO_CAPTURE_SAMPLE_RATE_HZ) != 0U))
  {
    diagnostics.last_error = AUDIO_CAPTURE_ERROR_BAD_CLOCK;
    return diagnostics.last_error;
  }

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_ADC12_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_TIM6_CLK_ENABLE();

  {
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);
  }

  hdma_adc1.Instance = DMA1_Stream0;
  hdma_adc1.Init.Request = DMA_REQUEST_ADC1;
  hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
  hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  hdma_adc1.Init.Mode = DMA_CIRCULAR;
  hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;
  hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
  {
    diagnostics.last_error = AUDIO_CAPTURE_ERROR_DMA_INIT;
    return diagnostics.last_error;
  }

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_16B;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T6_TRGO;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc1.Init.OversamplingMode = DISABLE;
  __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    diagnostics.last_error = AUDIO_CAPTURE_ERROR_ADC_INIT;
    return diagnostics.last_error;
  }

  channel.Channel = ADC_CHANNEL_16;
  channel.Rank = ADC_REGULAR_RANK_1;
  channel.SamplingTime = ADC_SAMPLETIME_64CYCLES_5;
  channel.SingleDiff = ADC_SINGLE_ENDED;
  channel.OffsetNumber = ADC_OFFSET_NONE;
  channel.Offset = 0U;
  if (HAL_ADC_ConfigChannel(&hadc1, &channel) != HAL_OK)
  {
    diagnostics.last_error = AUDIO_CAPTURE_ERROR_ADC_INIT;
    return diagnostics.last_error;
  }
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
  {
    diagnostics.last_error = AUDIO_CAPTURE_ERROR_ADC_INIT;
    return diagnostics.last_error;
  }

  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 0U;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = (timer_clock / AUDIO_CAPTURE_SAMPLE_RATE_HZ) - 1U;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    diagnostics.last_error = AUDIO_CAPTURE_ERROR_TIMER_INIT;
    return diagnostics.last_error;
  }
  master.MasterOutputTrigger = TIM_TRGO_UPDATE;
  master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &master) != HAL_OK)
  {
    diagnostics.last_error = AUDIO_CAPTURE_ERROR_TIMER_INIT;
    return diagnostics.last_error;
  }

  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5U, 0U);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  return AUDIO_CAPTURE_OK;
}

AudioCapture_Error AudioCapture_Start(void)
{
  ready_mask = 0U;
  acquired_block = 0xFFU;
  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)samples, AUDIO_CAPTURE_BUFFER_SAMPLES) != HAL_OK)
  {
    diagnostics.last_error = AUDIO_CAPTURE_ERROR_START;
    return diagnostics.last_error;
  }
  if (HAL_TIM_Base_Start(&htim6) != HAL_OK)
  {
    (void)HAL_ADC_Stop_DMA(&hadc1);
    diagnostics.last_error = AUDIO_CAPTURE_ERROR_START;
    return diagnostics.last_error;
  }
  diagnostics.last_error = AUDIO_CAPTURE_OK;
  return AUDIO_CAPTURE_OK;
}

void AudioCapture_Stop(void)
{
  (void)HAL_TIM_Base_Stop(&htim6);
  (void)HAL_ADC_Stop_DMA(&hadc1);
  ready_mask = 0U;
  acquired_block = 0xFFU;
}

const uint16_t *AudioCapture_AcquireBlock(uint32_t *sample_count)
{
  uint8_t block;
  if ((sample_count == NULL) || (acquired_block != 0xFFU)) return NULL;

  __disable_irq();
  if ((ready_mask & 1U) != 0U) block = 0U;
  else if ((ready_mask & 2U) != 0U) block = 1U;
  else block = 0xFFU;
  if (block != 0xFFU)
  {
    ready_mask &= (uint8_t)~(1U << block);
    acquired_block = block;
  }
  __enable_irq();

  if (block == 0xFFU) return NULL;
  *sample_count = AUDIO_CAPTURE_BLOCK_SAMPLES;
  return &samples[(uint32_t)block * AUDIO_CAPTURE_BLOCK_SAMPLES];
}

void AudioCapture_ReleaseBlock(void)
{
  acquired_block = 0xFFU;
}

const AudioCapture_Diagnostics *AudioCapture_GetDiagnostics(void)
{
  return &diagnostics;
}

void AudioCapture_DMA_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_adc1);
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1) PublishBlock(0U);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1) PublishBlock(1U);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1) ++diagnostics.dma_errors;
}

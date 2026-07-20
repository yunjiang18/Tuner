#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include "stm32h7xx_hal.h"

#define AUDIO_CAPTURE_SAMPLE_RATE_HZ 16000U
#define AUDIO_CAPTURE_BUFFER_SAMPLES 4096U
#define AUDIO_CAPTURE_BLOCK_SAMPLES  (AUDIO_CAPTURE_BUFFER_SAMPLES / 2U)

typedef enum
{
  AUDIO_CAPTURE_OK = 0,
  AUDIO_CAPTURE_ERROR_BAD_CLOCK,
  AUDIO_CAPTURE_ERROR_ADC_INIT,
  AUDIO_CAPTURE_ERROR_DMA_INIT,
  AUDIO_CAPTURE_ERROR_TIMER_INIT,
  AUDIO_CAPTURE_ERROR_START
} AudioCapture_Error;

typedef struct
{
  volatile uint32_t completed_blocks;
  volatile uint32_t dropped_blocks;
  volatile uint32_t dma_errors;
  volatile uint16_t minimum;
  volatile uint16_t maximum;
  volatile uint32_t clipped_samples;
  volatile uint32_t weak_blocks;
  volatile AudioCapture_Error last_error;
} AudioCapture_Diagnostics;

AudioCapture_Error AudioCapture_Init(void);
AudioCapture_Error AudioCapture_Start(void);
void AudioCapture_Stop(void);
const uint16_t *AudioCapture_AcquireBlock(uint32_t *sample_count);
void AudioCapture_ReleaseBlock(void);
const AudioCapture_Diagnostics *AudioCapture_GetDiagnostics(void);
void AudioCapture_DMA_IRQHandler(void);

#endif

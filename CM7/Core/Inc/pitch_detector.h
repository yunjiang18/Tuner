#ifndef PITCH_DETECTOR_H
#define PITCH_DETECTOR_H

#include <stdint.h>

typedef struct
{
  float frequency_hz;
  float confidence;
  uint8_t valid;
} PitchDetector_Result;

void PitchDetector_Estimate(const uint16_t *samples, uint32_t count,
                            float target_hz, PitchDetector_Result *result);

#endif

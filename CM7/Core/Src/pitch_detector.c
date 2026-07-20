#include "pitch_detector.h"

#include "audio_capture.h"

#define YIN_MAX_LAG       240U
#define YIN_THRESHOLD     0.15f
#define MIN_PEAK_TO_PEAK  256U

static float cmnd[YIN_MAX_LAG + 1U];

void PitchDetector_Estimate(const uint16_t *samples, uint32_t count,
                            float target_hz, PitchDetector_Result *result)
{
  uint16_t minimum = 0xFFFFU;
  uint16_t maximum = 0U;
  uint32_t min_lag;
  uint32_t max_lag;
  uint32_t tau;
  uint32_t i;
  float running_sum = 0.0f;
  uint32_t best_tau = 0U;
  float best_value = 1.0f;

  if (result == 0) return;
  result->frequency_hz = 0.0f;
  result->confidence = 0.0f;
  result->valid = 0U;
  if ((samples == 0) || (count < 512U) || (target_hz < 50.0f)) return;

  for (i = 0U; i < count; ++i)
  {
    if (samples[i] < minimum) minimum = samples[i];
    if (samples[i] > maximum) maximum = samples[i];
  }
  if (((uint32_t)maximum - minimum) < MIN_PEAK_TO_PEAK) return;

  /* Limit detection to +/- one semitone around the explicitly selected
     string. This rejects most sympathetic-string and octave errors. */
  min_lag = (uint32_t)((float)AUDIO_CAPTURE_SAMPLE_RATE_HZ /
                       (target_hz * 1.0594631f));
  max_lag = (uint32_t)(((float)AUDIO_CAPTURE_SAMPLE_RATE_HZ /
                       (target_hz / 1.0594631f)) + 1.0f);
  if (min_lag < 2U) min_lag = 2U;
  if (max_lag > YIN_MAX_LAG) max_lag = YIN_MAX_LAG;
  if ((max_lag >= count) || (min_lag >= max_lag)) return;

  cmnd[0] = 1.0f;
  for (tau = 1U; tau <= max_lag; ++tau)
  {
    float sum = 0.0f;
    uint32_t limit = count - max_lag;
    for (i = 0U; i < limit; ++i)
    {
      float delta = (float)((int32_t)samples[i] - (int32_t)samples[i + tau]);
      sum += delta * delta;
    }
    running_sum += sum;
    cmnd[tau] = (running_sum > 0.0f) ?
                (sum * (float)tau / running_sum) : 1.0f;
  }

  for (tau = min_lag; tau <= max_lag; ++tau)
  {
    if (cmnd[tau] < best_value)
    {
      best_value = cmnd[tau];
      best_tau = tau;
    }
    if ((cmnd[tau] < YIN_THRESHOLD) &&
        ((tau == max_lag) || (cmnd[tau + 1U] >= cmnd[tau])))
    {
      best_tau = tau;
      best_value = cmnd[tau];
      break;
    }
  }

  if ((best_tau != 0U) && (best_value < YIN_THRESHOLD))
  {
    float refined_tau = (float)best_tau;
    if ((best_tau > min_lag) && (best_tau < max_lag))
    {
      float left = cmnd[best_tau - 1U];
      float center = cmnd[best_tau];
      float right = cmnd[best_tau + 1U];
      float denominator = 2.0f * (2.0f * center - right - left);
      if ((denominator > 0.000001f) || (denominator < -0.000001f))
      {
        refined_tau += (right - left) / denominator;
      }
    }
    result->frequency_hz = (float)AUDIO_CAPTURE_SAMPLE_RATE_HZ / refined_tau;
    result->confidence = 1.0f - best_value;
    result->valid = 1U;
  }
}

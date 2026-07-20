#include "tuner_ui.h"

#include "audio_capture.h"
#include "pitch_detector.h"
#include "lvgl.h"
#include <math.h>

typedef struct
{
  uint8_t string_number;
  const char *note;
  uint32_t frequency_x100;
} GuzhengTarget;

/* D tuning, A4=440 Hz. Standard guzheng numbering:
   string 1 is the highest D6 and string 21 is the lowest D2. */
static const GuzhengTarget d_tuning_targets[] = {
  {21U, "D2", 7342U},  {20U, "E2", 8241U},  {19U, "F#2", 9250U},
  {18U, "A2", 11000U}, {17U, "B2", 12347U}, {16U, "D3", 14683U},
  {15U, "E3", 16481U}, {14U, "F#3", 18500U},{13U, "A3", 22000U},
  {12U, "B3", 24694U}, {11U, "D4", 29366U}, {10U, "E4", 32963U},
  {9U, "F#4", 36999U}, {8U, "A4", 44000U},  {7U, "B4", 49388U},
  {6U, "D5", 58733U},  {5U, "E5", 65926U},  {4U, "F#5", 73999U},
  {3U, "A5", 88000U},  {2U, "B5", 98777U},  {1U, "D6", 117466U}
};

static lv_obj_t *string_value_label;
static lv_obj_t *target_value_label;
static lv_obj_t *measured_value_label;
static lv_obj_t *cents_value_label;
static lv_obj_t *direction_label;
static lv_obj_t *capture_label;
static lv_obj_t *level_label;
static uint8_t selected_target = 7U; /* String 14 / F#3. */
static uint8_t result_valid;
static float measured_hz;
static float measured_cents;
static uint32_t last_refresh_ms;

static void SetColor(lv_obj_t *object, lv_color_t color)
{
  lv_obj_set_style_text_color(object, color, 0);
}

static lv_obj_t *CreateCaption(lv_obj_t *screen, const char *text, lv_coord_t y)
{
  lv_obj_t *label = lv_label_create(screen);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  SetColor(label, lv_color_hex(0x8CA3B8));
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, 15, y);
  return label;
}

static lv_obj_t *CreateValue(lv_obj_t *screen, lv_coord_t y)
{
  lv_obj_t *label = lv_label_create(screen);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
  SetColor(label, lv_color_hex(0xF2F7FA));
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, 82, y - 2);
  return label;
}

static void UpdateTargetText(void)
{
  const GuzhengTarget *target = &d_tuning_targets[selected_target];
  char text[40];

  lv_snprintf(text, sizeof(text), "%02u", target->string_number);
  lv_label_set_text(string_value_label, text);
  lv_snprintf(text, sizeof(text), "%s   %lu.%02lu Hz", target->note,
              (unsigned long)(target->frequency_x100 / 100U),
              (unsigned long)(target->frequency_x100 % 100U));
  lv_label_set_text(target_value_label, text);
}

static void UpdatePitchText(void)
{
  char text[40];
  int32_t frequency_x100;
  int32_t cents_x10;

  if (!result_valid)
  {
    lv_label_set_text(measured_value_label, "--.-- Hz");
    lv_label_set_text(cents_value_label, "--.- cent");
    lv_label_set_text(direction_label, "PLUCK THE SELECTED STRING");
    SetColor(direction_label, lv_color_hex(0x8CA3B8));
    return;
  }

  frequency_x100 = (int32_t)(measured_hz * 100.0f + 0.5f);
  cents_x10 = (int32_t)(measured_cents * 10.0f +
                        ((measured_cents >= 0.0f) ? 0.5f : -0.5f));
  lv_snprintf(text, sizeof(text), "%ld.%02ld Hz",
              (long)(frequency_x100 / 100),
              (long)(frequency_x100 % 100));
  lv_label_set_text(measured_value_label, text);
  lv_snprintf(text, sizeof(text), "%s%ld.%01ld cent",
              (cents_x10 >= 0) ? "+" : "-",
              (long)((cents_x10 >= 0 ? cents_x10 : -cents_x10) / 10),
              (long)((cents_x10 >= 0 ? cents_x10 : -cents_x10) % 10));
  lv_label_set_text(cents_value_label, text);

  if (measured_cents < -2.0f)
  {
    lv_label_set_text(direction_label, "FLAT  -  TIGHTEN STRING");
    SetColor(direction_label, lv_palette_main(LV_PALETTE_RED));
  }
  else if (measured_cents > 2.0f)
  {
    lv_label_set_text(direction_label, "SHARP  -  LOOSEN STRING");
    SetColor(direction_label, lv_palette_main(LV_PALETTE_ORANGE));
  }
  else
  {
    lv_label_set_text(direction_label, "IN TUNE");
    SetColor(direction_label, lv_palette_main(LV_PALETTE_GREEN));
  }
}

void TunerUI_Init(void)
{
  lv_obj_t *screen = lv_scr_act();
  lv_obj_t *title;
  lv_obj_t *reference;

  lv_obj_set_style_bg_color(screen, lv_color_hex(0x101418), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  title = lv_label_create(screen);
  lv_label_set_text(title, "GUZHENG TUNER");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  SetColor(title, lv_color_hex(0xEAF2F8));
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

  reference = lv_label_create(screen);
  lv_label_set_text(reference, "D TUNING  |  A4=440 Hz");
  SetColor(reference, lv_palette_main(LV_PALETTE_CYAN));
  lv_obj_align(reference, LV_ALIGN_TOP_MID, 0, 34);

  CreateCaption(screen, "STRING", 64);
  string_value_label = CreateValue(screen, 64);
  CreateCaption(screen, "TARGET", 100);
  target_value_label = CreateValue(screen, 100);
  CreateCaption(screen, "MEASURE", 136);
  measured_value_label = CreateValue(screen, 136);
  CreateCaption(screen, "ERROR", 172);
  cents_value_label = CreateValue(screen, 172);

  direction_label = lv_label_create(screen);
  lv_obj_set_style_text_font(direction_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(direction_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(direction_label, 220);
  lv_obj_align(direction_label, LV_ALIGN_TOP_MID, 0, 215);

  capture_label = lv_label_create(screen);
  lv_obj_set_style_text_font(capture_label, &lv_font_montserrat_14, 0);
  SetColor(capture_label, lv_color_hex(0x8CA3B8));
  lv_obj_align(capture_label, LV_ALIGN_BOTTOM_MID, 0, -38);

  level_label = lv_label_create(screen);
  SetColor(level_label, lv_color_hex(0x75899A));
  lv_obj_align(level_label, LV_ALIGN_BOTTOM_MID, 0, -14);

  UpdateTargetText();
  UpdatePitchText();
}

void TunerUI_Task(void)
{
  const AudioCapture_Diagnostics *diagnostics;
  const uint16_t *block;
  uint32_t count;
  uint32_t now = lv_tick_get();
  char text[48];

  /* Consume each DMA half promptly. Pitch processing will replace this release
     point; display updates remain limited to 5 Hz. */
  do
  {
    block = AudioCapture_AcquireBlock(&count);
    if (block != NULL)
    {
      PitchDetector_Result pitch;
      float target_hz = (float)d_tuning_targets[selected_target].frequency_x100 / 100.0f;
      PitchDetector_Estimate(block, count, target_hz, &pitch);
      if (pitch.valid)
      {
        float cents = 1200.0f * (logf(pitch.frequency_hz / target_hz) / 0.69314718056f);
        TunerUI_SetPitchResult(pitch.frequency_hz, cents, pitch.confidence, 1U);
      }
      else
      {
        TunerUI_SetPitchResult(0.0f, 0.0f, 0.0f, 0U);
      }
      AudioCapture_ReleaseBlock();
    }
  } while (block != NULL);

  if ((now - last_refresh_ms) < 200U) return;
  last_refresh_ms = now;
  diagnostics = AudioCapture_GetDiagnostics();

  if (diagnostics->last_error != AUDIO_CAPTURE_OK)
  {
    lv_snprintf(text, sizeof(text), "CAPTURE ERROR %u", (unsigned int)diagnostics->last_error);
    SetColor(capture_label, lv_palette_main(LV_PALETTE_RED));
  }
  else
  {
    lv_snprintf(text, sizeof(text), "AUDIO OK   B:%lu  D:%lu  E:%lu",
                (unsigned long)diagnostics->completed_blocks,
                (unsigned long)diagnostics->dropped_blocks,
                (unsigned long)diagnostics->dma_errors);
    SetColor(capture_label, lv_palette_main(LV_PALETTE_GREEN));
  }
  lv_label_set_text(capture_label, text);

  lv_snprintf(text, sizeof(text), "LEVEL:%u   BIAS:%u.%02u V",
              (unsigned int)((uint32_t)diagnostics->maximum - diagnostics->minimum),
              (unsigned int)(((uint32_t)(diagnostics->maximum + diagnostics->minimum) * 165U) /
                             65535U / 100U),
              (unsigned int)(((uint32_t)(diagnostics->maximum + diagnostics->minimum) * 165U) /
                             65535U % 100U));
  lv_label_set_text(level_label, text);
  UpdatePitchText();
}

void TunerUI_SelectString(uint8_t string_number)
{
  if ((string_number < 1U) || (string_number > 21U)) return;
  selected_target = (uint8_t)(21U - string_number);
  result_valid = 0U;
  UpdateTargetText();
}

void TunerUI_SetPitchResult(float frequency_hz, float cents, float result_confidence,
                            uint8_t valid)
{
  measured_hz = frequency_hz;
  measured_cents = cents;
  (void)result_confidence;
  result_valid = valid;
}

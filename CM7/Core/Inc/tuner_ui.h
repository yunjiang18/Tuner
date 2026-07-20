#ifndef TUNER_UI_H
#define TUNER_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void TunerUI_Init(void);
void TunerUI_Task(void);
void TunerUI_SelectString(uint8_t string_number);
void TunerUI_SetPitchResult(float frequency_hz, float cents, float confidence,
                            uint8_t valid);

#ifdef __cplusplus
}
#endif

#endif /* TUNER_UI_H */

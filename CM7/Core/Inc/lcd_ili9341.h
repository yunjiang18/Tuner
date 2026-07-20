#ifndef LCD_ILI9341_H
#define LCD_ILI9341_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define LCD_WIDTH 240U
#define LCD_HEIGHT 320U

void LCD_Init(void);
void LCD_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_WritePixels(const uint16_t *pixels, uint32_t count);
void LCD_FillScreen(uint16_t color);
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void LCD_BacklightOn(void);

#ifdef __cplusplus
}
#endif

#endif /* LCD_ILI9341_H */

#ifndef __DEV__CONSOLE_H__
#define __DEV__CONSOLE_H__

#include <stdint.h>
#include <stdbool.h>

bool console_init(uint32_t *_fb,
                  int _fb_width,
                  int _fb_height,
                  int _fb_pitch,
                  uint8_t *_font,
                  int _font_width,
                  int _font_height);

#endif

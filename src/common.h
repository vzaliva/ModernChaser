#ifndef common_h
#define common_h

#include "pebble.h"

#undef INVERSE
#undef SHOW_SECONDS

#define FULL_W 144
#define FULL_H 168
#define GRECT_FULL_WINDOW GRect(0,0,FULL_W,FULL_H)

void conserve_power(bool conserve);

#endif

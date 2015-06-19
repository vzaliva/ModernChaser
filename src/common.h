#ifndef common_h
#define common_h

#include "pebble.h"

#undef INVERSE
#undef SHOW_SECONDS

#define GRECT_FULL_WINDOW GRect(0,0,144,168)

void conserve_power(bool conserve);

#endif

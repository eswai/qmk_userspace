#pragma once

#define OLED_UPDATE_INTERVAL 100
#define OLED_TIMEOUT 60000

// https://docs.qmk.fm/#/squeezing_avr
#undef LOCKING_SUPPORT_ENABLE
#undef LOCKING_RESYNC_ENABLE
#define NO_ACTION_ONESHOT
// #define NO_ACTION_TAPPING
#define LAYER_STATE_8BIT
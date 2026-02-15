
#pragma once

#include "action.h"
#include "quantum_keycodes.h"

#include "nicola.h"

#define JU_LBRC KC_RBRACKET
#define JU_RBRC KC_NONUS_HASH
#define JU_BSLS KC_INT3

enum jtu_keycodes {
  JU_2 = NG_SAFE_RANGE,
  JU_6,
  JU_7,
  JU_8,
  JU_9,
  JU_0,
  JU_MINS,
  JU_EQL,
  JU_SCLN,
  JU_QUOT,
  JU_GRV,
  JTU_SAFE_RANGE
};

bool process_jtu(uint16_t keycode, keyrecord_t *record);

#include QMK_KEYBOARD_H
#include "jtu.h"

enum jtu_jis_key {
    JU_J_2,
    JU_J_6,
    JU_J_7,
    JU_J_8,
    JU_J_9,
    JU_J_0,
    JU_J_MINS,
    JU_J_EQL,
    JU_J_INT3,
    JU_J_LBRC,
    JU_J_SCLN,
    JU_J_QUOT,
    JU_J_INT1,
    JU_N_J
};
static bool jtu_pressed[JU_N_J];

static bool rshift_to_unreg = false;
static bool lshift_to_reg = false;
static bool rshift_to_reg = false;

#define UNREG_SHIFT() \
          if (lshift) { unregister_code(KC_LSFT); lshift_to_reg = true; } \
          if (rshift) { unregister_code(KC_RSFT); rshift_to_reg = true; }

#define CLOSE_UNREG_SHIFT() \
          if (lshift_to_reg) { register_code(KC_LSFT); lshift_to_reg = false; } \
          if (rshift_to_reg) { register_code(KC_RSFT); rshift_to_reg = false; }

#define REG_SHIFT() \
          { register_code(KC_RSFT); rshift_to_unreg = true; }

#define CLOSE_REG_SHIFT() \
          if (rshift_to_unreg) { unregister_code(KC_RSFT); rshift_to_unreg = false; }

#define JTU_PRESS(x) \
          if (jtu_pressed[JU_J_ ## x]) unregister_code(KC_ ## x); \
          register_code(KC_ ## x); \
          jtu_pressed[JU_J_ ## x] = true \

#define JTU_RELEASE(x) \
          if (jtu_pressed[JU_J_ ## x]) unregister_code(KC_ ## x); \
          jtu_pressed[JU_J_ ## x] = false \

enum jtu_ansi_key {
    JU_A_2,
    JU_A_6,
    JU_A_7,
    JU_A_8,
    JU_A_9,
    JU_A_0,
    JU_A_MINS,
    JU_A_EQL,
    JU_A_BSLS,
    JU_A_SCLN,
    JU_A_QUOT,
    JU_A_GRV,
    JU_N_A
};
static bool jtu_a_pressed_nshift[JU_N_A];
static bool jtu_a_pressed_shift[JU_N_A];

bool process_jtu(uint16_t keycode, keyrecord_t *record) {
  static bool lshift = false;
  static bool rshift = false;

  switch (keycode) {
    case KC_LSFT:
      if (record->event.pressed) {
        CLOSE_REG_SHIFT();
        lshift = true;
      } else {
        CLOSE_UNREG_SHIFT();
        lshift = false;
      }
      break;
    case KC_RSFT:
      if (record->event.pressed) {
        CLOSE_REG_SHIFT();
        rshift = true;
      } else {
        CLOSE_UNREG_SHIFT();
        rshift = false;
      }
      break;
    case JU_2:
      if (record->event.pressed) {
        if (lshift || rshift) {
          jtu_a_pressed_shift[JU_A_2] = true;
          UNREG_SHIFT();
          JTU_PRESS(LBRC);
        } else {
          jtu_a_pressed_nshift[JU_A_2] = true;
          CLOSE_REG_SHIFT();
          JTU_PRESS(2);
        }
      } else {
          if(jtu_a_pressed_shift[JU_A_2]) {
            jtu_a_pressed_shift[JU_A_2] = false;
            JTU_RELEASE(LBRC);
            CLOSE_UNREG_SHIFT();
          }
          if(jtu_a_pressed_nshift[JU_A_2]) {
            jtu_a_pressed_nshift[JU_A_2] = false;
            JTU_RELEASE(2);
          }
      }
      return false;
    case JU_6:
      if (record->event.pressed) {
        if (lshift || rshift) {
          jtu_a_pressed_shift[JU_A_6] = true;
          UNREG_SHIFT();
          JTU_PRESS(EQL);
        } else {
          jtu_a_pressed_nshift[JU_A_6] = true;
          CLOSE_REG_SHIFT();
          JTU_PRESS(6);
        }
      } else {
          if(jtu_a_pressed_shift[JU_A_6]) {
            jtu_a_pressed_shift[JU_A_6] = false;
            JTU_RELEASE(EQL);
            CLOSE_UNREG_SHIFT();
          }
          if(jtu_a_pressed_nshift[JU_A_6]) {
            jtu_a_pressed_nshift[JU_A_6] = false;
            JTU_RELEASE(6);
          }
      }
      return false;
    case JU_7:
      if (record->event.pressed) {
        if (lshift || rshift) {
          jtu_a_pressed_shift[JU_A_7] = true;
          CLOSE_UNREG_SHIFT();
          JTU_PRESS(6);
        } else {
          jtu_a_pressed_nshift[JU_A_7] = true;
          CLOSE_REG_SHIFT();
          JTU_PRESS(7);
        }
      } else {
          if(jtu_a_pressed_shift[JU_A_7]) {
            jtu_a_pressed_shift[JU_A_7] = false;
            JTU_RELEASE(6);
          }
          if(jtu_a_pressed_nshift[JU_A_7]) {
            jtu_a_pressed_nshift[JU_A_7] = false;
            JTU_RELEASE(7);
          }
      }
      return false;
    case JU_8:
      if (record->event.pressed) {
        if (lshift || rshift) {
          jtu_a_pressed_shift[JU_A_8] = true;
          CLOSE_UNREG_SHIFT();
          JTU_PRESS(QUOT);
        } else {
          jtu_a_pressed_nshift[JU_A_8] = true;
          CLOSE_REG_SHIFT();
          JTU_PRESS(8);
        }
      } else {
          if(jtu_a_pressed_shift[JU_A_8]) {
            jtu_a_pressed_shift[JU_A_8] = false;
            JTU_RELEASE(QUOT);
          }
          if(jtu_a_pressed_nshift[JU_A_8]) {
            jtu_a_pressed_nshift[JU_A_8] = false;
            JTU_RELEASE(8);
          }
      }
      return false;
    case JU_9:
      if (record->event.pressed) {
        if (lshift || rshift) {
          jtu_a_pressed_shift[JU_A_9] = true;
          CLOSE_UNREG_SHIFT();
          JTU_PRESS(8);
        } else {
          jtu_a_pressed_nshift[JU_A_9] = true;
          CLOSE_REG_SHIFT();
          JTU_PRESS(9);
        }
      } else {
          if(jtu_a_pressed_shift[JU_A_9]) {
            jtu_a_pressed_shift[JU_A_9] = false;
            JTU_RELEASE(8);
          }
          if(jtu_a_pressed_nshift[JU_A_9]) {
            jtu_a_pressed_nshift[JU_A_9] = false;
            JTU_RELEASE(9);
          }
      }
      return false;
    case JU_0:
      if (record->event.pressed) {
        if (lshift || rshift) {
          jtu_a_pressed_shift[JU_A_0] = true;
          CLOSE_UNREG_SHIFT();
          JTU_PRESS(9);
        } else {
          jtu_a_pressed_nshift[JU_A_0] = true;
          CLOSE_REG_SHIFT();
          JTU_PRESS(0);
        }
      } else {
          if(jtu_a_pressed_shift[JU_A_0]) {
            jtu_a_pressed_shift[JU_A_0] = false;
            JTU_RELEASE(9);
          }
          if(jtu_a_pressed_nshift[JU_A_0]) {
            jtu_a_pressed_nshift[JU_A_0] = false;
            JTU_RELEASE(0);
          }
      }
      return false;
    case JU_MINS:
      if (record->event.pressed) {
        if (lshift || rshift) {
          jtu_a_pressed_shift[JU_A_MINS] = true;
          CLOSE_UNREG_SHIFT();
          JTU_PRESS(INT1);
        } else {
          jtu_a_pressed_nshift[JU_A_MINS] = true;
          CLOSE_REG_SHIFT();
          JTU_PRESS(MINS);
        }
      } else {
          if(jtu_a_pressed_shift[JU_A_MINS]) {
            jtu_a_pressed_shift[JU_A_MINS] = false;
            JTU_RELEASE(INT1);
          }
          if(jtu_a_pressed_nshift[JU_A_MINS]) {
            jtu_a_pressed_nshift[JU_A_MINS] = false;
            JTU_RELEASE(MINS);
          }
      }
      return false;
    case JU_EQL:
      if (record->event.pressed) {
        if (lshift || rshift) {
          jtu_a_pressed_shift[JU_A_EQL] = true;
          CLOSE_UNREG_SHIFT();
          JTU_PRESS(SCLN);
        } else {
          jtu_a_pressed_nshift[JU_A_EQL] = true;
          REG_SHIFT();
          JTU_PRESS(MINS);
        }
      } else {
          if(jtu_a_pressed_shift[JU_A_EQL]) {
            jtu_a_pressed_shift[JU_A_EQL] = false;
            JTU_RELEASE(SCLN);
          }
          if(jtu_a_pressed_nshift[JU_A_EQL]) {
            jtu_a_pressed_nshift[JU_A_EQL] = false;
            JTU_RELEASE(MINS);
            CLOSE_REG_SHIFT();
          }
      }
      return false;
    case JU_SCLN:
      if (record->event.pressed) {
        if (lshift || rshift) {
          jtu_a_pressed_shift[JU_A_SCLN] = true;
          UNREG_SHIFT();
          JTU_PRESS(QUOT);
        } else {
          jtu_a_pressed_nshift[JU_A_SCLN] = true;
          CLOSE_REG_SHIFT();
          JTU_PRESS(SCLN);
        }
      } else {
          if(jtu_a_pressed_shift[JU_A_SCLN]) {
            jtu_a_pressed_shift[JU_A_SCLN] = false;
            JTU_RELEASE(QUOT);
            CLOSE_UNREG_SHIFT();
          }
          if(jtu_a_pressed_nshift[JU_A_SCLN]) {
            jtu_a_pressed_nshift[JU_A_SCLN] = false;
            JTU_RELEASE(SCLN);
          }
      }
      return false;
    case JU_QUOT:
      if (record->event.pressed) {
        if (lshift || rshift) {
          jtu_a_pressed_shift[JU_A_QUOT] = true;
          CLOSE_UNREG_SHIFT();
          JTU_PRESS(2);
        } else {
          jtu_a_pressed_nshift[JU_A_QUOT] = true;
          REG_SHIFT();
          JTU_PRESS(7);
        }
      } else {
          if(jtu_a_pressed_shift[JU_A_QUOT]) {
            jtu_a_pressed_shift[JU_A_QUOT] = false;
            JTU_RELEASE(2);
          }
          if(jtu_a_pressed_nshift[JU_A_QUOT]) {
            jtu_a_pressed_nshift[JU_A_QUOT] = false;
            JTU_RELEASE(7);
            CLOSE_REG_SHIFT();
          }
      }
      return false;
    case JU_GRV:
      if (record->event.pressed) {
        if (lshift || rshift) {
          jtu_a_pressed_shift[JU_A_GRV] = true;
          CLOSE_UNREG_SHIFT();
          JTU_PRESS(EQL);
        } else {
          jtu_a_pressed_nshift[JU_A_GRV] = true;
          REG_SHIFT();
          JTU_PRESS(LBRC);
        }
      } else {
          if(jtu_a_pressed_shift[JU_A_GRV]) {
            jtu_a_pressed_shift[JU_A_GRV] = false;
            JTU_RELEASE(EQL);
          }
          if(jtu_a_pressed_nshift[JU_A_GRV]) {
            jtu_a_pressed_nshift[JU_A_GRV] = false;
            JTU_RELEASE(LBRC);
            CLOSE_REG_SHIFT();
          }
      }
      return false;
    default:
      CLOSE_REG_SHIFT();
      CLOSE_UNREG_SHIFT();
  }
  return true;
}

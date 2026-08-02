/* Copyright 2020 eswai
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include QMK_KEYBOARD_H

// Defines names for use in layer keycodes and the keymap
enum layer_names {
    _BASE,
    _LOWER,
    _RAISE,
};

// Defines the keycodes used by our macros in process_record_user
enum custom_keycodes {
    QMKBEST = SAFE_RANGE,
    QMKURL
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [_BASE] = LAYOUT(
    KC_TAB,        KC_Q,   KC_W,   KC_E,    KC_R,   KC_T,   G(KC_X), G(KC_Z), KC_Y,   KC_U,   KC_I,    KC_O,   KC_P,    KC_BSPC,
    WIN_T(KC_ESC), KC_A,   KC_S,   KC_D,    KC_F,   KC_G,   G(KC_C), G(KC_S), KC_H,   KC_J,   KC_K,    KC_L,   KC_SCLN, KC_QUOT,
    KC_LSFT,       KC_Z,   KC_X,   KC_C,    KC_V,   KC_B,   G(KC_V), G(KC_Y), KC_N,   KC_M,   KC_COMM, KC_DOT, KC_SLSH, KC_RSFT,
    KC_LCTL,       KC_LWIN,KC_LALT,KC_LCTL, MO(_LOWER),     KC_SPC,  KC_ENT,          MO(_RAISE),KC_LEFT,KC_DOWN, KC_UP,   KC_RGHT
  ),

  [_LOWER] = LAYOUT(
    _______, XXXXXXX, XXXXXXX, KC_COLN, KC_SCLN, XXXXXXX, XXXXXXX, XXXXXXX, KC_SLSH, KC_7,    KC_8,    KC_9,    KC_MINS, KC_DEL,
    _______, XXXXXXX, KC_LBRC, KC_LCBR, KC_LPRN, KC_LT,   XXXXXXX, XXXXXXX, KC_ASTR, KC_4,    KC_5,    KC_6,    KC_PLUS, _______,
    _______, XXXXXXX, KC_RBRC, KC_RCBR, KC_RPRN, KC_GT,   XXXXXXX, XXXXXXX, KC_0,    KC_1,    KC_2,    KC_3,    KC_EQL,  _______,
    QK_BOOT,  _______, _______, _______, _______,          _______, _______,          _______, _______, _______, _______, _______
  ),

  [_RAISE] = LAYOUT(
    _______, KC_TILD, KC_AT,   KC_HASH, KC_DLR,  XXXXXXX,         XXXXXXX, XXXXXXX, XXXXXXX, KC_HOME, KC_UP,   KC_END,  XXXXXXX, KC_DEL,
    _______, KC_CIRC, KC_AMPR, KC_QUES, KC_PERC, KC_INT3,         XXXXXXX, XXXXXXX, XXXXXXX, KC_LEFT, KC_DOWN, KC_RGHT, XXXXXXX, XXXXXXX,
    _______, KC_GRV,  KC_PIPE, KC_EXLM, KC_UNDS, LALT(KC_INT3),   XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, _______,
    _______, _______, _______, _______, _______,                  _______, _______,          _______, _______, _______, _______, _______
  )
};

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  oled_task_user();
  return true;
}

void matrix_init_user() {
}

#ifdef OLED_ENABLE
oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    if (is_keyboard_master()) {
        return OLED_ROTATION_180;  // flips the display 180 degrees if offhand
    }
    return rotation;
}

static void render_logo(void) {
    static const char PROGMEM qmk_logo[] = {
      0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0x0
    };
    oled_write_P(qmk_logo, false);
}

bool oled_task_user(void) {
    if (is_keyboard_master()) {
        render_logo();
    } else {
        render_logo();  // Renders a static logo
        // oled_scroll_left();  // Turns on scrolling
    }
    return false;
}
#endif
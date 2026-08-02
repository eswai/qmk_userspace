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
// #include "twpair_on_jis.h"
#include "keymap_japanese.h"

// Defines names for use in layer keycodes and the keymap
enum layer_names {
  _WIN,
  _LOWER,
  _RAISE,
  _ADJUST,
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [_WIN] = LAYOUT(
//            ,         ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,
	  	KC_GRV  ,KC_1     ,KC_2    ,KC_3    ,KC_4    ,KC_5    ,KC_6    ,KC_7    ,KC_8    ,KC_9    ,KC_0    ,JP_MINS ,JP_CIRC  ,JP_YEN ,KC_DEL  , \
		  KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    JP_AT, KC_BSPC,
  		KC_ESC,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, JP_COLN, KC_ENT,
  		KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, JP_LBRC, KC_UP,   JP_RBRC,
		  KC_LCTL ,KC_INT4  ,MO(_LOWER)       ,KC_SPC          ,KC_ENT   ,MO(_RAISE)              ,KC_LALT ,KC_LEFT ,KC_DOWN ,KC_RGHT
  ),

  [_LOWER] = LAYOUT(
//            ,         ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,
	    XXXXXXX ,KC_F1    ,KC_F2   ,KC_F3   ,KC_F4   ,KC_F5   ,KC_F6   ,KC_F7   ,KC_F8   ,KC_F9   ,KC_F10  ,KC_F11  ,KC_F12  ,KC_HOME ,KC_END , \
      XXXXXXX ,XXXXXXX  ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,KC_SLSH ,KC_7    ,KC_8    ,KC_9    ,KC_MINS ,XXXXXXX ,KC_PIPE , \
   LWIN(KC_E) ,XXXXXXX  ,KC_LBRC ,KC_LCBR ,KC_LPRN ,KC_LT   ,KC_ASTR ,KC_4    ,KC_5    ,KC_6    ,KC_PLUS ,XXXXXXX ,XXXXXXX , \
      XXXXXXX ,XXXXXXX  ,KC_RBRC ,KC_RCBR ,KC_RPRN ,KC_GT   ,KC_0    ,KC_1    ,KC_2    ,KC_3    ,KC_EQL  ,XXXXXXX ,KC_PGUP ,XXXXXXX , \
		  _______ ,_______  ,_______          ,_______          ,_______         ,_______           ,_______ ,KC_HOME ,KC_PGDN ,KC_END
  ),

  [_RAISE] = LAYOUT(
//            ,         ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,
	    XXXXXXX ,KC_F1    ,KC_F2   ,KC_F3   ,KC_F4   ,KC_F5   ,KC_F6   ,KC_F7   ,KC_F8   ,KC_F9   ,KC_F10  ,KC_F11  ,KC_F12  ,KC_PSCR ,KC_INS  , \
      XXXXXXX ,KC_TILD  ,KC_AT   ,KC_HASH ,KC_DLR  ,XXXXXXX ,XXXXXXX ,XXXXXXX ,KC_HOME ,KC_UP   ,KC_END  ,KC_DEL  ,KC_PIPE , \
      XXXXXXX ,KC_CIRC  ,KC_AMPR ,KC_QUES ,KC_PERC ,KC_INT3  ,XXXXXXX ,XXXXXXX ,KC_LEFT ,KC_DOWN ,KC_RGHT ,XXXXXXX ,XXXXXXX , \
      XXXXXXX ,KC_GRV   ,KC_PIPE ,KC_EXLM ,KC_UNDS ,LALT(KC_INT3),XXXXXXX,XXXXXXX,XXXXXXX,XXXXXXX,XXXXXXX,XXXXXXX ,KC_PGUP ,XXXXXXX , \
	    _______ ,_______   ,_______          ,_______          ,_______         ,_______          ,_______ ,XXXXXXX ,KC_PGDN ,XXXXXXX
  ),

  [_ADJUST] = LAYOUT(
//            ,         ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,        ,
	    XXXXXXX ,XXXXXXX  ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX , \
      XXXXXXX ,EE_CLR   ,QK_BOOT ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX , \
      XXXXXXX ,XXXXXXX  ,KC_SLEP ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX , \
      XXXXXXX ,XXXXXXX  ,KC_WAKE ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX, XXXXXXX, XXXXXXX  ,XXXXXXX ,XXXXXXX ,XXXXXXX, \
		  _______ ,_______  ,_______          ,_______          ,_______          ,_______          ,_______ ,XXXXXXX ,XXXXXXX ,XXXXXXX
  ),

};


layer_state_t layer_state_set_user(layer_state_t state) {
  return update_tri_layer_state(state, _LOWER, _RAISE, _ADJUST);
}



// bool process_record_user(uint16_t keycode, keyrecord_t *record) {

//   // typewriter pairing on jis keyboard
//   if (!twpair_on_jis(keycode, record))
//     return false;

//   return true;
// }

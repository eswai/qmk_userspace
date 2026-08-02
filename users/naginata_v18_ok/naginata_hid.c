/* Copyright eswai <@eswai>
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

// MacUnicodeInputからraw HIDでmacOSのかな/英数を受け取り、薙刀式のオンオフを合わせる。
//
// 通信はホスト→キーボードの一方向。パケットは32バイト固定長で、余りは0埋め。
//   [0] マジック 'N'
//   [1] コマンド STATE (0x01)
//   [2] 1:かな(薙刀式オン) / 0:英数(オフ)
//
// MacUnicodeInputは入力ソースが変わったときと、キーボードを見つけたときに送ってくる。
// 受け取った状態に対してかな/英数キーを送り返さない(naginata_sync_state)ので、
// ホストとの間でオンオフが往復し続けることはない。
//
// 逆向き(キーボード→mac)は従来どおりnaginata_on/offが送るかな/英数キーで切り替わる。

#include QMK_KEYBOARD_H
#include "naginata.h"

#ifdef RAW_ENABLE

#  include "raw_hid.h"

#  define NG_HID_MAGIC 'N'
#  define NG_HID_CMD_STATE 0x01

void raw_hid_receive(uint8_t *data, uint8_t length) {
  if (length < 3 || data[0] != NG_HID_MAGIC) return;

  if (data[1] == NG_HID_CMD_STATE) {
    naginata_sync_state(data[2] != 0);
  }
}

#endif // RAW_ENABLE

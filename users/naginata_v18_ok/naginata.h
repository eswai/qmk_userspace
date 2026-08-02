/* Copyright 2018-2019 eswai <@eswai>
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

#pragma once
#include QMK_KEYBOARD_H

// キーマップから使う関数
// (かな変換・カーソル操作などの内部関数はnaginata_v18.c内でstaticにしている)

// 薙刀式レイヤーとオン/オフキーを設定する。keyboard_post_init_userから呼ぶ
void set_naginata(uint8_t, uint16_t *, uint16_t *);
// process_record_userから呼ぶ。falseを返したらキー入力を消費済み
bool process_naginata(uint16_t, keyrecord_t *);
// 保留中の出力を確定させる。naginata_v18.cのhousekeeping_task_userから呼ばれる
void naginata_task(void);

void naginata_on(void);
void naginata_off(void);
bool naginata_state(void);
// ホスト(MacUnicodeInput)のかな/英数に合わせる。かな⇔英数のキーは送出しない
void naginata_sync_state(bool);

void switchOS(uint8_t);
void ng_set_unicode_mode(uint8_t);
void ng_show_os(void);
void tategaki_toggle(void);

// 縦書き/横書きを見て動く論理カーソル移動(薙刀式本体は使っていない)
void ng_prev_char(void);
void ng_next_char(void);
void ng_prev_row(void);
void ng_next_row(void);

// Unicode文字列の送出(strはRAM、pstrはPROGMEM)
void ng_send_unicode_string(const char *);
void ng_send_unicode_string_P(const char *);

// なぜKC_キーコードを使わず、NG_キーコードを定義するのか
// 1. 英字レイアウトがQWERTYでない場合でもOK
// 2. 薙刀式レイヤーでもKC_を定義すれば、かな変換せず出力できる
typedef enum naginata_keycodes {
  NG_Q = SAFE_RANGE, // 薙刀式シフトキー
  NG_W,
  NG_E,
  NG_R,
  NG_T,
  NG_Y,
  NG_U,
  NG_I,
  NG_O,
  NG_P,

  NG_A,
  NG_S,
  NG_D,
  NG_F,
  NG_G,
  NG_H,
  NG_J,
  NG_K,
  NG_L,
  NG_SCLN,

  NG_Z,
  NG_X,
  NG_C,
  NG_V,
  NG_B,
  NG_N,
  NG_M,
  NG_COMM,
  NG_DOT,
  NG_SLSH,

  NG_SHFT,
  NG_SHFT2,

  // NG_ON,
  // NG_OFF,
  // NG_CLR,
  NGSW_WIN,
  NGSW_MAC,
  NGSW_LNX,
  // NG_MLV,
  NG_SHOS,
  NG_TAYO,
  NG_KOTI,
} NGKEYS;

// EEPROMに保存する設定
typedef union {
  uint32_t raw;
  struct {
    uint8_t os;
    bool tategaki :1;
  };
} user_config_t;

// 修正: ヘッダでは実体を定義せずextern宣言にする。
//       naginata_v17.c側で定義。-fno-common環境での多重定義リンクエラーを回避
extern user_config_t naginata_config;

// 修正: 展開時の演算順序事故を防ぐため括弧で囲む
#define NG_SAFE_RANGE (SAFE_RANGE + 42)

// 重なり時間がこの値(ms)未満なら同時押しではなく個別打鍵と判定する。0で無効(従来動作)
#ifndef NG_MIN_OVERLAP_MS
#define NG_MIN_OVERLAP_MS 30
#endif

#define NG_WIN 1
#define NG_MAC 2
#define NG_LINUX 3

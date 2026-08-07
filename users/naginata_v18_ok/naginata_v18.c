/* Copyright eswai <@eswai> / Satoru NAKAYA <@tor-nky>
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
#include "nglist.h"
#include "nglistarray.h"
#include "naginata.h"

#include <string.h>

#if defined(CONSOLE_ENABLE)
#  define ngdbg(...) uprintf(__VA_ARGS__)
#else
#  define ngdbg(...)
#endif

// 修正: naginata.hはextern宣言のみになったので、実体はここで定義する
user_config_t naginata_config;

static bool is_naginata = false; // 薙刀式がオンかオフか
static uint8_t naginata_layer = 0; // NG_*を配置しているレイヤー番号
static uint16_t ngon_keys[2]; // 薙刀式をオンにするキー(通常HJ)
static uint16_t ngoff_keys[2]; // 薙刀式をオフにするキー(通常FG)
static NGListArray nginput;
static uint32_t pressed_keys; // 押しているキーのビットをたてる
static int8_t n_pressed_keys; // 押しているキーの数
static uint16_t last_press_time; // 最後に押した文字キーの押下時刻
static uint16_t last_pressed_kc; // 最後に押した文字キーのキーコード
static bool defer_flush; // 候補1件の確定出力をNG_MIN_OVERLAP_MS経過まで保留中

// 漢直(Unicode直接入力)のプレフィックス状態
typedef enum {
  NGU_NONE = 0,  // 通常
  NGU_PLANE1,    // 変換(KC_INT4)が押された
  NGU_PLANE2,    // 変換押下中にMが押された
  NGU_PLANE3,    // 変換押下中にVが押された
} ngu_state_t;
static ngu_state_t ngu_state = NGU_NONE;
static bool int4_held = false;      // KC_INT4を物理的に押下中か
static bool ngu_used_while_held = false; // 変換押下中に漢字を出力したか
static uint32_t ngu_consumed = 0;   // pressを消費したNG_キーのrelease消費用ビット

// 31キーを32bitの各ビットに割り当てる
#define B_Q    (1UL<<0)
#define B_W    (1UL<<1)
#define B_E    (1UL<<2)
#define B_R    (1UL<<3)
#define B_T    (1UL<<4)

#define B_Y    (1UL<<5)
#define B_U    (1UL<<6)
#define B_I    (1UL<<7)
#define B_O    (1UL<<8)
#define B_P    (1UL<<9)

#define B_A    (1UL<<10)
#define B_S    (1UL<<11)
#define B_D    (1UL<<12)
#define B_F    (1UL<<13)
#define B_G    (1UL<<14)

#define B_H    (1UL<<15)
#define B_J    (1UL<<16)
#define B_K    (1UL<<17)
#define B_L    (1UL<<18)
#define B_SCLN (1UL<<19)

#define B_Z    (1UL<<20)
#define B_X    (1UL<<21)
#define B_C    (1UL<<22)
#define B_V    (1UL<<23)
#define B_B    (1UL<<24)

#define B_N    (1UL<<25)
#define B_M    (1UL<<26)
#define B_COMM (1UL<<27)
#define B_DOT  (1UL<<28)
#define B_SLSH (1UL<<29)

#define B_SHFT (1UL<<30)

// キーコードとキービットの対応
// B_Q..B_SLSHのビット位置はNG_Q..NG_SLSHの並び順と一致しているので計算で求まる
// (NG_SHFT2だけは例外でNG_SHFTと同じB_SHFTを使う)
#define NG_BIT(kc) ((kc) == NG_SHFT2 ? B_SHFT : 1UL << ((kc) - NG_Q))

// カナ変換テーブル
// .kanaの中身は3種類
//   "ka"       : ローマ字。send_stringでそのまま送出する
//   NGU("百")  : Unicode文字
//   NGUE("」") : Unicode文字 + 確定(Windows/Linuxのみ。後述のng_kakutei)
//   ""         : .funcを呼ぶ
// Unicodeは先頭1バイトのタグで見分ける(kana[8] = タグ + 3バイト文字2個 + NUL)。
// SS_TAP()は"\1"で始まるのでタグには使えない。send_stringが解釈しない値を選ぶ
#define NGU_TAG  '\x1f'
#define NGUE_TAG '\x1e'
#define NGU(s)  "\x1f" s
#define NGUE(s) "\x1e" s

typedef struct {
  uint32_t shift;
  uint32_t douji;
  char kana[8];
  void (*func)(void);
} naginata_kanamap;

#define MAX_STRLEN 40

// RAM上の文字列を送る。辞書の.kana(memcpy_P済み)からも呼ぶ
void ng_send_unicode_string(const char *str) {
  switch (naginata_config.os) {
    case NG_LINUX:
      tap_code(KC_INTERNATIONAL_5);
      send_unicode_string(str);
      tap_code(KC_INTERNATIONAL_4);
      break;
    case NG_WIN:
    case NG_MAC:
      send_unicode_string(str);
      break;
  }
}

// PROGMEM上の文字列を送る
void ng_send_unicode_string_P(const char *pstr) {
  // 修正: 終端NUL分の1バイトを確保。以前はstr[MAX_STRLEN]でlen==MAX_STRLENの
  //       ときにstrcpy_Pがスタックを1バイト破壊していた
  if (strlen_P(pstr) > MAX_STRLEN) return;
  char str[MAX_STRLEN + 1];
  strcpy_P(str, pstr);
  ng_send_unicode_string(str);
}

static void nofunc(void) {}

// ---- 下位ユーティリティ ----

static void ng_tap_n(uint16_t kc, uint8_t n) {
  while (n--) {
    tap_code(kc);
  }
}

// Windows/Linuxは変換中の文字を確定する必要がある。
// {改行}だと改行が入ってしまうので、改行にならないCtrl+Mを使う。
// Macはng_send_unicode_stringの中で確定済みなので不要
static void ng_kakutei(void) {
  if (naginata_config.os != NG_MAC) {
    tap_code16(LCTL(KC_M));
  }
}

// Shiftを押しながらkcをn回打つ(範囲選択)
static void ng_sft_tap(uint16_t kc, uint8_t n) {
  register_code(KC_LSFT);
  ng_tap_n(kc, n);
  unregister_code(KC_LSFT);
}

// Shiftを押しながらOS依存の移動を行う(ng_home/ng_endなど)
static void ng_sft_fn(void (*f)(void)) {
  register_code(KC_LSFT);
  f();
  unregister_code(KC_LSFT);
}

// OSごとにキーが違うだけの操作をまとめる
static void ng_os_tap(uint16_t win, uint16_t mac) {
  tap_code16(naginata_config.os == NG_MAC ? mac : win);
}

// 縦書き/横書きで方向が入れ替わるカーソル移動(Macは論理移動のキーがある)
static void ng_move(uint16_t yoko, uint16_t tate, uint16_t mac) {
  if (naginata_config.os == NG_MAC) {
    tap_code16(mac);
    return;
  }
  tap_code(naginata_config.tategaki ? tate : yoko);
}

// ---- カーソル移動・編集 ----

// 縦書き対応の論理移動。辞書からは使っていないのでキーマップ用に公開しておく

static void ng_up(uint8_t c)    { ng_tap_n(KC_UP, c); }
static void ng_down(uint8_t c)  { ng_tap_n(KC_DOWN, c); }
static void ng_left(uint8_t c)  { ng_tap_n(KC_LEFT, c); }
static void ng_right(uint8_t c) { ng_tap_n(KC_RIGHT, c); }

static void ng_cut(void)      { ng_os_tap(LCTL(KC_X),   LCMD(KC_X)); }
static void ng_copy(void)     { ng_os_tap(LCTL(KC_C),   LCMD(KC_C)); }
static void ng_home(void)     { ng_os_tap(KC_HOME,      LCTL(KC_A)); }
static void ng_end(void)      { ng_os_tap(KC_END,       LCTL(KC_E)); }
static void ng_katakana(void) { ng_os_tap(LCTL(KC_I),   LCTL(KC_K)); }
static void ng_hiragana(void) { ng_os_tap(LCTL(KC_U),   LCTL(KC_J)); }
static void ng_save(void)     { ng_os_tap(LCTL(KC_S),   LCMD(KC_S)); }
static void ng_undo(void)     { ng_os_tap(LCTL(KC_Z),   LCMD(KC_Z)); }
static void ng_redo(void)     { ng_os_tap(LCTL(KC_Y),   LSFT(LCMD(KC_Z))); }
static void ng_eof(void)      { ng_os_tap(LCTL(KC_END), LCMD(KC_DOWN)); }

void ng_prev_char(void) { ng_move(KC_LEFT,  KC_UP,    LCTL(KC_B)); }
void ng_next_char(void) { ng_move(KC_RIGHT, KC_DOWN,  LCTL(KC_F)); }
void ng_prev_row(void)  { ng_move(KC_UP,    KC_RIGHT, LCTL(KC_P)); }
void ng_next_row(void)  { ng_move(KC_DOWN,  KC_LEFT,  LCTL(KC_N)); }

// Macはコマンドキーを押す間隔を空けないと取りこぼす
static void ng_paste(void) {
  if (naginata_config.os != NG_MAC) {
    tap_code16(LCTL(KC_V));
    return;
  }
  register_code(KC_LCMD);
  wait_ms(100);
  tap_code(KC_V);
  wait_ms(100);
  unregister_code(KC_LCMD);
  wait_ms(100);
}

// Macはかなキーの2打で再変換する
static void ng_saihenkan(void) {
  if (naginata_config.os != NG_MAC) {
    tap_code(KC_INT4);
    return;
  }
  tap_code(KC_LANGUAGE_1);
  tap_code(KC_LANGUAGE_1);
}

// {Home}{改行}{Space n}{←} 前の行に字下げした行を作る
static void ngh_new_indented_row(uint8_t n) {
  ng_home();
  tap_code(KC_ENT);
  ng_tap_n(KC_SPC, n);
  ng_left(1);
}

// {Home}{→}{End}{Del n}{←} 前の行の行末からn文字消す
static void ngh_del_at_prev_eol(uint8_t n) {
  ng_home();
  ng_right(1);
  ng_end();
  ng_tap_n(KC_DEL, n);
  ng_left(1);
}

// ---- 辞書から呼ばれる操作 ----

static void ng_T(void)  { ng_left(1); }
static void ng_Y(void)  { ng_right(1); }
static void ng_ST(void) { ng_sft_tap(KC_LEFT, 1); }
static void ng_SY(void) { ng_sft_tap(KC_RIGHT, 1); }

static void ngh_DFY(void)    { ng_home(); }
static void ngh_DFI(void)    { ng_saihenkan(); }
static void ngh_DFJ(void)    { ng_up(1); }
static void ngh_DFK(void)    { ng_sft_tap(KC_UP, 1); }
static void ngh_DFL(void)    { ng_sft_tap(KC_UP, 7); }
static void ngh_DFSCLN(void) { ng_katakana(); }
static void ngh_DFN(void)    { ng_end(); }
static void ngh_DFM(void)    { ng_down(1); }
static void ngh_DFCOMM(void) { ng_sft_tap(KC_DOWN, 1); }
static void ngh_DFDOT(void)  { ng_sft_tap(KC_DOWN, 7); }
static void ngh_DFSLSH(void) { ng_hiragana(); }
static void ngh_DFP(void)    { ng_sft_tap(KC_ESC, 3); }

static void ngh_DFU(void) { // +{End}{BS} 行末まで選択して削除
  ng_sft_fn(ng_end);
  tap_code(KC_BSPC);
}

static void ngh_DFH(void) { // {改行}{End}
  tap_code(KC_ENT);
  ng_end();
}

static void ngh_JKQ(void) { ng_eof(); }
static void ngh_JKR(void) { ng_save(); }

static void ngh_CVY(void)    { ng_sft_fn(ng_home); }
static void ngh_CVU(void)    { ng_cut(); }
static void ngh_CVI(void)    { ng_saihenkan(); }
static void ngh_CVO(void)    { ng_paste(); }
static void ngh_CVP(void)    { ng_undo(); }
static void ngh_CVH(void)    { ng_copy(); }
static void ngh_CVJ(void)    { ng_left(1); }
static void ngh_CVK(void)    { ng_right(1); }
static void ngh_CVL(void)    { ng_sft_tap(KC_LEFT, 7); }
static void ngh_CVSCLN(void) { ng_sft_tap(KC_RIGHT, 7); }
static void ngh_CVN(void)    { ng_sft_fn(ng_end); }
static void ngh_CVM(void)    { ng_sft_tap(KC_LEFT, 1); }
static void ngh_CVCOMM(void) { ng_sft_tap(KC_RIGHT, 1); }
static void ngh_CVSLSH(void) { ng_redo(); }

static void ngh_CVDOT(void) { // {End}+{Home} 行を選択する
  ng_end();
  ng_sft_fn(ng_home);
}

static void ngh_MCE(void) { ngh_del_at_prev_eol(2); }
static void ngh_MCD(void) { ngh_del_at_prev_eol(4); }
static void ngh_MCR(void) { ngh_new_indented_row(1); }
static void ngh_MCF(void) { ngh_new_indented_row(3); }
static void ngh_MCG(void) { ng_tap_n(KC_SPC, 3); }

static void ngh_MCW(void) { // ×　　　×　　　×{確定}{改行}
  ng_send_unicode_string_P(PSTR("　　　×　　　×　　　×"));
  ng_kakutei();
  tap_code(KC_ENT);
}

static void ngh_MCC(void) { // 」{確定}{改行}
  ng_send_unicode_string_P(PSTR("」"));
  ng_kakutei();
  tap_code(KC_ENT);
}

static void ngh_MCV(void) { // 」{確定}{改行}「{確定}
  ng_send_unicode_string_P(PSTR("」"));
  ng_kakutei();
  tap_code(KC_ENT);
  ng_send_unicode_string_P(PSTR("「"));
  ng_kakutei();
}

static void ngh_MCB(void) { // 」{確定}{改行}{Space}
  ng_send_unicode_string_P(PSTR("」"));
  ng_kakutei();
  tap_code(KC_ENT);
  tap_code(KC_SPC);
}

static const PROGMEM naginata_kanamap ngdickana[] = {
  {.shift = 0UL        , .douji = B_SHFT         , .kana = " "                                           , .func = nofunc },
  {.shift = 0UL        , .douji = B_W            , .kana = "ki"                                          , .func = nofunc }, // き
  {.shift = 0UL        , .douji = B_E            , .kana = "te"                                          , .func = nofunc }, // て
  {.shift = 0UL        , .douji = B_R            , .kana = "si"                                          , .func = nofunc }, // し
  {.shift = 0UL        , .douji = B_T            , .kana = ""                                            , .func = ng_T }, // {←}
  {.shift = 0UL        , .douji = B_Y            , .kana = ""                                            , .func = ng_Y }, // {→}
  {.shift = 0UL        , .douji = B_U            , .kana = SS_TAP(X_BACKSPACE)                           , .func = nofunc }, // {BS}
  {.shift = 0UL        , .douji = B_I            , .kana = "ru"                                          , .func = nofunc }, // る
  {.shift = 0UL        , .douji = B_O            , .kana = "su"                                          , .func = nofunc }, // す
  {.shift = 0UL        , .douji = B_P            , .kana = "he"                                          , .func = nofunc }, // へ
  {.shift = 0UL        , .douji = B_A            , .kana = "ro"                                          , .func = nofunc }, // ろ
  {.shift = 0UL        , .douji = B_S            , .kana = "ke"                                          , .func = nofunc }, // け
  {.shift = 0UL        , .douji = B_D            , .kana = "to"                                          , .func = nofunc }, // と
  {.shift = 0UL        , .douji = B_F            , .kana = "ka"                                          , .func = nofunc }, // か
  {.shift = 0UL        , .douji = B_G            , .kana = "xtu"                                         , .func = nofunc }, // っ
  {.shift = 0UL        , .douji = B_H            , .kana = "ku"                                          , .func = nofunc }, // く
  {.shift = 0UL        , .douji = B_J            , .kana = "a"                                           , .func = nofunc }, // あ
  {.shift = 0UL        , .douji = B_K            , .kana = "i"                                           , .func = nofunc }, // い
  {.shift = 0UL        , .douji = B_L            , .kana = "u"                                           , .func = nofunc }, // う
  {.shift = 0UL        , .douji = B_SCLN         , .kana = "-"                                           , .func = nofunc }, // ー
  {.shift = 0UL        , .douji = B_Z            , .kana = "ho"                                          , .func = nofunc }, // ほ
  {.shift = 0UL        , .douji = B_X            , .kana = "hi"                                          , .func = nofunc }, // ひ
  {.shift = 0UL        , .douji = B_C            , .kana = "ha"                                          , .func = nofunc }, // は
  {.shift = 0UL        , .douji = B_V            , .kana = "ko"                                          , .func = nofunc }, // こ
  {.shift = 0UL        , .douji = B_B            , .kana = "so"                                          , .func = nofunc }, // そ
  {.shift = 0UL        , .douji = B_N            , .kana = "ta"                                          , .func = nofunc }, // た
  {.shift = 0UL        , .douji = B_M            , .kana = "na"                                          , .func = nofunc }, // な
  {.shift = 0UL        , .douji = B_COMM         , .kana = "nn"                                          , .func = nofunc }, // ん
  {.shift = 0UL        , .douji = B_DOT          , .kana = "ra"                                          , .func = nofunc }, // ら
  {.shift = 0UL        , .douji = B_SLSH         , .kana = "re"                                          , .func = nofunc }, // れ
  {.shift = B_SHFT     , .douji = B_W            , .kana = "ne"                                          , .func = nofunc }, // ね
  {.shift = B_SHFT     , .douji = B_E            , .kana = "ri"                                          , .func = nofunc }, // り
  {.shift = B_SHFT     , .douji = B_R            , .kana = "me"                                          , .func = nofunc }, // め
  {.shift = B_SHFT     , .douji = B_T            , .kana = ""                                            , .func = ng_ST }, // +{←}
  {.shift = B_SHFT     , .douji = B_Y            , .kana = ""                                            , .func = ng_SY }, // +{→}
  {.shift = B_SHFT     , .douji = B_U            , .kana = "sa"                                          , .func = nofunc }, // さ
  {.shift = B_SHFT     , .douji = B_I            , .kana = "yo"                                          , .func = nofunc }, // よ
  {.shift = B_SHFT     , .douji = B_O            , .kana = "e"                                           , .func = nofunc }, // え
  {.shift = B_SHFT     , .douji = B_P            , .kana = "yu"                                          , .func = nofunc }, // ゆ
  {.shift = B_SHFT     , .douji = B_A            , .kana = "se"                                          , .func = nofunc }, // せ
  {.shift = B_SHFT     , .douji = B_S            , .kana = "mi"                                          , .func = nofunc }, // み
  {.shift = B_SHFT     , .douji = B_D            , .kana = "ni"                                          , .func = nofunc }, // に
  {.shift = B_SHFT     , .douji = B_F            , .kana = "ma"                                          , .func = nofunc }, // ま
  {.shift = B_SHFT     , .douji = B_G            , .kana = "ti"                                          , .func = nofunc }, // ち
  {.shift = B_SHFT     , .douji = B_H            , .kana = "ya"                                          , .func = nofunc }, // や
  {.shift = B_SHFT     , .douji = B_J            , .kana = "no"                                          , .func = nofunc }, // の
  {.shift = B_SHFT     , .douji = B_K            , .kana = "mo"                                          , .func = nofunc }, // も
  {.shift = B_SHFT     , .douji = B_L            , .kana = "tu"                                          , .func = nofunc }, // つ
  {.shift = B_SHFT     , .douji = B_SCLN         , .kana = "hu"                                          , .func = nofunc }, // ふ
  {.shift = B_SHFT     , .douji = B_Z            , .kana = "ho"                                          , .func = nofunc }, // ほ
  {.shift = B_SHFT     , .douji = B_X            , .kana = "hi"                                          , .func = nofunc }, // ひ
  {.shift = B_SHFT     , .douji = B_C            , .kana = "wo"                                          , .func = nofunc }, // を
  {.shift = B_SHFT     , .douji = B_V            , .kana = ","SS_TAP(X_ENTER)                            , .func = nofunc }, // 、{Enter}
  {.shift = B_SHFT     , .douji = B_B            , .kana = "nu"                                          , .func = nofunc }, // ぬ
  {.shift = B_SHFT     , .douji = B_N            , .kana = "o"                                           , .func = nofunc }, // お
  {.shift = B_SHFT     , .douji = B_M            , .kana = "."SS_TAP(X_ENTER)                            , .func = nofunc }, // 。{Enter}
  {.shift = B_SHFT     , .douji = B_COMM         , .kana = "mu"                                          , .func = nofunc }, // む
  {.shift = B_SHFT     , .douji = B_DOT          , .kana = "wa"                                          , .func = nofunc }, // わ
  {.shift = B_SHFT     , .douji = B_SLSH         , .kana = "re"                                          , .func = nofunc }, // れ
  {.shift = 0UL        , .douji = B_F|B_U        , .kana = "za"                                          , .func = nofunc }, // ざ
  {.shift = 0UL        , .douji = B_F|B_O        , .kana = "zu"                                          , .func = nofunc }, // ず
  {.shift = 0UL        , .douji = B_F|B_P        , .kana = "be"                                          , .func = nofunc }, // べ
  {.shift = 0UL        , .douji = B_F|B_H        , .kana = "gu"                                          , .func = nofunc }, // ぐ
  {.shift = 0UL        , .douji = B_F|B_L        , .kana = "du"                                          , .func = nofunc }, // づ
  {.shift = 0UL        , .douji = B_F|B_SCLN     , .kana = "bu"                                          , .func = nofunc }, // ぶ
  {.shift = 0UL        , .douji = B_F|B_N        , .kana = "da"                                          , .func = nofunc }, // だ
  {.shift = 0UL        , .douji = B_J|B_W        , .kana = "gi"                                          , .func = nofunc }, // ぎ
  {.shift = 0UL        , .douji = B_J|B_E        , .kana = "de"                                          , .func = nofunc }, // で
  {.shift = 0UL        , .douji = B_J|B_R        , .kana = "zi"                                          , .func = nofunc }, // じ
  {.shift = 0UL        , .douji = B_J|B_A        , .kana = "ze"                                          , .func = nofunc }, // ぜ
  {.shift = 0UL        , .douji = B_J|B_S        , .kana = "ge"                                          , .func = nofunc }, // げ
  {.shift = 0UL        , .douji = B_J|B_D        , .kana = "do"                                          , .func = nofunc }, // ど
  {.shift = 0UL        , .douji = B_J|B_F        , .kana = "ga"                                          , .func = nofunc }, // が
  {.shift = 0UL        , .douji = B_J|B_G        , .kana = "di"                                          , .func = nofunc }, // ぢ
  {.shift = 0UL        , .douji = B_J|B_Z        , .kana = "bo"                                          , .func = nofunc }, // ぼ
  {.shift = 0UL        , .douji = B_J|B_X        , .kana = "bi"                                          , .func = nofunc }, // び
  {.shift = 0UL        , .douji = B_J|B_C        , .kana = "ba"                                          , .func = nofunc }, // ば
  {.shift = 0UL        , .douji = B_J|B_V        , .kana = "go"                                          , .func = nofunc }, // ご
  {.shift = 0UL        , .douji = B_J|B_B        , .kana = "zo"                                          , .func = nofunc }, // ぞ
  {.shift = 0UL        , .douji = B_V|B_P        , .kana = "pe"                                          , .func = nofunc }, // ぺ
  {.shift = 0UL        , .douji = B_V|B_SCLN     , .kana = "pu"                                          , .func = nofunc }, // ぷ
  {.shift = 0UL        , .douji = B_M|B_Z        , .kana = "po"                                          , .func = nofunc }, // ぽ
  {.shift = 0UL        , .douji = B_M|B_X        , .kana = "pi"                                          , .func = nofunc }, // ぴ
  {.shift = 0UL        , .douji = B_M|B_C        , .kana = "pa"                                          , .func = nofunc }, // ぱ
  {.shift = 0UL        , .douji = B_Q|B_I        , .kana = "xyo"                                         , .func = nofunc }, // ょ
  {.shift = 0UL        , .douji = B_Q|B_O        , .kana = "xe"                                          , .func = nofunc }, // ぇ
  {.shift = 0UL        , .douji = B_Q|B_P        , .kana = "xyu"                                         , .func = nofunc }, // ゅ
  {.shift = 0UL        , .douji = B_Q|B_S        , .kana = "xke"                                         , .func = nofunc }, // ヶ
  {.shift = 0UL        , .douji = B_Q|B_F        , .kana = "xka"                                         , .func = nofunc }, // ヵ
  {.shift = 0UL        , .douji = B_Q|B_H        , .kana = "xya"                                         , .func = nofunc }, // ゃ
  {.shift = 0UL        , .douji = B_Q|B_J        , .kana = "xa"                                          , .func = nofunc }, // ぁ
  {.shift = 0UL        , .douji = B_Q|B_K        , .kana = "xi"                                          , .func = nofunc }, // ぃ
  {.shift = 0UL        , .douji = B_Q|B_L        , .kana = "xu"                                          , .func = nofunc }, // ぅ
  {.shift = 0UL        , .douji = B_Q|B_N        , .kana = "xo"                                          , .func = nofunc }, // ぉ
  {.shift = 0UL        , .douji = B_Q|B_DOT      , .kana = "xwa"                                         , .func = nofunc }, // ゎ
  {.shift = 0UL        , .douji = B_H|B_W        , .kana = "kya"                                         , .func = nofunc }, // きゃ
  {.shift = 0UL        , .douji = B_H|B_E        , .kana = "rya"                                         , .func = nofunc }, // りゃ
  {.shift = 0UL        , .douji = B_H|B_R        , .kana = "sya"                                         , .func = nofunc }, // しゃ
  {.shift = 0UL        , .douji = B_H|B_S        , .kana = "mya"                                         , .func = nofunc }, // みゃ
  {.shift = 0UL        , .douji = B_H|B_D        , .kana = "nya"                                         , .func = nofunc }, // にゃ
  {.shift = 0UL        , .douji = B_H|B_G        , .kana = "tya"                                         , .func = nofunc }, // ちゃ
  {.shift = 0UL        , .douji = B_H|B_X        , .kana = "hya"                                         , .func = nofunc }, // ひゃ
  {.shift = 0UL        , .douji = B_P|B_W        , .kana = "kyu"                                         , .func = nofunc }, // きゅ
  {.shift = 0UL        , .douji = B_P|B_E        , .kana = "ryu"                                         , .func = nofunc }, // りゅ
  {.shift = 0UL        , .douji = B_P|B_R        , .kana = "syu"                                         , .func = nofunc }, // しゅ
  {.shift = 0UL        , .douji = B_P|B_S        , .kana = "myu"                                         , .func = nofunc }, // みゅ
  {.shift = 0UL        , .douji = B_P|B_D        , .kana = "nyu"                                         , .func = nofunc }, // にゅ
  {.shift = 0UL        , .douji = B_P|B_G        , .kana = "tyu"                                         , .func = nofunc }, // ちゅ
  {.shift = 0UL        , .douji = B_P|B_X        , .kana = "hyu"                                         , .func = nofunc }, // ひゅ
  {.shift = 0UL        , .douji = B_I|B_W        , .kana = "kyo"                                         , .func = nofunc }, // きょ
  {.shift = 0UL        , .douji = B_I|B_E        , .kana = "ryo"                                         , .func = nofunc }, // りょ
  {.shift = 0UL        , .douji = B_I|B_R        , .kana = "syo"                                         , .func = nofunc }, // しょ
  {.shift = 0UL        , .douji = B_I|B_S        , .kana = "myo"                                         , .func = nofunc }, // みょ
  {.shift = 0UL        , .douji = B_I|B_D        , .kana = "nyo"                                         , .func = nofunc }, // にょ
  {.shift = 0UL        , .douji = B_I|B_G        , .kana = "tyo"                                         , .func = nofunc }, // ちょ
  {.shift = 0UL        , .douji = B_I|B_X        , .kana = "hyo"                                         , .func = nofunc }, // ひょ
  {.shift = 0UL        , .douji = B_J|B_H|B_W    , .kana = "gya"                                         , .func = nofunc }, // ぎゃ
  {.shift = 0UL        , .douji = B_J|B_H|B_R    , .kana = "zya"                                         , .func = nofunc }, // じゃ
  {.shift = 0UL        , .douji = B_J|B_H|B_G    , .kana = "dya"                                         , .func = nofunc }, // ぢゃ
  {.shift = 0UL        , .douji = B_J|B_H|B_X    , .kana = "bya"                                         , .func = nofunc }, // びゃ
  {.shift = 0UL        , .douji = B_J|B_P|B_W    , .kana = "gyu"                                         , .func = nofunc }, // ぎゅ
  {.shift = 0UL        , .douji = B_J|B_P|B_R    , .kana = "zyu"                                         , .func = nofunc }, // じゅ
  {.shift = 0UL        , .douji = B_J|B_P|B_G    , .kana = "dyu"                                         , .func = nofunc }, // ぢゅ
  {.shift = 0UL        , .douji = B_J|B_P|B_X    , .kana = "byu"                                         , .func = nofunc }, // びゅ
  {.shift = 0UL        , .douji = B_J|B_I|B_W    , .kana = "gyo"                                         , .func = nofunc }, // ぎょ
  {.shift = 0UL        , .douji = B_J|B_I|B_R    , .kana = "zyo"                                         , .func = nofunc }, // じょ
  {.shift = 0UL        , .douji = B_J|B_I|B_G    , .kana = "dyo"                                         , .func = nofunc }, // ぢょ
  {.shift = 0UL        , .douji = B_J|B_I|B_X    , .kana = "byo"                                         , .func = nofunc }, // びょ
  {.shift = 0UL        , .douji = B_M|B_X|B_I    , .kana = "pyo"                                         , .func = nofunc }, // ぴょ
  {.shift = 0UL        , .douji = B_M|B_X|B_P    , .kana = "pyu"                                         , .func = nofunc }, // ぴゅ
  {.shift = 0UL        , .douji = B_M|B_X|B_H    , .kana = "pya"                                         , .func = nofunc }, // ぴゃ
  {.shift = 0UL        , .douji = B_M|B_E|B_P    , .kana = "texyu"                                       , .func = nofunc }, // てゅ
  {.shift = 0UL        , .douji = B_M|B_E|B_K    , .kana = "thi"                                         , .func = nofunc }, // てぃ
  {.shift = 0UL        , .douji = B_J|B_E|B_P    , .kana = "dhu"                                         , .func = nofunc }, // でゅ
  {.shift = 0UL        , .douji = B_J|B_E|B_K    , .kana = "dhi"                                         , .func = nofunc }, // でぃ
  {.shift = 0UL        , .douji = B_M|B_D|B_L    , .kana = "toxu"                                        , .func = nofunc }, // とぅ
  {.shift = 0UL        , .douji = B_J|B_D|B_L    , .kana = "doxu"                                        , .func = nofunc }, // どぅ
  {.shift = 0UL        , .douji = B_M|B_R|B_O    , .kana = "sye"                                         , .func = nofunc }, // しぇ
  {.shift = 0UL        , .douji = B_M|B_G|B_O    , .kana = "tye"                                         , .func = nofunc }, // ちぇ
  {.shift = 0UL        , .douji = B_J|B_R|B_O    , .kana = "zye"                                         , .func = nofunc }, // じぇ
  {.shift = 0UL        , .douji = B_J|B_G|B_O    , .kana = "dye"                                         , .func = nofunc }, // ぢぇ
  {.shift = 0UL        , .douji = B_V|B_SCLN|B_O , .kana = "fe"                                          , .func = nofunc }, // ふぇ
  {.shift = 0UL        , .douji = B_V|B_SCLN|B_P , .kana = "fyu"                                         , .func = nofunc }, // ふゅ
  {.shift = 0UL        , .douji = B_V|B_SCLN|B_J , .kana = "fa"                                          , .func = nofunc }, // ふぁ
  {.shift = 0UL        , .douji = B_V|B_SCLN|B_K , .kana = "fi"                                          , .func = nofunc }, // ふぃ
  {.shift = 0UL        , .douji = B_V|B_SCLN|B_N , .kana = "fo"                                          , .func = nofunc }, // ふぉ
  {.shift = 0UL        , .douji = B_F|B_L|B_O    , .kana = "ve"                                          , .func = nofunc }, // ヴぇ
  {.shift = 0UL        , .douji = B_F|B_L|B_P    , .kana = "vuxyu"                                       , .func = nofunc }, // ヴゅ
  {.shift = 0UL        , .douji = B_F|B_L|B_J    , .kana = "va"                                          , .func = nofunc }, // ヴぁ
  {.shift = 0UL        , .douji = B_F|B_L|B_K    , .kana = "vi"                                          , .func = nofunc }, // ヴぃ
  {.shift = 0UL        , .douji = B_F|B_L|B_SCLN , .kana = "vu"                                          , .func = nofunc }, // ヴ
  {.shift = 0UL        , .douji = B_F|B_L|B_N    , .kana = "vo"                                          , .func = nofunc }, // ヴぉ
  {.shift = 0UL        , .douji = B_V|B_L|B_O    , .kana = "we"                                          , .func = nofunc }, // うぇ
  {.shift = 0UL        , .douji = B_V|B_L|B_K    , .kana = "wi"                                          , .func = nofunc }, // うぃ
  {.shift = 0UL        , .douji = B_V|B_L|B_N    , .kana = "uxo"                                         , .func = nofunc }, // うぉ
  {.shift = 0UL        , .douji = B_V|B_K|B_O    , .kana = "ixe"                                         , .func = nofunc }, // いぇ
  {.shift = 0UL        , .douji = B_V|B_L|B_J    , .kana = "tsa"                                         , .func = nofunc }, // つぁ
  {.shift = 0UL        , .douji = B_V|B_H|B_O    , .kana = "kuxe"                                        , .func = nofunc }, // くぇ
  {.shift = 0UL        , .douji = B_V|B_H|B_J    , .kana = "kuxa"                                        , .func = nofunc }, // くぁ
  {.shift = 0UL        , .douji = B_V|B_H|B_K    , .kana = "kuxi"                                        , .func = nofunc }, // くぃ
  {.shift = 0UL        , .douji = B_V|B_H|B_N    , .kana = "kuxo"                                        , .func = nofunc }, // くぉ
  {.shift = 0UL        , .douji = B_V|B_H|B_DOT  , .kana = "kuxwa"                                       , .func = nofunc }, // くゎ
  {.shift = 0UL        , .douji = B_F|B_H|B_O    , .kana = "guxe"                                        , .func = nofunc }, // ぐぇ
  {.shift = 0UL        , .douji = B_F|B_H|B_J    , .kana = "guxa"                                        , .func = nofunc }, // ぐぁ
  {.shift = 0UL        , .douji = B_F|B_H|B_K    , .kana = "guxi"                                        , .func = nofunc }, // ぐぃ
  {.shift = 0UL        , .douji = B_F|B_H|B_N    , .kana = "guxo"                                        , .func = nofunc }, // ぐぉ
  {.shift = 0UL        , .douji = B_F|B_H|B_DOT  , .kana = "guxwa"                                       , .func = nofunc }, // ぐゎ
  {.shift = 0UL        , .douji = B_H|B_J        , .kana = ""                                            , .func = naginata_on }, // {vkF2}
  {.shift = B_SHFT     , .douji = B_H|B_J        , .kana = ""                                            , .func = naginata_on }, // {vkF2}
  {.shift = 0UL        , .douji = B_G|B_F        , .kana = ""                                            , .func = naginata_off }, // {vk1D}
  {.shift = B_SHFT     , .douji = B_G|B_F        , .kana = ""                                            , .func = naginata_off }, // {vk1D}
  {.shift = 0UL        , .douji = B_V|B_M        , .kana = SS_TAP(X_ENTER)                               , .func = nofunc }, // {Enter}
  {.shift = B_SHFT     , .douji = B_V|B_M        , .kana = SS_TAP(X_ENTER)                               , .func = nofunc }, // {Enter}
  {.shift = B_D|B_F    , .douji = B_Y            , .kana = ""                                            , .func = ngh_DFY }, // {Home}
  {.shift = B_D|B_F    , .douji = B_U            , .kana = ""                                            , .func = ngh_DFU }, // +{End}{BS}
  {.shift = B_D|B_F    , .douji = B_I            , .kana = ""                                            , .func = ngh_DFI }, // {vk1Csc079}
  {.shift = B_D|B_F    , .douji = B_O            , .kana = SS_TAP(X_DELETE)                              , .func = nofunc }, // {Del}
  {.shift = B_D|B_F    , .douji = B_P            , .kana = ""                                            , .func = ngh_DFP }, // +{Esc 3}
  {.shift = B_D|B_F    , .douji = B_H            , .kana = ""                                            , .func = ngh_DFH }, // {Enter}{End}
  {.shift = B_D|B_F    , .douji = B_J            , .kana = ""                                            , .func = ngh_DFJ }, // {↑}
  {.shift = B_D|B_F    , .douji = B_K            , .kana = ""                                            , .func = ngh_DFK }, // +{↑}
  {.shift = B_D|B_F    , .douji = B_L            , .kana = ""                                            , .func = ngh_DFL }, // +{↑ 7}
  {.shift = B_D|B_F    , .douji = B_SCLN         , .kana = ""                                            , .func = ngh_DFSCLN }, // ^i
  {.shift = B_D|B_F    , .douji = B_N            , .kana = ""                                            , .func = ngh_DFN }, // {End}
  {.shift = B_D|B_F    , .douji = B_M            , .kana = ""                                            , .func = ngh_DFM }, // {↓}
  {.shift = B_D|B_F    , .douji = B_COMM         , .kana = ""                                            , .func = ngh_DFCOMM }, // +{↓}
  {.shift = B_D|B_F    , .douji = B_DOT          , .kana = ""                                            , .func = ngh_DFDOT }, // +{↓ 7}
  {.shift = B_D|B_F    , .douji = B_SLSH         , .kana = ""                                            , .func = ngh_DFSLSH }, // ^u
  {.shift = B_J|B_K    , .douji = B_Q            , .kana = ""                                            , .func = ngh_JKQ }, // ^{End}
  {.shift = B_J|B_K    , .douji = B_W            , .kana = NGUE("／")                                    , .func = nofunc }, // ／{改行}
  {.shift = B_J|B_K    , .douji = B_R            , .kana = ""                                            , .func = ngh_JKR }, // ^s
  {.shift = B_J|B_K    , .douji = B_T            , .kana = "/"                                           , .func = nofunc }, // ・
  {.shift = B_J|B_K    , .douji = B_A            , .kana = NGUE("……")                                   , .func = nofunc }, // ……{改行}
  {.shift = B_J|B_K    , .douji = B_S            , .kana = NGUE("『")                                    , .func = nofunc }, // 『{改行}
  {.shift = B_J|B_K    , .douji = B_D            , .kana = NGUE("？")                                    , .func = nofunc }, // ？{改行}
  {.shift = B_J|B_K    , .douji = B_F            , .kana = NGUE("「")                                    , .func = nofunc }, // 「{改行}
  {.shift = B_J|B_K    , .douji = B_G            , .kana = NGUE("（")                                    , .func = nofunc }, // ({改行}
  {.shift = B_J|B_K    , .douji = B_Z            , .kana = NGUE("――")                                   , .func = nofunc }, // ――{改行}
  {.shift = B_J|B_K    , .douji = B_X            , .kana = NGUE("』")                                    , .func = nofunc }, // 』{改行}
  {.shift = B_J|B_K    , .douji = B_C            , .kana = NGUE("！")                                    , .func = nofunc }, // ！{改行}
  {.shift = B_J|B_K    , .douji = B_V            , .kana = NGUE("」")                                    , .func = nofunc }, // 」{改行}
  {.shift = B_J|B_K    , .douji = B_B            , .kana = NGUE("）")                                    , .func = nofunc }, // ){改行}
  {.shift = B_C|B_V    , .douji = B_Y            , .kana = ""                                            , .func = ngh_CVY }, // +{Home}
  {.shift = B_C|B_V    , .douji = B_U            , .kana = ""                                            , .func = ngh_CVU }, // ^x
  {.shift = B_C|B_V    , .douji = B_I            , .kana = ""                                            , .func = ngh_CVI }, // {vk1Csc079}
  {.shift = B_C|B_V    , .douji = B_O            , .kana = ""                                            , .func = ngh_CVO }, // ^v
  {.shift = B_C|B_V    , .douji = B_P            , .kana = ""                                            , .func = ngh_CVP }, // ^z
  {.shift = B_C|B_V    , .douji = B_H            , .kana = ""                                            , .func = ngh_CVH }, // ^c
  {.shift = B_C|B_V    , .douji = B_J            , .kana = ""                                            , .func = ngh_CVJ }, // {←}
  {.shift = B_C|B_V    , .douji = B_K            , .kana = ""                                            , .func = ngh_CVK }, // {→}
  {.shift = B_C|B_V    , .douji = B_L            , .kana = ""                                            , .func = ngh_CVL }, // +{← 7}
  {.shift = B_C|B_V    , .douji = B_SCLN         , .kana = ""                                            , .func = ngh_CVSCLN }, // +{→ 7}
  {.shift = B_C|B_V    , .douji = B_N            , .kana = ""                                            , .func = ngh_CVN }, // +{End}
  {.shift = B_C|B_V    , .douji = B_M            , .kana = ""                                            , .func = ngh_CVM }, // +{←}
  {.shift = B_C|B_V    , .douji = B_COMM         , .kana = ""                                            , .func = ngh_CVCOMM }, // +{→}
  {.shift = B_C|B_V    , .douji = B_DOT          , .kana = ""                                            , .func = ngh_CVDOT }, // {End}+{Home}
  {.shift = B_C|B_V    , .douji = B_SLSH         , .kana = ""                                            , .func = ngh_CVSLSH }, // ^y
  {.shift = B_M|B_COMM , .douji = B_Q            , .kana = NGUE("｜")                                    , .func = nofunc }, // ｜{改行}
  {.shift = B_M|B_COMM , .douji = B_W            , .kana = ""                                            , .func = ngh_MCW }, // ×　　　×　　　×{確定}{改行}
  {.shift = B_M|B_COMM , .douji = B_E            , .kana = ""                                            , .func = ngh_MCE }, // {Home}{→}{End}{Del 2}{←}
  {.shift = B_M|B_COMM , .douji = B_R            , .kana = ""                                            , .func = ngh_MCR }, // {Home}{改行}{Space 1}{←}
  {.shift = B_M|B_COMM , .douji = B_T            , .kana = NGUE("○")                                    , .func = nofunc }, // ○{改行}
  {.shift = B_M|B_COMM , .douji = B_A            , .kana = NGUE("《")                                    , .func = nofunc }, // 《{改行}
  {.shift = B_M|B_COMM , .douji = B_S            , .kana = NGUE("【")                                    , .func = nofunc }, // 【{改行}
  {.shift = B_M|B_COMM , .douji = B_D            , .kana = ""                                            , .func = ngh_MCD }, // {Home}{→}{End}{Del 4}{←}
  {.shift = B_M|B_COMM , .douji = B_F            , .kana = ""                                            , .func = ngh_MCF }, // {Home}{改行}{Space 3}{←}
  {.shift = B_M|B_COMM , .douji = B_G            , .kana = ""                                            , .func = ngh_MCG }, // {Space 3}
  {.shift = B_M|B_COMM , .douji = B_Z            , .kana = NGUE("》")                                    , .func = nofunc }, // 》{改行}
  {.shift = B_M|B_COMM , .douji = B_X            , .kana = NGUE("】")                                    , .func = nofunc }, // 】{改行}
  {.shift = B_M|B_COMM , .douji = B_C            , .kana = ""                                            , .func = ngh_MCC }, // 」{確定}{改行}
  {.shift = B_M|B_COMM , .douji = B_V            , .kana = ""                                            , .func = ngh_MCV }, // 」{確定}{改行}「{確定}
  {.shift = B_M|B_COMM , .douji = B_B            , .kana = ""                                            , .func = ngh_MCB }, // 」{確定}{改行}{Space}
  {.shift = B_X|B_C|B_V      , .douji = B_Y      , .kana = NGU("百")                                     , .func = nofunc }, // 百
  {.shift = B_X|B_C|B_V      , .douji = B_U      , .kana = NGU("七")                                     , .func = nofunc }, // 七
  {.shift = B_X|B_C|B_V      , .douji = B_I      , .kana = NGU("八")                                     , .func = nofunc }, // 八
  {.shift = B_X|B_C|B_V      , .douji = B_O      , .kana = NGU("九")                                     , .func = nofunc }, // 九
  {.shift = B_X|B_C|B_V      , .douji = B_P      , .kana = NGU("億")                                     , .func = nofunc }, // 億
  {.shift = B_X|B_C|B_V      , .douji = B_H      , .kana = NGU("十")                                     , .func = nofunc }, // 十
  {.shift = B_X|B_C|B_V      , .douji = B_J      , .kana = NGU("四")                                     , .func = nofunc }, // 四
  {.shift = B_X|B_C|B_V      , .douji = B_K      , .kana = NGU("五")                                     , .func = nofunc }, // 五
  {.shift = B_X|B_C|B_V      , .douji = B_L      , .kana = NGU("六")                                     , .func = nofunc }, // 六
  {.shift = B_X|B_C|B_V      , .douji = B_SCLN   , .kana = NGU("万")                                     , .func = nofunc }, // 万
  {.shift = B_X|B_C|B_V      , .douji = B_N      , .kana = NGU("〇")                                     , .func = nofunc }, // 〇
  {.shift = B_X|B_C|B_V      , .douji = B_M      , .kana = NGU("一")                                     , .func = nofunc }, // 一
  {.shift = B_X|B_C|B_V      , .douji = B_COMM   , .kana = NGU("二")                                     , .func = nofunc }, // 二
  {.shift = B_X|B_C|B_V      , .douji = B_DOT    , .kana = NGU("三")                                     , .func = nofunc }, // 三
  {.shift = B_X|B_C|B_V      , .douji = B_SLSH   , .kana = NGU("千")                                     , .func = nofunc }, // 千
  {.shift = B_S|B_D|B_F      , .douji = B_Y      , .kana = NGU("数")                                     , .func = nofunc }, // 数
  {.shift = B_S|B_D|B_F      , .douji = B_U      , .kana = NGU("７")                                     , .func = nofunc }, // ７
  {.shift = B_S|B_D|B_F      , .douji = B_I      , .kana = NGU("８")                                     , .func = nofunc }, // ８
  {.shift = B_S|B_D|B_F      , .douji = B_O      , .kana = NGU("９")                                     , .func = nofunc }, // ９
  {.shift = B_S|B_D|B_F      , .douji = B_P      , .kana = NGU("年")                                     , .func = nofunc }, // 年
  {.shift = B_S|B_D|B_F      , .douji = B_H      , .kana = NGU("歳")                                     , .func = nofunc }, // 歳
  {.shift = B_S|B_D|B_F      , .douji = B_J      , .kana = NGU("４")                                     , .func = nofunc }, // ４
  {.shift = B_S|B_D|B_F      , .douji = B_K      , .kana = NGU("５")                                     , .func = nofunc }, // ５
  {.shift = B_S|B_D|B_F      , .douji = B_L      , .kana = NGU("６")                                     , .func = nofunc }, // ６
  {.shift = B_S|B_D|B_F      , .douji = B_SCLN   , .kana = NGU("月")                                     , .func = nofunc }, // 月
  {.shift = B_S|B_D|B_F      , .douji = B_N      , .kana = NGU("０")                                     , .func = nofunc }, // ０
  {.shift = B_S|B_D|B_F      , .douji = B_M      , .kana = NGU("１")                                     , .func = nofunc }, // １
  {.shift = B_S|B_D|B_F      , .douji = B_COMM   , .kana = NGU("２")                                     , .func = nofunc }, // ２
  {.shift = B_S|B_D|B_F      , .douji = B_DOT    , .kana = NGU("３")                                     , .func = nofunc }, // ３
  {.shift = B_S|B_D|B_F      , .douji = B_SLSH   , .kana = NGU("日")                                     , .func = nofunc }, // 日
  {.shift = B_J|B_K|B_L      , .douji = B_Q      , .kana = NGU("外")                                     , .func = nofunc }, // 外
  {.shift = B_J|B_K|B_L      , .douji = B_W      , .kana = NGU("昇")                                     , .func = nofunc }, // 昇
  {.shift = B_J|B_K|B_L      , .douji = B_E      , .kana = NGU("上")                                     , .func = nofunc }, // 上
  {.shift = B_J|B_K|B_L      , .douji = B_R      , .kana = NGU("前")                                     , .func = nofunc }, // 前
  {.shift = B_J|B_K|B_L      , .douji = B_T      , .kana = NGU("内")                                     , .func = nofunc }, // 内
  {.shift = B_J|B_K|B_L      , .douji = B_A      , .kana = NGU("終")                                     , .func = nofunc }, // 終
  {.shift = B_J|B_K|B_L      , .douji = B_S      , .kana = NGU("左")                                     , .func = nofunc }, // 左
  {.shift = B_J|B_K|B_L      , .douji = B_D      , .kana = NGU("以")                                     , .func = nofunc }, // 以
  {.shift = B_J|B_K|B_L      , .douji = B_F      , .kana = NGU("右")                                     , .func = nofunc }, // 右
  {.shift = B_J|B_K|B_L      , .douji = B_G      , .kana = NGU("今")                                     , .func = nofunc }, // 今
  {.shift = B_J|B_K|B_L      , .douji = B_Z      , .kana = NGU("次")                                     , .func = nofunc }, // 次
  {.shift = B_J|B_K|B_L      , .douji = B_X      , .kana = NGU("降")                                     , .func = nofunc }, // 降
  {.shift = B_J|B_K|B_L      , .douji = B_C      , .kana = NGU("下")                                     , .func = nofunc }, // 下
  {.shift = B_J|B_K|B_L      , .douji = B_V      , .kana = NGU("後")                                     , .func = nofunc }, // 後
  {.shift = B_J|B_K|B_L      , .douji = B_B      , .kana = NGU("先")                                     , .func = nofunc }, // 先
  {.shift = B_M|B_COMM|B_DOT , .douji = B_Q      , .kana = NGU("進")                                     , .func = nofunc }, // 進
  {.shift = B_M|B_COMM|B_DOT , .douji = B_W      , .kana = NGU("遠")                                     , .func = nofunc }, // 遠
  {.shift = B_M|B_COMM|B_DOT , .douji = B_E      , .kana = NGU("止")                                     , .func = nofunc }, // 止
  {.shift = B_M|B_COMM|B_DOT , .douji = B_R      , .kana = NGU("同")                                     , .func = nofunc }, // 同
  {.shift = B_M|B_COMM|B_DOT , .douji = B_T      , .kana = NGU("高")                                     , .func = nofunc }, // 高
  {.shift = B_M|B_COMM|B_DOT , .douji = B_A      , .kana = NGU("停")                                     , .func = nofunc }, // 停
  {.shift = B_M|B_COMM|B_DOT , .douji = B_S      , .kana = NGU("小")                                     , .func = nofunc }, // 小
  {.shift = B_M|B_COMM|B_DOT , .douji = B_D      , .kana = NGU("中")                                     , .func = nofunc }, // 中
  {.shift = B_M|B_COMM|B_DOT , .douji = B_F      , .kana = NGU("最")                                     , .func = nofunc }, // 最
  {.shift = B_M|B_COMM|B_DOT , .douji = B_G      , .kana = NGU("異")                                     , .func = nofunc }, // 異
  {.shift = B_M|B_COMM|B_DOT , .douji = B_Z      , .kana = NGU("退")                                     , .func = nofunc }, // 退
  {.shift = B_M|B_COMM|B_DOT , .douji = B_X      , .kana = NGU("近")                                     , .func = nofunc }, // 近
  {.shift = B_M|B_COMM|B_DOT , .douji = B_C      , .kana = NGU("心")                                     , .func = nofunc }, // 心
  {.shift = B_M|B_COMM|B_DOT , .douji = B_V      , .kana = NGU("違")                                     , .func = nofunc }, // 違
  {.shift = B_M|B_COMM|B_DOT , .douji = B_B      , .kana = NGU("低")                                     , .func = nofunc }, // 低
};

// 漢直テーブル(dvorakj定義のJP106配置を移植)
// [キーコード - NG_Q]で引く。""は割当なし
// 並びは NG_Q..NG_P / NG_A..NG_SCLN / NG_Z..NG_SLSH
// 第1面: 変換
static const char ngmapuni1[30][4] PROGMEM = {
  "気","的","出","知","字","漢","覚","良","意","入",
  "全","見","取","感","書","来","会","言","打","部",
  "方","人","話",""  ,"自","思",""  ,"考","分","風",
};
// 第2面: 変換+M (Mの位置は「×」+BS)
static const char ngmapuni2[30][4] PROGMEM = {
  "聞","効","点","面","時","銀","金","色","動","用",
  "線","計","撮","間","描","明","白","行","使","運",
  "初","早","離","始","速","々","×","何","悪","体",
};
// 第3面: 変換+V (Vの位置は「×」+BS)
static const char ngmapuni3[30][4] PROGMEM = {
  "個","決","定","赤","青","対","起","解","味","変",
  "手","系","事","黒","暗","大","合","者","詰","深",
  "本","発","場","×","回","多","慣","難","笑","所",
};

// 薙刀式のレイヤー、オンオフするキー
// 後方で定義している関数の前方宣言
static void ng_type(NGList *keys);
static int number_of_matches(NGList *keys);
static int number_of_candidates(NGList *keys);
static void naginata_clear(void);

void set_naginata(uint8_t layer, uint16_t *onk, uint16_t *offk) {
  naginata_layer = layer;
  ngon_keys[0] = *onk;
  ngon_keys[1] = *(onk+1);
  ngoff_keys[0] = *offk;
  ngoff_keys[1] = *(offk+1);

  initializeListArray(&nginput);

  naginata_config.raw = eeconfig_read_user();
  if (naginata_config.os != NG_WIN && naginata_config.os != NG_MAC && naginata_config.os != NG_LINUX) {
    naginata_config.os = NG_WIN;
    naginata_config.tategaki = 1;
    eeconfig_update_user(naginata_config.raw);
  }
  ng_set_unicode_mode(naginata_config.os);
}

// 内部状態(フラグとレイヤー)だけを切り替える。ホストにはキーを送らない
static void ng_apply_state(bool on) {
  is_naginata = on;
  naginata_clear();
  if (on) {
    layer_on(naginata_layer);
  } else {
    layer_off(naginata_layer);
  }
}

// 薙刀式をオン
void naginata_on(void) {
  ng_apply_state(true);

  if (naginata_config.os == NG_MAC) {
    tap_code(KC_LANGUAGE_1); // かな
  } else {
    tap_code(KC_INTERNATIONAL_4); // 変換
  }
}

// 薙刀式をオフ
void naginata_off(void) {
  ng_apply_state(false);

  if (naginata_config.os == NG_MAC) {
    tap_code(KC_LANGUAGE_2); // 英数
  } else {
    tap_code(KC_INTERNATIONAL_5); // 無変換
  }
}

// ホストのIME状態に合わせる(MacUnicodeInputからのraw HID通知で呼ばれる)。
// かな/英数のキーを送り返さないので、ホストとの間でループしない
void naginata_sync_state(bool on) {
  if (is_naginata == on) return;
  ng_apply_state(on);
}

// 薙刀式のon/off状態を返す
bool naginata_state(void) {
  return is_naginata;
}

void switchOS(uint8_t os) {
  naginata_config.os = os;
  eeconfig_update_user(naginata_config.raw);
  ng_set_unicode_mode(naginata_config.os);
}

void ng_set_unicode_mode(uint8_t os) {
  switch (os) {
    case NG_WIN:
      set_unicode_input_mode(UNICODE_MODE_WINCOMPOSE);
      break;
    case NG_MAC:
      set_unicode_input_mode(UNICODE_MODE_MACOS);
      break;
    case NG_LINUX:
      set_unicode_input_mode(UNICODE_MODE_LINUX);
      break;
  }
}

void tategaki_toggle() {
  naginata_config.tategaki ^= 1;
  eeconfig_update_user(naginata_config.raw);
}

void ng_show_os(void) {
  switch (naginata_config.os) {
    case NG_WIN:
      send_string("win");
      break;
    case NG_MAC:
      send_string("mac");
      break;
    case NG_LINUX:
      send_string("linux");
      break;
  }
  if (naginata_config.tategaki) {
    send_string("/tate");
  } else {
    send_string("/yoko");
  }
}

// modifierが押されたら薙刀式レイヤーをオフしてベースレイヤーに戻す
// get_mods()がうまく動かない
static int n_modifier = 0;

static bool process_modifier(uint16_t keycode, keyrecord_t *record) {
  if (IS_MODIFIER_KEYCODE(keycode) || IS_QK_MOD_TAP(keycode)) {
    if (record->event.pressed) {
      n_modifier++;
      layer_off(naginata_layer);
    } else {
      n_modifier--;
      if (n_modifier <= 0) {
        n_modifier = 0;
        layer_on(naginata_layer);
      }
    }
    return true;
  }
  return false;
}

static uint16_t fghj_buf = 0; // 押しているJかKのキーコード
static uint8_t nkeypress = 0; // 同時にキーを押している数

// 薙刀式の起動処理(容量が大きいCOMBOを使わない)
static bool enable_naginata(uint16_t keycode, keyrecord_t *record) {
  // キープレス
  if (record->event.pressed) {
    nkeypress++;
    // 1キー目、JKの前に他のキーを押していないこと
    if (fghj_buf == 0 && nkeypress == 1) {
      // かなオンキーの場合
      if (keycode == ngon_keys[0] || keycode == ngon_keys[1] || keycode == ngoff_keys[0] || keycode == ngoff_keys[1]) {
        fghj_buf = keycode;
        return false;
      }
    // ２キー目
    } else {
      // ２キー目、１キー目、両方ともかなオンキー
      if ((keycode == ngon_keys[0] && fghj_buf == ngon_keys[1]) ||
          (keycode == ngon_keys[1] && fghj_buf == ngon_keys[0])) {
        naginata_on();
        fghj_buf = 0;
        nkeypress = 0;
        return false;
      } else if ((keycode == ngoff_keys[0] && fghj_buf == ngoff_keys[1]) ||
          (keycode == ngoff_keys[1] && fghj_buf == ngoff_keys[0])) {
        naginata_off();
        fghj_buf = 0;
        nkeypress = 0;
        return false;
      // ２キー目はかなオンキーではない
      } else {
        // 修正: fghj_bufが0のときtap_code(KC_NO)を送らないようガード
        if (fghj_buf > 0) tap_code(fghj_buf); // 1キー目を出力
        fghj_buf = 0;
        nkeypress = 0;
        return true; // 2キー目はQMKにまかせる
      }
    }
  } else {
    nkeypress = 0;
    // J/K単押しだった
    if (fghj_buf > 0) {
      tap_code(fghj_buf);
      fghj_buf = 0;

      // Shift + Jで、先にShiftを外した場合にShiftがリリースされない不具合対策
      if (IS_MODIFIER_KEYCODE(keycode)) {
        unregister_code(keycode);
      } else if (IS_QK_MOD_TAP(keycode)) {
        if (keycode & (MOD_LCTL << 8))
          unregister_code(KC_LEFT_CTRL);
        if (keycode & (MOD_LSFT << 8))
          unregister_code(KC_LEFT_SHIFT);
        if (keycode & (MOD_LALT << 8))
          unregister_code(KC_LEFT_ALT);
        if (keycode & (MOD_LGUI << 8))
          unregister_code(KC_LEFT_GUI);
        if (keycode & (MOD_RCTL << 8))
          unregister_code(KC_RIGHT_CTRL);
        if (keycode & (MOD_RSFT << 8))
          unregister_code(KC_RIGHT_SHIFT);
        if (keycode & (MOD_RALT << 8))
          unregister_code(KC_RIGHT_ALT);
        if (keycode & (MOD_RGUI << 8))
          unregister_code(KC_RIGHT_GUI);
      }
      return false;
    }
  }

  fghj_buf = 0;
  return true;
}

// バッファをクリアする
static void naginata_clear(void) {
  initializeListArray(&nginput);
  n_modifier = 0;
  nkeypress = 0;
  fghj_buf = 0;
  pressed_keys = 0;
  n_pressed_keys = 0;
  last_press_time = 0;
  last_pressed_kc = 0;
  defer_flush = false;
  ngu_state = NGU_NONE;
  int4_held = false;
  ngu_used_while_held = false;
  ngu_consumed = 0;
}

// 薙刀式の入力処理
bool process_naginata(uint16_t keycode, keyrecord_t *record) {

  if (record->event.pressed) {
    n_pressed_keys++;
  } else {
    if (n_pressed_keys > 0) {
      n_pressed_keys--;
    }
  }
  if (n_pressed_keys == 0)
    pressed_keys = 0;

  ngdbg(">process_naginata pressed_keys=%lu, %d\n", pressed_keys, n_pressed_keys);

  // まれに薙刀モードオンのまま、レイヤーがオフになることがあるので、対策
  if (n_modifier == 0 && is_naginata && !layer_state_is(naginata_layer))
    layer_on(naginata_layer);
  if (n_modifier == 0 && !is_naginata && layer_state_is(naginata_layer))
    layer_off(naginata_layer);
  if (n_modifier > 0 && layer_state_is(naginata_layer))
    layer_off(naginata_layer);

  // OS切り替え(UNICODE出力)
  if (record->event.pressed) {
    switch (keycode) {
      case NGSW_WIN:
        switchOS(NG_WIN);
        return false;
        break;
      case NGSW_MAC:
        switchOS(NG_MAC);
        return false;
        break;
      case NGSW_LNX:
        switchOS(NG_LINUX);
        return false;
        break;
      case NG_SHOS:
        ng_show_os();
        return false;
        break;
      case NG_TAYO:
        tategaki_toggle();
        return false;
        break;
    }
  }

  if (!is_naginata)
    // return true;
    return enable_naginata(keycode, record);

  if (process_modifier(keycode, record))
    return true;

  // 漢直(Unicode直接入力): 変換(KC_INT4)をプレフィックスに3面を切り替える
  if (record->event.pressed) {
    if (keycode == KC_INT4) {
      // 保留中のかなを確定出力してから漢直待機に入る(出力順序の逆転防止)
      while (nginput.size > 0) {
        ng_type(&(nginput.elements[0]));
        removeFromListArrayAt(&nginput, 0);
      }
      defer_flush = false;
      int4_held = true;
      ngu_used_while_held = false;
      ngu_state = NGU_PLANE1;
      return false;
    }
    // 変換押下中のM/Vは面切替キー
    if (ngu_state == NGU_PLANE1 && int4_held && (keycode == NG_M || keycode == NG_V)) {
      ngu_state = (keycode == NG_M) ? NGU_PLANE2 : NGU_PLANE3;
      ngu_consumed |= NG_BIT(keycode);
      return false;
    }
    if (ngu_state != NGU_NONE && keycode >= NG_Q && keycode <= NG_SLSH) {
      const char *ustr = NULL;
      bool ubs = false; // ×{BS}: 送出後にBSを1回送る
      switch (ngu_state) {
        case NGU_PLANE1: ustr = ngmapuni1[keycode - NG_Q]; break;
        case NGU_PLANE2: ustr = ngmapuni2[keycode - NG_Q]; ubs = (keycode == NG_M); break;
        case NGU_PLANE3: ustr = ngmapuni3[keycode - NG_Q]; ubs = (keycode == NG_V); break;
        default: break;
      }
      if (ustr != NULL && pgm_read_byte(ustr) != 0) {
        ng_send_unicode_string_P(ustr);
        if (ubs) tap_code(KC_BSPC);
        ngu_consumed |= NG_BIT(keycode);
        // 変換キーを押している間は面を維持して連続入力できる(離した時に解除)。
        // タップ(何も打たず離した)後はワンショット(1文字で解除)
        if (int4_held) {
          ngu_used_while_held = true;
        } else {
          ngu_state = NGU_NONE;
        }
        return false;
      }
      // 割当なしのキーは待機を解除して通常のかな処理へフォールスルー
      ngu_state = NGU_NONE;
    }
  } else {
    if (keycode == KC_INT4) {
      int4_held = false;
      // 押している間に漢字を打っていたら解除。何も打たずに離したら
      // リーダーキーとして次の1キーだけ有効(状態を保持)
      if (ngu_used_while_held) {
        ngu_state = NGU_NONE;
        ngu_used_while_held = false;
      }
      return false;
    }
    // pressを消費したキーのreleaseも消費する
    if (keycode >= NG_Q && keycode <= NG_SLSH && (ngu_consumed & NG_BIT(keycode))) {
      ngu_consumed &= ~NG_BIT(keycode);
      return false;
    }
  }

  if (keycode >= NG_Q && keycode <= NG_SHFT2) {
    if (record->event.pressed) {
  ngdbg(">process_naginata pressed=%u nginput.size=%u\n", keycode, nginput.size);

      pressed_keys |= NG_BIT(keycode); // キーの重ね合わせ

      if (keycode == NG_SHFT || keycode == NG_SHFT2) {
        NGList a;
        initializeList(&a);
        addToList(&a, keycode);
        addToListArray(&nginput, &a);
      } else {
        last_pressed_kc = keycode;
        last_press_time = record->event.time;
        NGList a;
        NGList b;
        if (nginput.size > 0) {
          copyList(&(nginput.elements[nginput.size - 1]), &a);
          copyList(&a, &b);
          addToList(&b, keycode);
        }

        // 前のキーとの同時押しの可能性があるなら前に足す
        // 同じキー連打を除外
        if (nginput.size > 0 && a.elements[a.size - 1] != keycode && number_of_candidates(&b) > 0) {
          removeFromListArrayAt(&nginput, nginput.size - 1);
          addToListArray(&nginput, &b);
        // 前のキーと同時押しはない
        } else {
          // 連続シフトではない
          NGList e;
          initializeList(&e);
          addToList(&e, keycode);
          addToListArray(&nginput, &e);
        }
      }

      // 連続シフト
      // 3キーシフトは2キーシフトの上位集合(SDF⊃DF等)なので、先に並べて優先させる
      static const uint16_t rs[14][3] = {
        {NG_S, NG_D, NG_F}, {NG_X, NG_C, NG_V}, {NG_J, NG_K, NG_L}, {NG_M, NG_COMM, NG_DOT},
        {NG_D, NG_F, 0}, {NG_C, NG_V, 0}, {NG_J, NG_K, 0}, {NG_M, NG_COMM, 0},
        {NG_SHFT, 0, 0}, {NG_SHFT2, 0, 0},
        {NG_F, 0, 0}, {NG_V, 0, 0}, {NG_J, 0, 0}, {NG_M, 0, 0}};
      uint32_t keyset = 0UL;
      for (int i = 0; i < nginput.elements[nginput.size - 1].size; i++) {
        keyset |= NG_BIT(nginput.elements[nginput.size - 1].elements[i]);
      }
      for (int i = 0; i < sizeof rs / sizeof rs[0]; i++) {
        NGList rskc;
        initializeList(&rskc);
        for (int j = 0; j < 3; j++) {
          if (rs[i][j] == 0) break;
          addToList(&rskc, rs[i][j]);
        }

        int c = includeList(&rskc, keycode);
        uint32_t brs = 0UL;
        for (int j = 0; j < rskc.size; j++) {
          brs |=  NG_BIT(rskc.elements[j]);
        }

        NGList l = nginput.elements[nginput.size - 1];
        for (int j = 0; j < l.size; j++) {
          addToList(&rskc, l.elements[j]);
        }

        if (c <  0 && ((brs & pressed_keys) == brs) &&  (keyset & brs) != brs && number_of_matches(&rskc) >  0) {
          nginput.elements[nginput.size - 1] = rskc;
          break;
        }
      }

      if (nginput.size > 1) {
        ng_type(&(nginput.elements[0]));
        removeFromListArrayAt(&nginput, 0);
        defer_flush = false;
      } else if (number_of_candidates(&(nginput.elements[0])) == 1) {
        // 文字キー同士の結合は、重なり時間が閾値未満なら個別打鍵に分割される
        // 可能性があるため、確定出力を保留する(naginata_taskで閾値経過後に出力)。
        // シフトキーとの結合は分割対象外なので従来通り即出力する
        int n_moji = 0;
        for (int i = 0; i < nginput.elements[0].size; i++) {
          if (nginput.elements[0].elements[i] != NG_SHFT && nginput.elements[0].elements[i] != NG_SHFT2) {
            n_moji++;
          }
        }
        if (NG_MIN_OVERLAP_MS > 0 && n_moji >= 2) {
          defer_flush = true;
        } else {
          ng_type(&(nginput.elements[0]));
          removeFromListArrayAt(&nginput, 0);
        }
      }

  ngdbg("<process_naginata pressed=%u nginput.size=%u\n", keycode, nginput.size);

    } else { // key release
  ngdbg(">process_naginata released=%u nginput.size=%u\n", keycode, nginput.size);

      // 先に押していた文字キーが、後から押した文字キーとの重なり閾値未満で
      // 離された場合(ロールオーバー)、同時押しではなく個別打鍵として分割する
      if (NG_MIN_OVERLAP_MS > 0 && nginput.size > 0 &&
          keycode != NG_SHFT && keycode != NG_SHFT2) {
        NGList *last = &(nginput.elements[nginput.size - 1]);
        if (last->size >= 2 &&
            keycode != last_pressed_kc &&
            last->elements[last->size - 1] == last_pressed_kc &&
            includeList(last, keycode) >= 0 &&
            TIMER_DIFF_16(record->event.time, last_press_time) < NG_MIN_OVERLAP_MS) {
          last->size--; // 末尾(最後に押したキー)を外し、独立要素として追加
          NGList e;
          initializeList(&e);
          addToList(&e, last_pressed_kc);
          addToListArray(&nginput, &e);
          defer_flush = false;
          // 分割により手前の要素はこれ以上結合されないので確定出力する
          while (nginput.size > 1) {
            ng_type(&(nginput.elements[0]));
            removeFromListArrayAt(&nginput, 0);
          }
        }
      }

      pressed_keys &= ~NG_BIT(keycode); // キーの重ね合わせ

      if (pressed_keys == 0UL) {
        while (nginput.size > 0) {
          ng_type(&(nginput.elements[0]));
          removeFromListArrayAt(&nginput, 0);
        }
        defer_flush = false;
      } else {
        if (nginput.size > 0 && number_of_candidates(&(nginput.elements[0])) == 1) {
          ng_type(&(nginput.elements[0]));
          removeFromListArrayAt(&nginput, 0);
          defer_flush = false;
        }
      }

  ngdbg("<process_naginata released=%u nginput.size=%u\n", keycode, nginput.size);
    }
    return false;
  }

  return true;
}

// 保留中の確定出力を、重なり閾値の経過後に行う(キーイベント外のタイマー処理)
void naginata_task(void) {
  if (defer_flush && timer_elapsed(last_press_time) >= NG_MIN_OVERLAP_MS) {
    defer_flush = false;
    if (nginput.size > 0 && number_of_candidates(&(nginput.elements[0])) == 1) {
      ng_type(&(nginput.elements[0]));
      removeFromListArrayAt(&nginput, 0);
    }
  }
}

void housekeeping_task_user(void) {
  naginata_task();
}

// キー入力を文字に変換して出力する
static void ng_type(NGList *keys) {
  ngdbg(">ng_type size=%u\n", keys->size);
  if (keys->size == 0) return;

  naginata_kanamap bngdickana;

  if (keys->size == 1 && keys->elements[0] == NG_SHFT2) {
    tap_code16(KC_ENT);
    return;
  }

  bool ftype = false;
  uint32_t keyset = 0UL;
  for (int i = 0; i < keys->size; i++) {
    keyset |= NG_BIT(keys->elements[i]);
  }
  for (int i = 0; i < sizeof ngdickana / sizeof bngdickana; i++) {
    memcpy_P(&bngdickana, &ngdickana[i], sizeof(bngdickana));
    if ((bngdickana.shift | bngdickana.douji) == keyset) {
      if (bngdickana.kana[0] == NGU_TAG || bngdickana.kana[0] == NGUE_TAG) {
        // bngdickanaはmemcpy_P済みなのでRAM側の関数を呼ぶ
        ng_send_unicode_string(bngdickana.kana + 1);
        if (bngdickana.kana[0] == NGUE_TAG) ng_kakutei();
      } else if (bngdickana.kana[0] > 0) {
        send_string(bngdickana.kana);
      } else {
        bngdickana.func();
      }
      ftype = true;
      break;
    }
  }
  // JIみたいにJIを含む同時押しはたくさんあるが、JIのみの同時押しがないとき
  // 最後の１キーを別に分けて変換する
  if (!ftype) {
    // 修正: 1キーで一致しない場合はこれ以上分割できないので打ち切る。
    if (keys->size <= 1) return;
    NGList a, b;
    initializeList(&a);
    initializeList(&b);
    for (int i = 0; i < keys->size - 1; i++) {
      addToList(&a, keys->elements[i]);
    }
    addToList(&b, keys->elements[keys->size - 1]);
    ng_type(&a);
    ng_type(&b);
  }

  ngdbg("<ng_type\n");
}

// Helper function for counting matches/candidates
static int count_kana_entries(NGList *keys, bool exact_match) {
  if (keys->size == 0) return 0;

  naginata_kanamap bngdickana;
  int count = 0;

  // 先頭からp個のキーの重ね合わせをprefix[p]に作っておく。
  // 「先頭p個をシフト、残りを同時押しキー」とみなして分割位置pを変えながら照合する。
  // シフトのキー数(2キーまで/3キーのXCV,SDF,JKL,MCDなど)に依存しないようにするため、
  // サイズ別のベタ書きではなくループで扱う
  uint32_t prefix[LIST_SIZE + 1];
  prefix[0] = 0UL;
  for (int i = 0; i < keys->size; i++) {
    prefix[i + 1] = prefix[i] | NG_BIT(keys->elements[i]);
  }
  const uint32_t keyset = prefix[keys->size];

  for (int i = 0; i < sizeof ngdickana / sizeof bngdickana; i++) {
    memcpy_P(&bngdickana, &ngdickana[i], sizeof(bngdickana));
    bool matches = false;

    for (int p = keys->size; p >= 0; p--) {
      if (p == keys->size) {
        // 押されているキーがすべてシフト。同時押しキーはまだ押されていない
        // 候補判定では途中まで押されたシフト(XCの次にVが来る)も認める
        matches = exact_match ? (bngdickana.shift == keyset)
                              : ((bngdickana.shift & keyset) == keyset);
      } else {
        if (bngdickana.shift != prefix[p]) continue;
        uint32_t douji = keyset & ~prefix[p];
        matches = exact_match ? (bngdickana.douji == douji)
                              : ((bngdickana.douji & douji) == douji);
      }
      if (matches) break;
    }

    if (matches) {
      // しぇ、ちぇ、3キーシフトの途中など、まだ足りないときは確定してはいけない
      if (!exact_match && keys->size >= 2 &&
          (bngdickana.shift | bngdickana.douji) != keyset) {
        count = 2;
      }
      count++;
      if (count > 1) break;
    }
  }

  return count;
}

static int number_of_matches(NGList *keys) {
  ngdbg(">number_of_matches\n");

  int result = count_kana_entries(keys, true);

  ngdbg("<number_of_matches nom=%u\n", result);
  return result;
}

static int number_of_candidates(NGList *keys) {
  ngdbg(">number_of_candidates\n");

  int result = count_kana_entries(keys, false);

  ngdbg("<number_of_candidates noc=%u\n", result);
  return result;
}

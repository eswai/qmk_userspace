/*
 * (C) 2020 Sadao Ikebe @bonyarou
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
#include "key_duration.h"
#include "deferred_exec.h"

static keypress_timer_expired_func_t callback;
static deferred_token keypress_timer_token;

static uint32_t keypress_timer_deferred_callback(uint32_t trigger_time, void *cb_arg) {
    (void)trigger_time;
    (void)cb_arg;
    callback();
    return 0;
}

void keypress_timer_init(keypress_timer_expired_func_t _clbk) {
    callback = _clbk;
    keypress_timer_token = 0;
}

void keypress_timer_start(uint16_t count) {
    if (keypress_timer_token) {
        cancel_deferred_exec(keypress_timer_token);
    }
    keypress_timer_token = defer_exec((count + 15) / 16, keypress_timer_deferred_callback, NULL);
}

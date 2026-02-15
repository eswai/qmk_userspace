SRC +=  nicola.c \
        jtu.c \
        key_duration.c

# deferred_exec を使用したタイムアウト
# 一定時間経過すると、key down 状態のままでもキーが出力される
OPT_DEFS = -DTIMEOUT_INTERRUPT
DEFERRED_EXEC_ENABLE = yes

# 既存設定を維持
BACKLIGHT_ENABLE = no       # Enable keyboard backlight functionality
SEND_STRING_ENABLE = yes
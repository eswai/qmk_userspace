UNICODE_ENABLE = yes
# CONSOLE_ENABLE = yes
# COMMAND_ENABLE = yes

# If you want to change the display of OLED, you need to change here
SRC +=  naginata_v18.c

# MacUnicodeInputとかな/英数を連動させる。使うキーマップのrules.mkでRAW_ENABLE
ifeq ($(strip $(RAW_ENABLE)), yes)
SRC +=  naginata_hid.c
endif
SRC +=  twpair_on_jis.c
SRC +=  nglist.c
SRC +=  nglistarray.c

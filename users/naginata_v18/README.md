# QMK薙刀式 v18トップ版

薙刀式v18のQMK実装です。

【薙刀式】v18トップ版、発表。
https://oookaworks.seesaa.net/article/521080503.html

## Unicodeによる記号入力

編集モードの記号入力には、WindowsではWinCompose、macOSではIshizukiを利用します。
macOSは`naginata_v18_ok`と同じ方式でUnicode入力を送ります。
Karabiner-ElementsによるUnicode Hex Inputへの切り替え設定は不要です。

https://github.com/eswai/Ishizuki

## オーバーラップ処理(短い重なりの個別打鍵判定)

キープレスが少しでも重なると同時押しと判定するのではなく、重なり時間が閾値
`NG_MIN_OVERLAP_MS`未満なら、同時押しではなく個別の打鍵として
扱います。高速なロールオーバー打鍵(例: F→U をわずかに重ねて打つ)が
「ざ」に誤爆せず「か」+「BS」になります。
0に設定すると、従来通りの同時押し判定になります。

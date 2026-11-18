// SPDX-License-Identifier: MIT
.section .rodata
.balign 16
.balign 16
.global dnx2_font_sans
dnx2_font_sans:
.incbin "assets/fonts/NotoSans[wdth,wght].ttf"
.balign 16
.global dnx2_font_math
dnx2_font_math:
.incbin "assets/fonts/NotoSansMath-Regular.ttf"
.balign 16
.global dnx2_font_symbols
dnx2_font_symbols:
.incbin "assets/fonts/NotoSansSymbols2-Regular.ttf"
.balign 16
.global dnx2_font_emoji
dnx2_font_emoji:
.incbin "assets/fonts/NotoEmoji[wght].ttf"
.section .note.GNU-stack,"",%progbits

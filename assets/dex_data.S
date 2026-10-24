.section .rodata
.global dex_data_start
.global dex_data_end
dex_data_start:
.incbin "classes.dex"
dex_data_end:

.file "getfp.s"
.text
.align 2
.global get_fp
.type   get_fp, %function
get_fp:
    mov r0, fp
    mov pc, lr

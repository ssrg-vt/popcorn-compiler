.section .text.log10l, "ax"
.global log10l
.type log10l,@function
log10l:
	fldlg2
	fldt 8(%rsp)
	fyl2x
	ret

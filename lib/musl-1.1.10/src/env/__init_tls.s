
__init_tls.o:     file format elf64-littleaarch64


Disassembly of section .text.__copy_tls:

0000000000000000 <__copy_tls>:
   0:	90000003 	adrp	x3, 20 <__copy_tls+0x20>
   4:	a9bf7bf3 	stp	x19, x30, [sp,#-16]!
   8:	91000064 	add	x4, x3, #0x0
   c:	aa0003f3 	mov	x19, x0
  10:	f9400061 	ldr	x1, [x3]
  14:	b4000261 	cbz	x1, 60 <__copy_tls+0x60>
  18:	aa0003e2 	mov	x2, x0
  1c:	d2800020 	mov	x0, #0x1                   	// #1
  20:	f9000260 	str	x0, [x19]
  24:	90000000 	adrp	x0, 0 <__libc>
  28:	f9400c81 	ldr	x1, [x4,#24]
  2c:	f9400013 	ldr	x19, [x0]
  30:	d1000421 	sub	x1, x1, #0x1
  34:	f9400880 	ldr	x0, [x4,#16]
  38:	d1098273 	sub	x19, x19, #0x260
  3c:	8b130053 	add	x19, x2, x19
  40:	8a210273 	bic	x19, x19, x1
  44:	f9400061 	ldr	x1, [x3]
  48:	cb000260 	sub	x0, x19, x0
  4c:	f900a662 	str	x2, [x19,#328]
  50:	f9000662 	str	x2, [x19,#8]
  54:	f9000440 	str	x0, [x2,#8]
  58:	f9400482 	ldr	x2, [x4,#8]
  5c:	94000000 	bl	0 <memcpy>
  60:	aa1303e0 	mov	x0, x19
  64:	a8c17bf3 	ldp	x19, x30, [sp],#16
  68:	d65f03c0 	ret

Disassembly of section .text.__init_tls:

000000000000006c <__init_tls>:
  6c:	a9be53f3 	stp	x19, x20, [sp,#-32]!
  70:	d2800003 	mov	x3, #0x0                   	// #0
  74:	d2800001 	mov	x1, #0x0                   	// #0
  78:	f9000bfe 	str	x30, [sp,#16]
  7c:	f9400c05 	ldr	x5, [x0,#24]
  80:	f9401404 	ldr	x4, [x0,#40]
  84:	aa0503e2 	mov	x2, x5
  88:	b40001a4 	cbz	x4, bc <__init_tls+0x50>
  8c:	b9400046 	ldr	w6, [x2]
  90:	710018df 	cmp	w6, #0x6
  94:	54000081 	b.ne	a4 <__init_tls+0x38>
  98:	f9400843 	ldr	x3, [x2,#16]
  9c:	cb0300a3 	sub	x3, x5, x3
  a0:	14000003 	b	ac <__init_tls+0x40>
  a4:	71001cdf 	cmp	w6, #0x7
  a8:	9a821021 	csel	x1, x1, x2, ne
  ac:	f9401006 	ldr	x6, [x0,#32]
  b0:	d1000484 	sub	x4, x4, #0x1
  b4:	8b060042 	add	x2, x2, x6
  b8:	17fffff4 	b	88 <__init_tls+0x1c>
  bc:	90000002 	adrp	x2, 20 <__copy_tls+0x20>
  c0:	b4000161 	cbz	x1, ec <__init_tls+0x80>
  c4:	f9400824 	ldr	x4, [x1,#16]
  c8:	91000040 	add	x0, x2, #0x0
  cc:	8b040063 	add	x3, x3, x4
  d0:	f9000043 	str	x3, [x2]
  d4:	f9401023 	ldr	x3, [x1,#32]
  d8:	f9000403 	str	x3, [x0,#8]
  dc:	f9401423 	ldr	x3, [x1,#40]
  e0:	f9401821 	ldr	x1, [x1,#48]
  e4:	f9000803 	str	x3, [x0,#16]
  e8:	f9000c01 	str	x1, [x0,#24]
  ec:	91000043 	add	x3, x2, #0x0
  f0:	f9400041 	ldr	x1, [x2]
  f4:	f9400860 	ldr	x0, [x3,#16]
  f8:	f9400c65 	ldr	x5, [x3,#24]
  fc:	8b000021 	add	x1, x1, x0
 100:	cb0103e1 	neg	x1, x1
 104:	d10004a4 	sub	x4, x5, #0x1
 108:	8a040021 	and	x1, x1, x4
 10c:	f1001cbf 	cmp	x5, #0x7
 110:	8b000020 	add	x0, x1, x0
 114:	f9000860 	str	x0, [x3,#16]
 118:	54000068 	b.hi	124 <__init_tls+0xb8>
 11c:	d2800101 	mov	x1, #0x8                   	// #8
 120:	f9000c61 	str	x1, [x3,#24]
 124:	91000042 	add	x2, x2, #0x0
 128:	f9400c41 	ldr	x1, [x2,#24]
 12c:	9109dc21 	add	x1, x1, #0x277
 130:	8b000020 	add	x0, x1, x0
 134:	927df001 	and	x1, x0, #0xfffffffffffffff8
 138:	90000000 	adrp	x0, 0 <__libc>
 13c:	91000002 	add	x2, x0, #0x0
 140:	f10ba03f 	cmp	x1, #0x2e8
 144:	aa0003f4 	mov	x20, x0
 148:	f9001441 	str	x1, [x2,#40]
 14c:	54000129 	b.ls	170 <__init_tls+0x104>
 150:	d2801bc8 	mov	x8, #0xde                  	// #222
 154:	d2800000 	mov	x0, #0x0                   	// #0
 158:	d2800062 	mov	x2, #0x3                   	// #3
 15c:	d2800443 	mov	x3, #0x22                  	// #34
 160:	92800004 	mov	x4, #0xffffffffffffffff    	// #-1
 164:	d2800005 	mov	x5, #0x0                   	// #0
 168:	d4000001 	svc	#0x0
 16c:	14000003 	b	178 <__init_tls+0x10c>
 170:	90000000 	adrp	x0, 0 <__copy_tls>
 174:	91000000 	add	x0, x0, #0x0
 178:	94000000 	bl	0 <__copy_tls>
 17c:	aa0003f3 	mov	x19, x0
 180:	f9000260 	str	x0, [x19]
 184:	94000000 	bl	0 <__set_thread_area>
 188:	6b1f001f 	cmp	w0, wzr
 18c:	5400020b 	b.lt	1cc <__init_tls+0x160>
 190:	54000061 	b.ne	19c <__init_tls+0x130>
 194:	52800020 	mov	w0, #0x1                   	// #1
 198:	b9000280 	str	w0, [x20]
 19c:	d2800c08 	mov	x8, #0x60                  	// #96
 1a0:	9100e260 	add	x0, x19, #0x38
 1a4:	d4000001 	svc	#0x0
 1a8:	b9003a60 	str	w0, [x19,#56]
 1ac:	90000000 	adrp	x0, 0 <__libc>
 1b0:	91000000 	add	x0, x0, #0x0
 1b4:	f9008260 	str	x0, [x19,#256]
 1b8:	91038260 	add	x0, x19, #0xe0
 1bc:	f9007260 	str	x0, [x19,#224]
 1c0:	f9400bfe 	ldr	x30, [sp,#16]
 1c4:	a8c253f3 	ldp	x19, x20, [sp],#32
 1c8:	d65f03c0 	ret
 1cc:	d2800000 	mov	x0, #0x0                   	// #0
 1d0:	3900001f 	strb	wzr, [x0]
 1d4:	d4207d00 	brk	#0x3e8

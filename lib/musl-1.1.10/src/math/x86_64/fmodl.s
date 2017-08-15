/* Pierre: -ffunction-section obviously does not work with as so let's mimic
 * its effect by hand. This one is tricky as the aarch64 equivalent is defined 
 * in a C file so -ffunction-sections works for arm but not for this assembly 
 * file! Thus, we need to take care of giving the same name as arm here. 
 * For power, it should be fine too but need to check once compiler support is
 * here */

.section .text.fmodl, "ax"
.global fmodl
.type fmodl,@function
fmodl:
	fldt 24(%rsp)
	fldt 8(%rsp)
1:	fprem
	fnstsw %ax
	testb $4,%ah
	jnz 1b
	fstp %st(1)
	ret

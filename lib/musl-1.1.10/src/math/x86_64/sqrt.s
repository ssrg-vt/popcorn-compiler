/* Pierre: -ffunction-section obviously does not work with as so let's mimic
 * its effect by hand. */

.section .text.sqrt, "ax"
.global sqrt
.type sqrt,@function
sqrt:	sqrtsd %xmm0, %xmm0
	ret

/* Pierre: -ffunction-section obviously does not work with as so let's mimic
 * its effect by hand. */
.section .text.sqrt, "ax"
/* .text */
.global sqrt
.type   sqrt,%function
sqrt:
	fsqrt d0, d0
	ret

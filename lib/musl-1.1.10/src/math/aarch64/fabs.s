/* Pierre: -ffunction-section obviously does not work with as so let's mimic
 * its effect by hand. There is no power8 specific function for fabs, meaning
 * it will probably use the dfault implementation in math/fabs.c. When power8 
 * support is implemented we need to check what will be the name of the section
 * in which -ffunction-sections will put fabs for power, and put the same name 
 * here */

.section .text.fabs, "ax"
/* .text */
.global fabs
.type   fabs,%function
fabs:
	fabs d0, d0
	ret

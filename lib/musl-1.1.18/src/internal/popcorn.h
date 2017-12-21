/*
 * To prevent clobbering environment variables & argv during stack
 * transformation, store the highest stack address dedicated to function
 * activations at application startup.
 */
extern void *__popcorn_stack_base = NULL;


static inline struct pthread *__pthread_self()
{
	struct pthread *self;
	__asm__ __volatile__ ("mov %%fs:0,%0" : "=r" (self) );
	return self;
}

// Popcorn: conform to uniform TLS layout (TLS above thread descriptor)
#define TLS_ABOVE_TP
#define TP_ADJ(p) (p)

#define MC_PC gregs[REG_RIP]

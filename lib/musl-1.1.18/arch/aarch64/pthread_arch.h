static inline struct pthread *__pthread_self()
{
	char *self;
	__asm__ __volatile__ ("mrs %0,tpidr_el0" : "=r"(self));
  // Popcorn: re-arranged aarch64's TLS
  return (void*)self;
	// return (void*)(self + 16 - sizeof(struct pthread));
}

// Popcorn: re-arranged aarch64's TLS
#define TP_ADJ(p) (p)
//#define TLS_ABOVE_TP
//#define TP_ADJ(p) ((char *)(p) + sizeof(struct pthread) - 16)

#define MC_PC pc

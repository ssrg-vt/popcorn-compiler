#ifndef __POPCORN_VARG__
#define __POPCORN_VARG__

#undef va_start
#undef va_arg
#undef va_end
#define va_list char;
#define va_start(a,b) char __index=0;
#define va_arg(a, tp) ((tp)vaarg_tab[++__index])
#define va_end(a) __index=0/**/

#ifndef __scc
#define __scc(X) ((long) (X))
typedef long call_vaarg_arg_t;
#endif

#if 0
#define __call_vaarg0(n) (__call_vaarg)(n)
#define __call_vaarg1(n,a) (__call_vaarg)(n,__scc(a))
#define __call_vaarg2(n,a,b) (__call_vaarg)(n,__scc(a),__scc(b))
#define __call_vaarg3(n,a,b,c) (__call_vaarg)(n,__scc(a),__scc(b),__scc(c))
#define __call_vaarg4(n,a,b,c,d) (__call_vaarg)(n,__scc(a),__scc(b),__scc(c),__scc(d))
#define __call_vaarg5(n,a,b,c,d,e) (__call_vaarg)(n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e))
#define __call_vaarg6(n,a,b,c,d,e,f) (__call_vaarg)(n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e),__scc(f))
#else
#define __call_vaarg1(__func,__nb_arg,n,a) __call_vaarg__1(__func,__nb_arg,n,__scc(a))
#define __call_vaarg2(__func,__nb_arg,n,a,b) __call_vaarg__2(__func,__nb_arg,n,__scc(a),__scc(b))
#define __call_vaarg3(__func,__nb_arg,n,a,b,c) __call_vaarg__3(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c))
#define __call_vaarg4(__func,__nb_arg,n,a,b,c,d) __call_vaarg__4(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d))
#define __call_vaarg5(__func,__nb_arg,n,a,b,c,d,e) __call_vaarg__5(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e))
#define __call_vaarg6(__func,__nb_arg,n,a,b,c,d,e,f) __call_vaarg__6(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e),__scc(f))
#endif
#define __call_vaarg7(__func,__nb_arg,n,a,b,c,d,e,f,g) (__call_vaarg__)(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e),__scc(f),__scc(g))

#define __SYSCALL_NARGS_X(a,b,c,d,e,f,g,h,n,...) n
#define __SYSCALL_NARGS(...) __SYSCALL_NARGS_X(__VA_ARGS__,7,6,5,4,3,2,1,0,)
#define __SYSCALL_CONCAT_X(a,b) a##b
#define __SYSCALL_CONCAT(a,b) __SYSCALL_CONCAT_X(a,b)
#define __SYSCALL_DISP(b,__func,...) __SYSCALL_CONCAT(b,__SYSCALL_NARGS(__VA_ARGS__))(__func,__SYSCALL_NARGS(__VA_ARGS__, dummy),__VA_ARGS__)

#define __call_vaarg(__func, ...) __SYSCALL_DISP(__call_vaarg, __func, __VA_ARGS__)
#define call_vaarg(__func, ...) (__call_vaarg(__func, __VA_ARGS__))

#define __call_vaarg__1(__func, __nb_arg, __def, __a0) 	\
({ 						\
	long __tab[__nb_arg+1];			\
	__tab[0]=__a0;				\
	__tab[1]=0;				\
	__func(__a0, (long*)&__tab);			\
})

#define __call_vaarg__2(__func, __nb_arg, __def,  __a0, __a1) 	\
({ 						\
	long __tab[__nb_arg+1];			\
	__tab[0]=__a0;				\
	__tab[1]=__a1;				\
	__tab[2]=0;				\
	__func(__a0, (long*)&__tab);			\
})

#define __call_vaarg__3(__func, __nb_arg, __def, __a0, __a1, __a2) 	\
({ 						\
	long __tab[__nb_arg+1];			\
	__tab[0]=__a0;				\
	__tab[1]=__a1;				\
	__tab[2]=__a2;				\
	__tab[3]=0;				\
	__func(__a0, (long*)&__tab);			\
})

#define __call_vaarg__4(__func, __nb_arg, __def, __a0, __a1, __a2, __a3) 	\
({ 						\
	long __tab[__nb_arg+1];			\
	__tab[0]=__a0;				\
	__tab[1]=__a1;				\
	__tab[2]=__a2;				\
	__tab[3]=__a3;				\
	__tab[4]=0;				\
	__func(__a0, (long*)&__tab);			\
})

//TODO: use preprocessor loop: https://stackoverflow.com/questions/319328/how-to-write-a-while-loop-with-the-c-preprocessor
#define __call_vaarg__5(__func, __nb_arg, __def, __a0, __a1, __a2, __a3, __a4) 	\
({ 						\
	long __tab[__nb_arg+1];			\
	__tab[0]=__a0;				\
	__tab[1]=__a1;				\
	__tab[2]=__a2;				\
	__tab[3]=__a3;				\
	__tab[4]=__a4;				\
	__tab[5]=0;				\
	__func(__a0, (long*)&__tab);			\
})

#endif

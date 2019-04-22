#ifndef __POPCORN_VARG__
#define __POPCORN_VARG__

typedef long vaarg_arg_t;
typedef vaarg_arg_t vaarg_tab_t;

#undef va_start
#undef va_arg
#undef va_end
#define va_list vaarg_tab_t*
#define va_start(ap,first) char __index=0; (ap=__vaarg_tab)
#define va_arg(a, tp) ((tp)__vaarg_tab[++__index])
#define va_end(a) __index=-1 

#ifndef __scc
#define __scc(X) ((vaarg_arg_t) (X))
#endif

#define __call_vaarg1(__func,__nb_arg,n,a) __call_vaarg__1(__func,__nb_arg,n,__scc(a))
#define __call_vaarg2(__func,__nb_arg,n,a,b) __call_vaarg__2(__func,__nb_arg,n,__scc(a),__scc(b))
#define __call_vaarg3(__func,__nb_arg,n,a,b,c) __call_vaarg__3(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c))
#define __call_vaarg4(__func,__nb_arg,n,a,b,c,d) __call_vaarg__4(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d))
#define __call_vaarg5(__func,__nb_arg,n,a,b,c,d,e) __call_vaarg__5(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e))
#define __call_vaarg6(__func,__nb_arg,n,a,b,c,d,e,f) __call_vaarg__6(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e),__scc(f))
#define __call_vaarg7(__func,__nb_arg,n,a,b,c,d,e,f,g) (__call_vaarg__7)(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e),__scc(f),__scc(g))

#define __VAARG_NARGS_X(a,b,c,d,e,f,g,h,n,...) n
#define __VAARG_NARGS(...) __VAARG_NARGS_X(__VA_ARGS__,7,6,5,4,3,2,1,0,)
#define __VAARG_CONCAT_X(a,b) a##b
#define __VAARG_CONCAT(a,b) __VAARG_CONCAT_X(a,b)
#define __VAARG_DISP(b,__func,...) __VAARG_CONCAT(b,__VAARG_NARGS(__VA_ARGS__))(__func,__VAARG_NARGS(__VA_ARGS__, dummy),__VA_ARGS__)

#define __call_vaarg(__func, ...) __VAARG_DISP(__call_vaarg, __func, __VA_ARGS__)
#define call_vaarg(__func, ...) (__call_vaarg(__func, __VA_ARGS__))



#define __call_vaarg__1(__func, __nb_arg, __def, __a0) 	\
({ 						\
	vaarg_tab_t __vaarg_tab[__nb_arg+1];			\
	__vaarg_tab[0]=__a0;				\
	__vaarg_tab[1]=0;				\
	__func(__a0, (vaarg_tab_t*)&__vaarg_tab);			\
})

#define __call_vaarg__2(__func, __nb_arg, __def,  __a0, __a1) 	\
({ 						\
	vaarg_tab_t __vaarg_tab[__nb_arg+1];			\
	__vaarg_tab[0]=__a0;				\
	__vaarg_tab[1]=__a1;				\
	__vaarg_tab[2]=0;				\
	__func(__a0, (vaarg_tab_t*)&__vaarg_tab);			\
})

#define __call_vaarg__3(__func, __nb_arg, __def, __a0, __a1, __a2) 	\
({ 						\
	vaarg_tab_t __vaarg_tab[__nb_arg+1];			\
	__vaarg_tab[0]=__a0;				\
	__vaarg_tab[1]=__a1;				\
	__vaarg_tab[2]=__a2;				\
	__vaarg_tab[3]=0;				\
	__func(__a0, (vaarg_tab_t*)&__vaarg_tab);			\
})

#define __call_vaarg__4(__func, __nb_arg, __def, __a0, __a1, __a2, __a3) 	\
({ 						\
	vaarg_tab_t __vaarg_tab[__nb_arg+1];			\
	__vaarg_tab[0]=__a0;				\
	__vaarg_tab[1]=__a1;				\
	__vaarg_tab[2]=__a2;				\
	__vaarg_tab[3]=__a3;				\
	__vaarg_tab[4]=0;				\
	__func(__a0, (vaarg_tab_t*)&__vaarg_tab);			\
})

//TODO: use preprocessor loop: https://stackoverflow.com/questions/319328/how-to-write-a-while-loop-with-the-c-preprocessor
#define __call_vaarg__5(__func, __nb_arg, __def, __a0, __a1, __a2, __a3, __a4) 	\
({ 						\
	vaarg_tab_t __vaarg_tab[__nb_arg+1];			\
	__vaarg_tab[0]=__a0;				\
	__vaarg_tab[1]=__a1;				\
	__vaarg_tab[2]=__a2;				\
	__vaarg_tab[3]=__a3;				\
	__vaarg_tab[4]=__a4;				\
	__vaarg_tab[5]=0;				\
	__func(__a0, (vaarg_tab_t*)&__vaarg_tab);			\
})


/* Two default arguments */
#define __call_vaarg_2_1(__func,__nb_arg,n,a) __call_vaarg_2__1(__func,__nb_arg,n,__scc(a))
#define __call_vaarg_2_2(__func,__nb_arg,n,a,b) __call_vaarg_2__2(__func,__nb_arg,n,__scc(a),__scc(b))
#define __call_vaarg_2_3(__func,__nb_arg,n,a,b,c) __call_vaarg_2__3(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c))
#define __call_vaarg_2_4(__func,__nb_arg,n,a,b,c,d) __call_vaarg_2__4(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d))
#define __call_vaarg_2_5(__func,__nb_arg,n,a,b,c,d,e) __call_vaarg_2__5(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e))
#define __call_vaarg_2_6(__func,__nb_arg,n,a,b,c,d,e,f) __call_vaarg_2__6(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e),__scc(f))
#define __call_vaarg_2_7(__func,__nb_arg,n,a,b,c,d,e,f,g) (__call_vaarg_2__7)(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e),__scc(f),__scc(g))

#define __VAARG2_NARGS_X(a,b,c,d,e,f,g,h,n,...) n
#define __VAARG2_NARGS(...) __VAARG2_NARGS_X(__VA_ARGS__,7,6,5,4,3,2,1,0,)
#define __VAARG2_CONCAT_X(a,b) a##b
#define __VAARG2_CONCAT(a,b) __VAARG2_CONCAT_X(a,b)
#define __VAARG2_DISP(b,__func,...) __VAARG2_CONCAT(b,__VAARG2_NARGS(__VA_ARGS__))(__func,__VAARG2_NARGS(__VA_ARGS__, dummy),__VA_ARGS__)

#define __call_vaarg_2(__func, ...) __VAARG2_DISP(__call_vaarg_2_, __func, __VA_ARGS__)
#define call_vaarg_2(__func, ...) (__call_vaarg_2(__func, __VA_ARGS__))


#define __call_vaarg_2__2(__func, __nb_arg, __def,  __a0, __a1) 	\
({ 								\
	vaarg_tab_t __vaarg_tab[__nb_arg];			\
	__vaarg_tab[0]=__a1;					\
	__vaarg_tab[1]=0;					\
	__func(__def, __a0, (vaarg_tab_t*)&__vaarg_tab);		\
})

#define __call_vaarg_2__3(__func, __nb_arg, __def, __a0, __a1, __a2) 	\
({ 									\
	vaarg_tab_t __vaarg_tab[__nb_arg];				\
	__vaarg_tab[0]=__a1;						\
	__vaarg_tab[1]=__a2;						\
	__vaarg_tab[__nb_arg-1]=0;						\
	__func(__def, __a0, (vaarg_tab_t*)&__vaarg_tab);			\
})

#define __call_vaarg_2__4(__func, __nb_arg, __def, __a0, __a1, __a2, __a3) 	\
({ 										\
	vaarg_tab_t __vaarg_tab[__nb_arg];					\
	__vaarg_tab[0]=__a1;						\
	__vaarg_tab[1]=__a2;						\
	__vaarg_tab[2]=__a3;						\
	__vaarg_tab[__nb_arg-1]=0;						\
	__func(__def, __a0, (vaarg_tab_t*)&__vaarg_tab);			\
})

//TODO: use preprocessor loop: https://stackoverflow.com/questions/319328/how-to-write-a-while-loop-with-the-c-preprocessor
#define __call_vaarg_2__5(__func, __nb_arg, __def, __a0, __a1, __a2, __a3, __a4) 	\
({ 						\
	vaarg_tab_t __vaarg_tab[__nb_arg];					\
	__vaarg_tab[0]=__a1;						\
	__vaarg_tab[1]=__a2;						\
	__vaarg_tab[2]=__a3;						\
	__vaarg_tab[3]=__a4;						\
	__vaarg_tab[__nb_arg-1]=0;						\
	__func(__def, __a0, (vaarg_tab_t*)&__vaarg_tab);			\
})




/* Three default arguments */
#define __call_vaarg_3_1(__func,__nb_arg,n,a) __call_vaarg_3__1(__func,__nb_arg,n,__scc(a))
#define __call_vaarg_3_2(__func,__nb_arg,n,a,b) __call_vaarg_3__2(__func,__nb_arg,n,__scc(a),__scc(b))
#define __call_vaarg_3_3(__func,__nb_arg,n,a,b,c) __call_vaarg_3__3(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c))
#define __call_vaarg_3_4(__func,__nb_arg,n,a,b,c,d) __call_vaarg_3__4(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d))
#define __call_vaarg_3_5(__func,__nb_arg,n,a,b,c,d,e) __call_vaarg_3__5(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e))
#define __call_vaarg_3_6(__func,__nb_arg,n,a,b,c,d,e,f) __call_vaarg_3__6(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e),__scc(f))
#define __call_vaarg_3_7(__func,__nb_arg,n,a,b,c,d,e,f,g) (__call_vaarg_3__7)(__func,__nb_arg,n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e),__scc(f),__scc(g))

#define __VAARG3_NARGS_X(a,b,c,d,e,f,g,h,n,...) n
#define __VAARG3_NARGS(...) __VAARG3_NARGS_X(__VA_ARGS__,7,6,5,4,3,2,1,0,)
#define __VAARG3_CONCAT_X(a,b) a##b
#define __VAARG3_CONCAT(a,b) __VAARG3_CONCAT_X(a,b)
#define __VAARG3_DISP(b,__func,...) __VAARG3_CONCAT(b,__VAARG3_NARGS(__VA_ARGS__))(__func,__VAARG3_NARGS(__VA_ARGS__, dummy),__VA_ARGS__)

#define __call_vaarg_3(__func, ...) __VAARG3_DISP(__call_vaarg_3_, __func, __VA_ARGS__)
#define call_vaarg_3(__func, ...) (__call_vaarg_3(__func, __VA_ARGS__))

#define __call_vaarg_3__2(__func, __nb_arg, __def, __a0, __a1, __a2) 	\
({ 									\
	vaarg_tab_t __vaarg_tab[__nb_arg-1];				\
	__vaarg_tab[__nb_arg-2]=0;						\
	__func(__def, __a0, __a1, __a2, (vaarg_tab_t*)&__vaarg_tab);			\
})

#define __call_vaarg_3__3(__func, __nb_arg, __def, __a0, __a1, __a2) 	\
({ 									\
	vaarg_tab_t __vaarg_tab[__nb_arg-1];				\
	__vaarg_tab[0]=__a2;						\
	__vaarg_tab[__nb_arg-2]=0;						\
	__func(__def, __a0, __a1, (vaarg_tab_t*)&__vaarg_tab);			\
})

#define __call_vaarg_3__4(__func, __nb_arg, __def, __a0, __a1, __a2, __a3) 	\
({ 										\
	vaarg_tab_t __vaarg_tab[__nb_arg-1];				\
	__vaarg_tab[0]=__a2;						\
	__vaarg_tab[1]=__a3;						\
	__vaarg_tab[__nb_arg-2]=0;						\
	__func(__def, __a0, __a1, (vaarg_tab_t*)&__vaarg_tab);			\
})

//TODO: use preprocessor loop: https://stackoverflow.com/questions/319328/how-to-write-a-while-loop-with-the-c-preprocessor
#define __call_vaarg_3__5(__func, __nb_arg, __def, __a0, __a1, __a2, __a3, __a4) 	\
({ 						\
	vaarg_tab_t __vaarg_tab[__nb_arg-1];				\
	__vaarg_tab[0]=__a2;						\
	__vaarg_tab[1]=__a3;						\
	__vaarg_tab[2]=__a4;						\
	__vaarg_tab[__nb_arg-2]=0;						\
	__func(__def, __a0, __a1, (vaarg_tab_t*)&__vaarg_tab);			\
})

#endif

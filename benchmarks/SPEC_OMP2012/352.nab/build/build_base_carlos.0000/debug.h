#ifdef NDEBUG
#  define EXPR(t,q) (q)
#else
#  define EXPR(t,q) \
		(fprintf( stdout, "%20s(%4i) %s --> " t "\n", \
			__FILE__,__LINE__,#q,q))
#endif

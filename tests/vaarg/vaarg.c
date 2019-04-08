#include<stdio.h>
#include<stdarg.h>
#include<popcorn-vaarg.h>

//void varfun(int i, ...);
//void varfun2(int i, ...);
void varfun2(int n_args, long* tab);
int main(){
        //varfun(6, 1, 2, 3, 4, 5, 6);
	int first_default_arg=-1;
	call_vaarg(varfun2, first_default_arg, 1, 2982);
	call_vaarg(varfun2, first_default_arg, 1, 2, 3);
	call_vaarg(varfun2, first_default_arg, 1, 2, 3, 4);
	call_vaarg(varfun2, first_default_arg, 1, 2, 3, 4, 5);
        return 0;
}

/*
void varfun(int n_args, ...){
        va_list ap;
        int i, t;
        va_start(ap, n_args);
        for(i=0;t = va_arg(ap, int);i++){
               printf("%d\n", t);
        }
        va_end(ap);
}
*/

void varfun2(int n_args, long* vaarg_tab){
        //va_list ap;
        int i, t;
        printf("%d\n", n_args);
        va_start(ap, n_args);
        for(i=0;t = va_arg(ap, int);i++){
               printf("%d\n", t);
        }
        va_end(ap);
}




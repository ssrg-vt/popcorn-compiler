#include<stdio.h>
#include<stdarg.h>
#include<popcorn-vaarg.h>

//void varfun(int i, ...);
//void varfun(int i, ...);
void varfun(int n_args, long* tab);
void varfun2(int def0, int def1, vaarg_tab_t* __vaarg_tab);
int main(){
        //varfun(6, 1, 2, 3, 4, 5, 6);
	int first_default_arg=-1;
	call_vaarg(varfun, first_default_arg, 1, 2982);
	call_vaarg(varfun, first_default_arg, 1, 2, 3);
	call_vaarg(varfun, first_default_arg, 1, 2, 3, 4);
	call_vaarg(varfun, first_default_arg, 1, 2, 3, 4, 5);

	call_vaarg_2(varfun2, first_default_arg, first_default_arg, 1, 2);
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

void varfun(int def0, vaarg_tab_t* __vaarg_tab){
        va_list ap;
        int i, t;
        printf("%d\n", def0);
        va_start(ap, def0);
        for(i=0;t = va_arg(ap, int);i++){
               printf("%d\n", t);
        }
        va_end(ap);
}


void varfun2(int def0, int def1, vaarg_tab_t* __vaarg_tab){
        va_list ap;
        int i, t;
        printf("%d\n", def1);
        va_start(ap, def1);
        for(i=0;t = va_arg(ap, int);i++){
               printf("%d\n", t);
        }
        va_end(ap);
}





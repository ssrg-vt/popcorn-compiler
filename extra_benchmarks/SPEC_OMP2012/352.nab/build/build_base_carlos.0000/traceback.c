#include <stdio.h>
#include <stdlib.h>

#include "errormsg.h"

int	rt_errormsg( int, char [] );
int	rt_errormsg_s( int, char [], char [] );
int	rt_errormsg_2s( int, char [], char [], char [] );
int	rt_errormsg_d( int, char [], int );

int	rt_errormsg( int fatal, char msg[] )
{

	fprintf( stderr, msg );
	if( fatal )
		exit( 1 );
	return(0);
}

int	rt_errormsg_s( int fatal, char fmt[], char str[] )
{

	fprintf( stderr, fmt, str );
	if( fatal )
		exit( 1 );
	return(0);
}

int	rt_errormsg_2s( int fatal, char fmt[], char str1[], char str2[] )
{

	fprintf( stderr, fmt, str1, str2 );
	if( fatal )
		exit( 1 );
	return(0);
}

int	rt_errormsg_d( int fatal, char fmt[], int i )
{

	fprintf( stderr, fmt, i );
	if( fatal )
		exit( 1 );
	return(0);
}

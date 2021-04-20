#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "memutil.h"

int readbinposhdr( FILE *fp )
{
	char	magic[ 10 ];

	if( fread( magic, 1, 4, fp ) != 4 ){
		fprintf( stderr, "Couldn't read magic number from BINPOS\n" );
		exit( -1 );
	}

	magic[ 4 ] = '\0';
	if( strcmp( magic, "fxyz" ) != 0 ){
		fprintf( stderr, "bad magic number \"%s\"\n", magic );
		exit( -1 );
	}
	return( 0 );
}

int readbinposfrm( int n_atom, REAL_T apos[], FILE *fp )
/*    returns 1 on successful read of a "frame", 0 for eof      */
{
	int	count, i;
	float	*aposf;

	if( fread( &n_atom, sizeof( int ), 1, fp ) != 1 ) {
		return( 0 ); 
	}

#ifdef NAB_DOUBLE_PRECISION
/*    set up dummy space to read in floats, convert to doubles:       */
	aposf = ( float * )malloc( 3*n_atom * sizeof( float ) );
	if( !aposf ){ fprintf( stderr, "malloc error in binpos\n" ); exit(1); }
	if( ( count = fread( aposf, sizeof( float ), 3 * n_atom, fp ) )
		!= 3 * n_atom ){
		fprintf( stderr, "Could only read %d of %d atoms requested\n",
			count / 3, n_atom );
		exit( -1 );
	}
	for( i=0; i<3*n_atom; i++ ){
		apos[i] = aposf[i];
	}
	free( aposf );
#else
	if( ( count = fread( apos, sizeof( float ), 3 * n_atom, fp ) )
		!= 3 * n_atom ){
		fprintf( stderr, "Could only read %d of %d atoms requested\n",
			count / 3, n_atom );
		exit( -1 );
	}
#endif
	return( 1 );
}

int writebinposhdr( FILE *fp )
{
    /* write magic number */
    fwrite( "fxyz", 4, 1, fp );
	return( 0 );
}

int writebinposfrm( int n_atom, REAL_T apos[], FILE *fp )
{
	float *aposf;
	int i;

	fwrite( &n_atom, sizeof( int ), 1, fp ) ;

#ifdef NAB_DOUBLE_PRECISION
/*    set up dummy space to read in floats, convert to doubles:       */
	aposf = ( float * )malloc( 3*n_atom * sizeof( float ) );
	if( !aposf ){ fprintf( stderr, "malloc error in binpos\n" ); exit(1); }
	for( i=0; i<3*n_atom; i++ ){
		aposf[i] = apos[i];
	}
	fwrite( aposf, sizeof( float ), 3 * n_atom, fp );
	free( aposf );
#else
	fwrite( apos, sizeof( float ), 3 * n_atom, fp );
#endif
	fflush( fp );
	return( 0 );
}

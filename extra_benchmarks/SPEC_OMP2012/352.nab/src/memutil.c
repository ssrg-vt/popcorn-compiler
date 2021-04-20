#include <stdio.h>
#include <stdlib.h>

#include "nab.h"

void	nrerror( char msg[] )
/*
char	msg[];
*/
{

	fprintf( stderr, "FATAL: %s\n", msg );
	exit( 1 );
}

REAL_T	*vector( size_t nl, size_t nh )
{
	REAL_T	*v;

#if 0
	fprintf( nabout,"vector: nl = %lld  nh = %lld\n", nl, nh);
	fflush(nabout);
	if (nh < nl) {
	  fprintf( nabout,"argument error in vector: nh < nl\n");
	  fflush(nabout);
	  exit (1);
	}
#endif
	v = ( REAL_T * )malloc( ( nh - nl + 1 ) * sizeof( REAL_T ) );
	if( !v ) {
	        fprintf( nabout,"allocation failure in vector: nh = %lld\n", nh);
		fflush(nabout);
		nrerror( "allocation failure in vector()" );
	}
#if 0
	fprintf( nabout,"returning from vector\n\n");
	fflush(nabout);
#endif
	return( v - nl );
}

int	*ivector( int nl, int nh )
/*
int	nl, nh;
*/
{
	int	*v;

	v = ( int * )malloc( ( nh - nl + 1 ) * sizeof( int ) );
	if( !v )
		nrerror( "allocation failure in ivector()" );
	return( v - nl );
}

int	*ipvector( int nl, int nh )
/*
int	nl, nh;
*/
{
	int	*v;

	v = ( int * )malloc( ( nh - nl + 1 ) * sizeof( int * ) );
	if( !v )
		nrerror( "allocation failure in ipvector()" );
	return( v - nl );
}

#define	NR_END	1
REAL_T	**matrix( int nrl, int nrh, int ncl, int nch )
/*
int	nrl, nrh, ncl, nch;
*/
{
	int	i;
	int	nrow, ncol;
	REAL_T	**m;

	nrow = nrh - nrl + 1;
	ncol = nch - ncl + 1;

	m = ( REAL_T ** )malloc( ( nrow + NR_END ) * sizeof( REAL_T * ) );
	if( !m )
		nrerror( "allocation failure 1 in matrix()" );
	m += NR_END;
	m -= nrl;

	m[ nrl ] = ( REAL_T * )
		malloc( ( nrow * ncol + NR_END ) * sizeof( REAL_T ) );
	if( !m[ nrl ] )
		nrerror( "allocation failure 2 in matrix()" );
	m[ nrl ] += NR_END;
	m[ nrl ] -= ncl;

	for( i = nrl + 1; i <= nrh; i++ )
		m[ i ] = m[ i - 1 ] + ncol;
	return( m );
}

int	**imatrix( int nrl, int nrh, int ncl, int nch )
/*
int	nrl, nrh, ncl, nch;
*/
{
	int	i;
	int	nrow, ncol;
	int	**m;

	nrow = nrh - nrl + 1;
	ncol = nch - ncl + 1;

	m = ( int ** )malloc( ( nrow + NR_END ) * sizeof( int * ) );
	if( !m )
		nrerror( "allocation failure 1 in matrix()" );
	m += NR_END;
	m -= nrl;

	m[ nrl ] = ( int * )
		malloc( ( nrow * ncol + NR_END ) * sizeof( int ) );
	if( !m[ nrl ] )
		nrerror( "allocation failure 2 in matrix()" );
	m[ nrl ] += NR_END;
	m[ nrl ] -= ncl;

	for( i = nrl + 1; i <= nrh; i++ )
		m[ i ] = m[ i - 1 ] + ncol;
	return( m );
}

void	free_vector( REAL_T *v, size_t nl, size_t nh )
{

	free( ( char * )( v + nl ) );
}

void	free_ivector( int *v, int nl, int nh )
/*
int	*v;
int	nl, nh;
*/
{

	free( ( char * )( v + nl ) );
}

void	free_matrix( REAL_T **m, int nrl, int nrh, int ncl, int nch )
/*
REAL_T	**m;
int	nrl, nrh, ncl, nch;
*/
{
	free( ( char * )( m[ nrl ] + ncl - NR_END ) );
	free( ( char * )( m + nrl - NR_END ) );
}

void	free_imatrix( int **m, int nrl, int nrh, int ncl, int nch )
/*
int	**m;
int	nrl, nrh, ncl, nch;
*/
{
	free( ( char * )( m[ nrl ] + ncl - NR_END ) );
	free( ( char * )( m + nrl - NR_END ) );
}

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "nab.h"

#define	LAST	(-1)

#define	AEXPR_SIZE	1000
static	char	aexpr[ AEXPR_SIZE ];
static	char	*spart;
static	char	*rpart;
static	char	*apart;

#define	REXPR_SIZE	1000
static	char	rexpr[ REXPR_SIZE ];

#define	EXPBUF_SIZE	1000
static	char	expbuf[ EXPBUF_SIZE ];

int	select_atoms( MOLECULE_T *, char [] );
int	atom_in_aexpr( ATOM_T *, char [] );
void	set_attr_if( MOLECULE_T *, int, int );
void	clear_attr( MOLECULE_T *, int );

static	int	eval_1_aexpr( MOLECULE_T *, char [] );
static	int	atom_in_1_aexpr( ATOM_T *, char [] );
static	int	is_pattern( char [], int *, int * );
static	void	select_all( MOLECULE_T * );
static	void	clear_select( MOLECULE_T * );
static	void	clear_work( MOLECULE_T * );
static	void	or_select( MOLECULE_T * );
static	void	set_select( MOLECULE_T * );
static	void	match_str_pat( MOLECULE_T *, char [] );
static	int	atom_in_str_pat( ATOM_T *, char [] );
static	void	match_str_range( MOLECULE_T *, int, int );
static	int	atom_in_str_range( ATOM_T *, int, int );
static	void	match_res_pat( MOLECULE_T *, char [] );
static	int	atom_in_res_pat( ATOM_T *, char [] );
static	void	match_res_range( MOLECULE_T *, int, int );
static	int	atom_in_res_range( ATOM_T *, int, int );
static	void	match_atom_pat( MOLECULE_T *, char [] );
static	int	atom_in_atom_pat( ATOM_T *, char [] );
static	void	aexpr2rexpr( char [], char [] );

int	setpoint( MOLECULE_T *mol, char aexpr[], POINT_T point )
{
	int	r, a, ta;
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;
	REAL_T	x, y, z;

	select_atoms( mol, aexpr );

	/* compute the point as the CM of the selected atoms	*/
	x = y = z = 0.0;
	for( ta = 0, sp = mol->m_strands; sp; sp = sp->s_next ){
		if( AT_SELECT & sp->s_attr ){
			for( r = 0; r < sp->s_nresidues; r++ ){
				res = sp->s_residues[ r ];
				if( AT_SELECT & res->r_attr ){
					for( a = 0; a < res->r_natoms; a++ ){
						ap = &res->r_atoms[ a ];
						if( AT_SELECT & ap->a_attr ){
							x += ap->a_pos[ 0 ];
							y += ap->a_pos[ 1 ];
							z += ap->a_pos[ 2 ];
							ta++;
						}
					}
				}
			}
		}
	}

	if( ta == 0 ){
		fprintf( stderr, "setpoint: %s: no atoms selected\n", aexpr );
		return( 1 );
	}else{
		point[ 0 ] = x / ta;
		point[ 1 ] = y / ta;
		point[ 2 ] = z / ta;
	}

	return( 0 );
}

int	select_atoms( MOLECULE_T *mol, char aex[] )
{
	char	*aep, *n_aep;
	int	ael;

	if( aex == NULL ){
		select_all( mol );
		return( 0 );
	}

	clear_work( mol );
	clear_select( mol );

	for( aep = aex, n_aep = strchr( aep, '|' ); aep; ){
		if( n_aep ){
			ael = n_aep - aep;
			n_aep++;
		}else
			ael = strlen( aep );
		if( ael >= AEXPR_SIZE ){
			fprintf( stderr,
				"select_atoms: atom-expr too complicated\n" );
			return( 1 );
		}
		strncpy( aexpr, aep, ael );
		aexpr[ ael ] = '\0';
		eval_1_aexpr( mol, aexpr );
		or_select( mol );
		aep = n_aep;
		if( aep )
			n_aep = strchr( aep, '|' );
		clear_select( mol );
	}
	set_select( mol );

	return( 0 );
}

int	atom_in_aexpr( ATOM_T *ap, char aex[] )
{
	char	*aep, *n_aep;
	int	ael;

	if( aex == NULL )
		return( 0 );

	for( aep = aex, n_aep = strchr( aep, '|' ); aep; ){
		if( n_aep ){
			ael = n_aep - aep;
			n_aep++;
		}else
			ael = strlen( aep );
		if( ael >= AEXPR_SIZE ){
			fprintf( stderr,
				"atom_in_aexpr: atom-expr too complicated\n" );
			return( 0 );
		}
		strncpy( aexpr, aep, ael );
		aexpr[ ael ] = '\0';
		if( atom_in_1_aexpr( ap, aexpr ) )
			return( 1 );
		aep = n_aep;
		if( aep )
			n_aep = strchr( aep, '|' );
	}
	return( 0 );
}

void	set_attr_if( MOLECULE_T *mol, int attr, int i_attr )
{
	int		a, r;
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;

	for( sp = mol->m_strands; sp; sp = sp->s_next ){
		sp->s_attr |= ( sp->s_attr & i_attr ) ? attr : 0;
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			res->r_attr |= ( res->r_attr & i_attr ) ?
				attr : 0;
			for( a = 0; a < res->r_natoms; a++ ){
				ap = &res->r_atoms[ a ];
				ap->a_attr |= ( ap->a_attr & i_attr ) ?
					attr : 0;
			}
		}
	}
}

void	clear_attr( MOLECULE_T *mol, int attr )
{
	int		a, r;
	STRAND_T	*sp;
	RESIDUE_T	*res;

	for( sp = mol->m_strands; sp; sp = sp->s_next ){
		sp->s_attr &= ~attr;
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			res->r_attr &= ~attr;
			for( a = 0; a < res->r_natoms; a++ )
				res->r_atoms[ a ].a_attr &= ~attr;
		}
	}
}

static	int	eval_1_aexpr( MOLECULE_T *mol, char aex[] )
{
	char	*aep;
	char	*wp;
	int	lo, hi;
	
	aep = aex;
	if( *aep == ':' ){
		spart = NULL;
		aep++;
	}else{
		spart = strtok( aep, ":" );
		aep += strlen( spart ) + 1;
	}
	if( *aep == ':' ){
		rpart = NULL;
		aep++;
	}else{
		rpart = strtok( aep, ":" );
		aep += strlen( rpart ) + 1;
	}
	apart = strtok( aep, ":" );

	if( spart ){
		wp = strtok( spart, "," );
		if( is_pattern( wp, &lo, &hi ) )
			match_str_pat( mol, wp ); 
		else
			match_str_range( mol, lo, hi );
		while( (wp = strtok( NULL, "," )) ){
			if( is_pattern( wp, &lo, &hi ) )
				match_str_pat( mol, wp ); 
			else
				match_str_range( mol, lo, hi );
		}
	}else
		match_str_range( mol, 1, LAST );

	if( rpart ){
		wp = strtok( rpart, "," );
		if( is_pattern( wp, &lo, &hi ) )
			match_res_pat( mol, wp );
		else
			match_res_range( mol, lo, hi );
		while( (wp = strtok( NULL, "," )) ){
			if( is_pattern( wp, &lo, &hi ) )
				match_res_pat( mol, wp );
			else
				match_res_range( mol, lo, hi );
		}
	}else
		match_res_range( mol, 1, LAST );

	if( apart ){
		wp = strtok( apart, "," );
		if( is_pattern( wp, &lo, &hi ) )
			match_atom_pat( mol, wp );
		else{
			fprintf( stderr, "atom range not allowed\n" );
		}
		while( (wp = strtok( NULL, "," )) ){
			if( is_pattern( wp, &lo, &hi ) )
				match_atom_pat( mol, wp );
			else{
				fprintf( stderr, "atom range not allowed\n" );
			}
		}
	}else
		match_atom_pat( mol, "*" );
	return(0);
}

static	int	atom_in_1_aexpr( ATOM_T *ap, char aex[] )
{
	char	*aep;
	char	*wp;
	int	lo, hi;
	
	aep = aex;
	if( *aep == ':' ){
		spart = NULL;
		aep++;
	}else{
		spart = strtok( aep, ":" );
		aep += strlen( spart ) + 1;
	}
	if( *aep == ':' ){
		rpart = NULL;
		aep++;
	}else{
		rpart = strtok( aep, ":" );
		aep += strlen( rpart ) + 1;
	}
	apart = strtok( aep, ":" );

	if( spart ){
		wp = strtok( spart, "," );
		if( is_pattern( wp, &lo, &hi ) ){
			if( atom_in_str_pat( ap, wp ) )
				goto RPART;
		}else if( atom_in_str_range( ap, lo, hi ) )
			goto RPART;
		while( (wp = strtok( NULL, "," )) ){
			if( is_pattern( wp, &lo, &hi ) ){
				if( atom_in_str_pat( ap, wp ) )
					goto RPART;
			}else if( atom_in_str_range( ap, lo, hi ) )
				goto RPART;
		}
		return( 0 );
	}

RPART : if( rpart ){
		wp = strtok( rpart, "," );
		if( is_pattern( wp, &lo, &hi ) ){
			if( atom_in_res_pat( ap, wp ) )
				goto APART;
		}else if( atom_in_res_range( ap, lo, hi ) )
			goto APART;
		while( (wp = strtok( NULL, "," )) ){
			if( is_pattern( wp, &lo, &hi ) ){
				if( atom_in_res_pat( ap, wp ) )
					goto APART;
			}else if( atom_in_res_range( ap, lo, hi ) )
				goto APART;
		}
		return( 0 );
	}

APART :	if( apart ){
		wp = strtok( apart, "," );
		if( is_pattern( wp, &lo, &hi ) ){
			if( atom_in_atom_pat( ap, wp ) )
				return( 1 );
		}else{
			fprintf( stderr, "atom range not allowed\n" );
			return( 0 );
		}
		while( (wp = strtok( NULL, "," )) ){
			if( is_pattern( wp, &lo, &hi ) ){
				if( atom_in_atom_pat( ap, wp ) )
					return( 1 );
			}else{
				fprintf( stderr, "atom range not allowed\n" );
				return( 0 );
			}
		}
	}else
		return( 1 );

	return(0);
}

static	int	is_pattern( char item[], int *lo, int *hi )
{
	int	val;
	char	*ip;

	if( !isdigit( *item ) && *item != '-' )
		return( TRUE );
	if( isdigit( *item ) ){
		for( val = 0, ip = item; isdigit( *ip ); ip++ )
			val = 10 * val + *ip - '0';
		*lo = val;
		if( !*ip ){
			*hi = *lo;
			return( FALSE );
		}else if( *ip == '-' )
			ip++;
		if( !*ip ){
			*hi = LAST;
			return( FALSE );
		}else if( !isdigit( *ip ) )
			return( TRUE );
		for( val = 0; isdigit( *ip ); ip++ )
			val = 10 * val + *ip - '0'; 
		*hi = val;
		return( *ip );
	}else{
		*lo = 1;
		ip = &item[ 1 ];
	}
	if( !*ip ){
		*hi = LAST;
		return( FALSE );
	}else if( isdigit( *ip ) ){
		for( val = 0; isdigit( *ip ); ip++ )
			val = 10 * val + *ip - '0';
		*hi = val;
		return( *ip );
	}
	return(0);
}

static	void	select_all( MOLECULE_T *mol )
{
	int		a, r;
	STRAND_T	*sp;
	RESIDUE_T	*res;

	for( sp = mol->m_strands; sp; sp = sp->s_next ){
		sp->s_attr |= AT_SELECT;
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			res->r_attr |= AT_SELECT;
			for( a = 0; a < res->r_natoms; a++ )
				res->r_atoms[ a ].a_attr |= AT_SELECT;
		}
	}
}

static	void	clear_select( MOLECULE_T *mol )
{
	int		a, r;
	STRAND_T	*sp;
	RESIDUE_T	*res;

	for( sp = mol->m_strands; sp; sp = sp->s_next ){
		sp->s_attr &= ~AT_SELECT;
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			res->r_attr &= ~AT_SELECT;
			for( a = 0; a < res->r_natoms; a++ )
				res->r_atoms[ a ].a_attr &= ~AT_SELECT;
		}
	}
}

static	void	clear_work( MOLECULE_T *mol )
{
	int		a, r;
	STRAND_T	*sp;
	RESIDUE_T	*res;

	for( sp = mol->m_strands; sp; sp = sp->s_next ){
		sp->s_attr &= ~AT_WORK;
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			res->r_attr &= ~AT_WORK;
			for( a = 0; a < res->r_natoms; a++ )
				res->r_atoms[ a ].a_attr &= ~AT_WORK;
		}
	}
}

static	void	or_select( MOLECULE_T *mol )
{
	int		a, r;
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;

	for( sp = mol->m_strands; sp; sp = sp->s_next ){
		sp->s_attr |= ( sp->s_attr & AT_SELECT ) ? AT_WORK : 0;
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			res->r_attr |= ( res->r_attr & AT_SELECT ) ?
				AT_WORK : 0;
			for( a = 0; a < res->r_natoms; a++ ){
				ap = &res->r_atoms[ a ];
				ap->a_attr |= ( ap->a_attr & AT_SELECT ) ?
					AT_WORK : 0;
			}
		}
	}
}

static	void	set_select( MOLECULE_T *mol )
{
	int		a, r;
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;

	for( sp = mol->m_strands; sp; sp = sp->s_next ){
		sp->s_attr |= ( sp->s_attr & AT_WORK ) ? AT_SELECT : 0;
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			res->r_attr |= ( res->r_attr & AT_WORK ) ?
				AT_SELECT : 0;
			for( a = 0; a < res->r_natoms; a++ ){
				ap = &res->r_atoms[ a ];
				ap->a_attr |= ( ap->a_attr & AT_WORK ) ?
					AT_SELECT : 0;
			}
		}
	}
}

static	void	match_str_pat( MOLECULE_T *mol, char pat[] )
{
	STRAND_T	*sp;

	aexpr2rexpr( pat, rexpr );
	compile( rexpr, expbuf, &expbuf[ EXPBUF_SIZE ], '\0' );
	for( sp = mol->m_strands; sp; sp = sp->s_next ){
		sp->s_attr |= step( sp->s_strandname, expbuf ) ? AT_SELECT : 0;
	}
}

static	int	atom_in_str_pat( ATOM_T *ap, char pat[] )
{
	RESIDUE_T	*res;
	STRAND_T	*sp;

	res = ap->a_residue;
	sp = res->r_strand;
	aexpr2rexpr( pat, rexpr );
	compile( rexpr, expbuf, &expbuf[ EXPBUF_SIZE ], '\0' );
	return(	step( sp->s_strandname, expbuf ) );
}

static	void	match_str_range( MOLECULE_T *mol, int lo, int hi )
{
	int		m;
	STRAND_T	*sp;

	if( hi == UNDEF )
		hi = mol->m_nstrands;
	for( m = 1, sp = mol->m_strands; m <= mol->m_nstrands;
		m++, sp = sp->s_next ){
		if( lo <= m && m <= hi )
			sp->s_attr |= AT_SELECT;
	}
}

static	int	atom_in_str_range( ATOM_T *ap, int lo, int hi )
{
	int		m;
	RESIDUE_T	*res;
	STRAND_T	*sp, *sp1;
	MOLECULE_T	*mol;

	res = ap->a_residue;
	sp = res->r_strand;
	mol = sp->s_molecule;
	if( hi == UNDEF )
		hi = mol->m_nstrands;
	for( m = 1, sp1 = mol->m_strands; m <= mol->m_nstrands;
		m++, sp1 = sp1->s_next ){
		if( sp == sp1 ){
			if( lo <= m && m <= hi )
				return( 1 );
		}
	}
	return( 0 );
}

static	void	match_res_pat( MOLECULE_T *mol, char pat[] )
{
	int		r;
	STRAND_T	*sp;
	RESIDUE_T	*res;

	aexpr2rexpr( pat, rexpr );
	compile( rexpr, expbuf, &expbuf[ EXPBUF_SIZE ], '\0' );
	for( sp = mol->m_strands; sp; sp = sp->s_next ){
		if( AT_SELECT & sp->s_attr ){
			for( r = 0; r < sp->s_nresidues; r++ ){
				res = sp->s_residues[ r ];
				res->r_attr |= step( res->r_resname, expbuf ) ?
					AT_SELECT : 0;
			}
		}
	}
}

static	int	atom_in_res_pat( ATOM_T *ap, char pat[] )
{
	RESIDUE_T	*res;

	res = ap->a_residue;
	aexpr2rexpr( pat, rexpr );
	compile( rexpr, expbuf, &expbuf[ EXPBUF_SIZE ], '\0' );
	return( step( res->r_resname, expbuf ) );
}

static	void	match_res_range( MOLECULE_T *mol, int lo, int hi )
{
	int		r, rhi;
	STRAND_T	*sp;
	RESIDUE_T	*res;

	for( sp = mol->m_strands; sp; sp = sp->s_next ){
		if( AT_SELECT & sp->s_attr ){
			rhi = ( hi == UNDEF ) ? sp->s_nresidues : hi;
			for( r = 0; r < sp->s_nresidues; r++ ){
				res = sp->s_residues[ r ];
				if( lo <= r + 1 && r + 1 <= rhi )
					res->r_attr |= AT_SELECT;
			}
		}
	}
}

static	int	atom_in_res_range( ATOM_T *ap, int lo, int hi )
{
	int		r, rhi;
	STRAND_T	*sp;
	RESIDUE_T	*res, *res1;

	res = ap->a_residue;
	sp = res->r_strand;
	
	rhi = ( hi == UNDEF ) ? sp->s_nresidues : hi;
	for( r = 0; r < sp->s_nresidues; r++ ){
		res1 = sp->s_residues[ r ];
		if( res == res1 ){
			if( lo <= r + 1 && r + 1 <= rhi )
				return( 1 );
		}
	}
	return( 0 );
}

static	void	match_atom_pat( MOLECULE_T *mol, char pat[] )
{
	int		r, a;
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;

	aexpr2rexpr( pat, rexpr );
	compile( rexpr, expbuf, &expbuf[ EXPBUF_SIZE ], '\0' );
	for( sp = mol->m_strands; sp; sp = sp->s_next ){
		if( AT_SELECT & sp->s_attr ){
			for( r = 0; r < sp->s_nresidues; r++ ){
				res = sp->s_residues[ r ];
				if( AT_SELECT & res->r_attr ){
					for( a = 0; a < res->r_natoms; a++ ){
						ap = &res->r_atoms[ a ];
						ap->a_attr |= 
						    step(ap->a_atomname,expbuf)?
						    AT_SELECT : 0;
					}
				}
			}
		}
	}
}

static	int	atom_in_atom_pat( ATOM_T *ap, char pat[] )
{

	aexpr2rexpr( pat, rexpr );
	compile( rexpr, expbuf, &expbuf[ EXPBUF_SIZE ], '\0' );
	return( step( ap->a_atomname, expbuf ) );
}

static	void	aexpr2rexpr( char aexpr[], char rexpr[] )
{
	char	*aep, *rep;

	rep = rexpr;
	*rep++ = '^';
	for( aep = aexpr; *aep; aep++ ){
		if( *aep == '*' ){
			*rep++ = '.';
			*rep++ = '*';
		}else if( *aep == '?' )
			*rep++ = '.';
		else
			*rep++ = *aep;
	}
	*rep++ = '$';
	*rep = '\0';
}

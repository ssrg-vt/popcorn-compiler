#include	<stdio.h>
#include	<ctype.h>
#include	<string.h>
#include	<stdlib.h>
#include	<math.h>

#include	"nab.h"
#include	"errormsg.h"
#include	"memutil.h"
#include    "debug.h"

#define	HBE_DIST	0.96
#define	HBE_ANGLE	108.5
#define	HBE_DIHEDRAL	0.0

#define	D2R	 0.01745329251994329576
#define	R2D	57.29577951308232090712
#ifndef PI
#define PI   3.14159265358979323844
#endif

#define	DIAG(x)	( x + c * ( 1.0 - x ) )
#define	NDIAG(x, y, z )	( (x) + (y) + (z) )

static	char	e_msg[ 256 ];

MOLECULE_T	*newmolecule( void );
int     rt_errormsg_s( int, char [], char [] );
int	select_atoms( MOLECULE_T *, char [] );
int	setpoint( MOLECULE_T *, char [], POINT_T );
int	freeresidue( RESIDUE_T * );
int	freemolecule( MOLECULE_T * );
int	addstrand( MOLECULE_T *, char [] );
int	addresidue( MOLECULE_T *, char [], RESIDUE_T * );
int	connectres( MOLECULE_T *, char [], int, char [], int, char [] );
int	mergestr( MOLECULE_T *, char [], char [], MOLECULE_T *, char [], char [] );
int	countmolstrands( MOLECULE_T *, char * );
int	countstrandresidues( MOLECULE_T *, int );
int	countmolres( MOLECULE_T *, char * );
int	countmolatoms( MOLECULE_T *, char * );
REAL_T	dist( MOLECULE_T *, char [], char [] );
REAL_T	distp( POINT_T, POINT_T );
REAL_T	angle( MOLECULE_T *, char [], char [], char [] );
REAL_T	anglep( POINT_T, POINT_T, POINT_T );
REAL_T	torsion( MOLECULE_T *, char [], char [], char [], char [] );
REAL_T	torsionp( POINT_T, POINT_T, POINT_T, POINT_T );
char	*getresname( RESIDUE_T * );
int	cap( MOLECULE_T *, char *, int , int );
EXTBOND_T	*copyextbonds( RESIDUE_T *res );
int	setreskind( MOLECULE_T *, char [], char [] );
int	setxyz_from_mol( MOLECULE_T **, char **, POINT_T [] );
int	setxyzw_from_mol( MOLECULE_T **, char **, REAL_T [] );
int	setmol_from_xyz( MOLECULE_T **, char **, REAL_T [] );
int	setmol_from_xyzw( MOLECULE_T **, char **, REAL_T [] );

void	NAB_initres( RESIDUE_T *, int );
void	NAB_initatom( ATOM_T *, int );

ATOM_T	*NAB_mnext( MOLECULE_T *, ATOM_T * );
RESIDUE_T	*NAB_rnext( MOLECULE_T *, RESIDUE_T * );
ATOM_T		*NAB_anext( RESIDUE_T *, ATOM_T * );
int	*NAB_mri( MOLECULE_T *, char [] );
int	*NAB_rri( RESIDUE_T *, char [] );
char	**NAB_rrc( RESIDUE_T *, char [] );
int	*NAB_ari( ATOM_T *, char [] );
REAL_T	*NAB_arf( ATOM_T *, char [] );
char	**NAB_arc( ATOM_T *, char [] );
POINT_T	*NAB_arp( ATOM_T *, char [] );
void	upd_molnumbers( MOLECULE_T * );
MATRIX_T	*newtransform( REAL_T, REAL_T, REAL_T, REAL_T, REAL_T, REAL_T );
RESIDUE_T	*transformres( MATRIX_T, RESIDUE_T *, char * );
int	transformmol( MATRIX_T, MOLECULE_T *, char * );
int	transformpts( MATRIX_T, POINT_T [], int );
MATRIX_T	*updtransform( MATRIX_T, MATRIX_T );
MATRIX_T	*trans4p( POINT_T, POINT_T, REAL_T );
MATRIX_T	*trans4( MOLECULE_T *, char [], char [], REAL_T );
MATRIX_T	*rot4p( POINT_T, POINT_T, REAL_T );
MATRIX_T	*rot4( MOLECULE_T *, char [], char [], REAL_T );
MATRIX_T	*NAB_matcpy( void *, void * );

static	void	raa2mat( REAL_T, REAL_T, REAL_T, REAL_T, MATRIX_T );
static	void	mk_idmat( MATRIX_T );
static	void	concat_mat( MATRIX_T, MATRIX_T, MATRIX_T );
static	void	copy_mat( MATRIX_T, MATRIX_T );
static	void	xfm_xyz( POINT_T, MATRIX_T, POINT_T );
static	void	fixextbonds( RESIDUE_T *, int );
static	void	freestrand( MOLECULE_T *, char [] );
static	void	cvt_p2hb( RESIDUE_T * );
static	void	add_he2o3( RESIDUE_T * );
static	int	find_atom( RESIDUE_T *, char [] );
static	int	delete_atom( RESIDUE_T *, int );
static	int	add_atom( RESIDUE_T *, char [] );

/* static	int	ae2xyz( MOLECULE_T *, char [], REAL_T [] )/ */

RESIDUE_T	*copyresidue( RESIDUE_T *);

/***********************************************************************
 							GET()
************************************************************************/

static char * get(size)
size_t	size;
{
	char	*ptr;

	if (size ==0)
		return((char *) NULL);

	if ((ptr = (char *) malloc(size * sizeof(char))) == NULL) {
		fprintf( nabout,"malloc %ld\n", size);
		fflush(nabout);
		perror("malloc err:");
		exit(1);
	}
	return(ptr);
}

MOLECULE_T	*newmolecule( void )
{
	MOLECULE_T	*mp;

	if(( mp = (MOLECULE_T *)malloc(sizeof(MOLECULE_T))) == NULL ){
		rt_errormsg_s( TRUE, E_NOMEM_FOR_S, "new molecule" );
		return( NULL );
	}

	/* the frame of the molecule:	*/
	/* origin:			*/
	mp->m_frame[ 0 ][ 0 ] = 0.0;
	mp->m_frame[ 0 ][ 1 ] = 0.0;
	mp->m_frame[ 0 ][ 2 ] = 0.0;
	/* x-axis:			*/
	mp->m_frame[ 1 ][ 0 ] = 1.0;
	mp->m_frame[ 1 ][ 1 ] = 0.0;
	mp->m_frame[ 1 ][ 2 ] = 0.0;
	/* y-axis:			*/
	mp->m_frame[ 2 ][ 0 ] = 0.0;
	mp->m_frame[ 2 ][ 1 ] = 1.0;
	mp->m_frame[ 2 ][ 2 ] = 0.0;
	/* z-axis:			*/
	mp->m_frame[ 3 ][ 0 ] = 0.0;
	mp->m_frame[ 3 ][ 1 ] = 0.0;
	mp->m_frame[ 3 ][ 2 ] = 1.0;

	mp->m_nstrands = 0;
	mp->m_strands = NULL;

	/* !!! major kludge to hold amber residue order! */
	mp->m_nresidues = 0;

	mp->m_nvalid = 0; /* atom & residue number are valid */

	mp->m_prm = NULL; /* NO forcefield info	*/

	return( mp );
}

int	freeresidue( RESIDUE_T *res )
{
	int		a;
	ATOM_T		*ap;
	EXTBOND_T	*ep, *epn;

	if ( !res )
		return ( 0 );

	if( res->r_resname )
		free( ( void * ) res->r_resname );
	if( res->r_resid )
		free( ( void * ) res->r_resid );
	for( ep = res->r_extbonds; ep; ep = epn ){
		epn = ep->eb_next;
		free( ( void * ) ep );
	}
	if( res->r_intbonds )
		free( ( void * ) res->r_intbonds );
	if( res->r_chiral )
		free( ( void * ) res->r_chiral );
	if( res->r_aindex )
		free( ( void * ) res->r_aindex );
	for( ap=res->r_atoms, a=0; a<res->r_natoms; a++, ap++ ){
		if( ap->a_atomname )
			free( ( void * )ap->a_atomname );
		if( ap->a_atomtype )
			free( ( void * )ap->a_atomtype );
		if( ap->a_element )
			free( ( void * )ap->a_element );
		if( ap->a_fullname )
			free( ( void * ) ap->a_fullname );
	}
	if( res->r_atoms )
		free( ( void * ) res->r_atoms );

	free( ( void * ) res );

	return ( 0 );
}

int	freemolecule( MOLECULE_T *mol )
{
	STRAND_T	*sp, *spn;
	PARMSTRUCT_T	*pp;
	int		r;

	if( !mol )
		return( 0 );

	for( sp = mol->m_strands; sp; sp = spn ){
		spn = sp->s_next;
		if( sp->s_strandname )
			free( ( void * ) sp->s_strandname );
		for( r = 0; r < sp->s_nresidues; r++ )
			freeresidue( sp->s_residues[ r ] );
		if( sp->s_residues )  
			free( ( void * ) sp->s_residues );  
	}
	if( mol->m_prm ){
		pp = mol->m_prm;
		if( pp->AtomNames )
			free( ( void * ) pp->AtomNames );
		if( pp->ResNames )
			free( ( void * ) pp->ResNames );
		if( pp->AtomSym )
			free( ( void * ) pp->AtomSym );
		if( pp->AtomTree )
			free( ( void * ) pp->AtomTree );
		free( ( void * ) mol->m_prm );
	}	
	
	free( ( void * ) mol );      
	return( 0 );
}

int	addstrand( MOLECULE_T *mp, char sname[] )
{
	STRAND_T	*sp, *spl;
	int	nsize;
	char	*np;

	for( sp = mp->m_strands; sp; sp = sp->s_next ){
		if( !strcmp( sp->s_strandname, sname ) ){
			fprintf( stderr,
				"addstrand: strand %s already in mol\n",
				sname );
			return( 1 );
		}
	}

	if((sp=(STRAND_T *)malloc(sizeof(STRAND_T))) == NULL ){
		sprintf( e_msg, "new strand %s", sname );
		rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
		return( 1 );
	}
	nsize = strlen( sname ) + 1;
	if((np=(char *)malloc(nsize*sizeof(char)) ) == NULL ){
		sprintf( e_msg, "name for new strand %s", sname );
		rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
		return( 1 );
	}else
		sp->s_strandname = np;

	strcpy( sp->s_strandname, sname );
	sp->s_strandnum = 0;
	sp->s_attr = 0;
	sp->s_molecule = mp;
	sp->s_next = NULL;
	sp->s_nresidues = 0;
	sp->s_res_size = 0;
	sp->s_residues = NULL;

	if( mp->m_nstrands == 0 )
		mp->m_strands = sp;
	else{
		spl = mp->m_strands;
		for( ; spl->s_next; spl = spl->s_next )
			;
		spl->s_next = sp;
	}	
	mp->m_nstrands++;
/*	not needed; needs to be done only if attr ref'd
	upd_molnumbers( mp );
*/
	mp->m_nvalid = 0;
	return( 0 );
}

int	addresidue( MOLECULE_T *mp, char sname[], RESIDUE_T *res )
{
	STRAND_T	*spl, *sp;
	RESIDUE_T	**rap, *nres;
	int	r, rsize;

	for( sp = NULL, spl = mp->m_strands; spl; spl = spl->s_next ){
		if( strcmp( spl->s_strandname, sname ) == 0 ){
			sp = spl;
			break;
		}
	}

	if( sp == NULL ){
		rt_errormsg_s( TRUE, E_NOSUCH_STRAND_S, sname );
		return( 1 );
	}

	if( sp->s_nresidues == sp->s_res_size ){
		rsize = sp->s_res_size + 10;
		if( ( rap = ( RESIDUE_T ** )malloc(rsize*sizeof(RESIDUE_T *)))
			== NULL )
		{
			sprintf( e_msg, "residue array in strand %s\n",
				sp->s_strandname );
			rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
			return( 0 );
		}
		for( r = 0; r < sp->s_nresidues; r++ )
			rap[ r ] = sp->s_residues[ r ];
		if ( sp->s_res_size > 0 )
			free( sp->s_residues );	
		sp->s_res_size = rsize;
		sp->s_residues = rap;
	}

	sp->s_residues[ sp->s_nresidues ] = nres = copyresidue( res );
	nres->r_strand = sp;
	sp->s_nresidues++;
/*	don't need, molnumbers are lazy eval
	upd_molnumbers( mp );
*/
	mp->m_nvalid = 0;
	return( 0 );
}

int	connectres( MOLECULE_T *mol, char sname[], int ri, char ainame[], int rj, char ajname[] )
{
	STRAND_T	*sp;
	EXTBOND_T	*ebi, *ebj;
	RESIDUE_T	*resi, *resj;
	ATOM_T	*ap;
	int	a, ai, aj;

	for( sp = mol->m_strands; sp; sp = sp->s_next ){
		if( strcmp( sp->s_strandname, sname ) == 0 )
			break;
	}
	if( sp == NULL ){
		rt_errormsg_s( TRUE, E_NOSUCH_STRAND_S, sname );
		return( 1 );
	}
	if( ri < 1 || ri > sp->s_nresidues ){
		sprintf( e_msg, "#%2d not in strand %s", ri, sname );
		rt_errormsg_s( TRUE, E_NOSUCH_RESIDUE_S, e_msg );
		return( 1 );
	}else
		resi = sp->s_residues[ ri - 1 ];

	for( ai = UNDEF, a = 0; a < resi->r_natoms; a++ ){
		ap = &resi->r_atoms[ a ];
		if( strcmp( ap->a_atomname, ainame ) == 0 ){
			ai = a;
			break;
		}
	}
	if( ai == UNDEF ){
		sprintf( e_msg, "%s not in residue %s",
			ainame, resi->r_resname );
		rt_errormsg_s( TRUE, E_NOSUCH_ATOM_S, e_msg );
		return( 1 );
	}

	if( rj < 1 || rj > sp->s_nresidues ){
		sprintf( e_msg, "#%2d not in strand %s\n", rj, sname );
		rt_errormsg_s( TRUE, E_NOSUCH_RESIDUE_S, e_msg );
		return( 1 );
	}else
		resj = sp->s_residues[ rj - 1 ];

	for( aj = UNDEF, a = 0; a < resj->r_natoms; a++ ){
		ap = &resj->r_atoms[ a ];
		if( strcmp( ap->a_atomname, ajname ) == 0 ){
			aj = a;
			break;
		}
	}
	if( aj == UNDEF ){
		sprintf( e_msg, "%s not in residue %s",
			ajname, resj->r_resname );
		rt_errormsg_s( TRUE, E_NOSUCH_ATOM_S, e_msg );
		return( 1 );
	}

	if( ( ebi = ( EXTBOND_T * )malloc( sizeof( EXTBOND_T ) ) ) == NULL ){
		sprintf( e_msg, "bond between %s %d:%s and %s %d:%s",
			resi->r_resname, ri, ainame,
			resj->r_resname, rj, ajname );
		rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
		return( 1 );
	}else{
		ebi->eb_next = resi->r_extbonds;
		resi->r_extbonds = ebi;
		ebi->eb_anum = ai + 1;
		ebi->eb_rnum = rj;
		ebi->eb_ranum = aj + 1;
	}

	if( ( ebj = ( EXTBOND_T * )malloc( sizeof( EXTBOND_T ) ) ) == NULL ){
		sprintf( e_msg, "bond between %s %d:%s and %s %d:%s",
			resj->r_resname, rj, ajname,
			resi->r_resname, ri, ainame );
		rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
		return( 1 );
	}else{
		ebj->eb_next = resj->r_extbonds;
		resj->r_extbonds = ebj;
		ebj->eb_anum = aj + 1;
		ebj->eb_rnum = ri;
		ebj->eb_ranum = ai + 1;
	}

	return( 0 );
}

/*
 *	mergestr() merges two strands into a single strand.  NO bonds are
 *	made between the two merged strands.
 *	The details depend on whether mol1 == mol2.
 *
 *
 *	If mol1 != mol2 then
 *	mergestr() COPIES the residues in mol2:strand2 into mol1:strand1.
 *	If end1/end2 is "last"/"first", the copied residues are added after
 *	the last end (original last residue) of mol1:strand1.  If end1/end2
 *	is "first"/"last", the copied residues ares inserted before the first
 *	end (original 1st residue) of mol1:strand1.  Mol2:strand2 is
 *	unchanged by ligate().
 *
 *	If mol1 == mol2 then the residues are NOT copied, but moved into
 *	the appropriate place and strand2 is REMOVED from mol1.
 *
 */
int	mergestr( MOLECULE_T *mol1, char strand1[], char end1[],
	MOLECULE_T *mol2, char strand2[], char end2[] )
{
	int	after;
	int	nres, nsize;
	int	r1, r2;
	int	copy;
	STRAND_T	*sp, *sp1, *sp2;
	RESIDUE_T	**rap;

	if( strcmp( end1, "last" ) == 0 && strcmp( end2, "first" ) == 0 )
		after = TRUE;
	else if( strcmp( end1, "first" ) == 0 && strcmp( end2, "last" ) == 0 )
		after = FALSE;
	else{
		sprintf( e_msg, "%s/%s", end1, end2 );
		rt_errormsg_s( TRUE, E_LIGATE_BAD_ENDS_S, e_msg );
		return( 1 );
	}

	for( sp1 = NULL, sp = mol1->m_strands; sp; sp = sp->s_next ){
		if( strcmp( sp->s_strandname, strand1 ) == 0 ){
			sp1 = sp;
			break;
		}
	}
	if( sp1 == NULL ){
		rt_errormsg_s( TRUE, E_NOSUCH_STRAND_S, strand1 );
		return( 1 );
	}

	for( sp2 = NULL, sp = mol2->m_strands; sp; sp = sp->s_next ){
		if( strcmp( sp->s_strandname, strand2 ) == 0 ){
			sp2 = sp;
			break;
		}
	}
	if( sp2 == NULL ){
		rt_errormsg_s( TRUE, E_NOSUCH_STRAND_S, strand2 );
		return( 1 );
	}

	nres = sp1->s_nresidues + sp2->s_nresidues;
	if( nres > sp1->s_res_size ){
		/*nsize = sp1->s_res_size + 10;*/
		nsize = nres + 10;
		if( ( rap = ( RESIDUE_T ** )
			malloc( nsize * sizeof( RESIDUE_T * ) ) ) == NULL ){
			rt_errormsg_s( TRUE, E_NOMEM_FOR_S,
				"merged residue array" );
			return( 1 );
		}
		for( r1 = 0; r1 < sp1->s_nresidues; r1++ )
			rap[ r1 ] = sp1->s_residues[ r1 ];
		if( sp1->s_res_size > 0 )
			free( sp1->s_residues );
		sp1->s_residues = rap;
		sp1->s_res_size = nsize;
	}

	copy = mol1 != mol2;


	if( after ){
		r1 = sp1->s_nresidues;
		if( copy ){
			for( r2 = 0; r2 < sp2->s_nresidues; r2++ ){
				sp1->s_residues[ r1 + r2 ] = 
					copyresidue( sp2->s_residues[ r2 ] );
				sp1->s_residues[ r1 + r2 ]->r_strand = sp1;
			}
		}else{
			for( r2 = 0; r2 < sp2->s_nresidues; r2++ ){
				sp1->s_residues[ r1 + r2 ] = 
					sp2->s_residues[ r2 ];
				sp1->s_residues[ r1 + r2 ]->r_strand = sp1;
			}
		}
		for( r2 = 0; r2 < sp2->s_nresidues; r2++ )
			fixextbonds( sp1->s_residues[ r1 + r2 ], r1 );
	}else{
		r2 = sp2->s_nresidues;
		for( r1 = sp1->s_nresidues - 1; r1 >= 0; r1-- )
			sp1->s_residues[ r2 + r1 ] = sp1->s_residues[ r1 ];
		if( copy ){
			for( r2 = 0; r2 < sp2->s_nresidues; r2++ ){
				sp1->s_residues[ r2 ] = 
					copyresidue( sp2->s_residues[ r2 ] );
				sp1->s_residues[ r2 ]->r_strand = sp1;
			} 
		}else{
			for( r2 = 0; r2 < sp2->s_nresidues; r2++ ){
				sp1->s_residues[ r2 ] = sp2->s_residues[ r2 ];
				sp1->s_residues[ r2 ]->r_strand = sp1;
			} 
		} 
		for( r1 = 0; r1 < sp1->s_nresidues; r1++ ){
			fixextbonds( sp1->s_residues[ r2 + r1 ], r2 );
		}
	}
	sp1->s_nresidues = nres;

	if( !copy ) 
		freestrand( mol1, strand2 );

	mol1->m_nvalid = 0;
	upd_molnumbers( mol1 );

	return( 0 );
}

int	countmolstrands( MOLECULE_T *m, char *aex )
{
	int	n;
	STRAND_T	*sp;

	select_atoms( m, aex );
	for( n = 0, sp = m->m_strands; sp; sp = sp->s_next ){
		if( sp->s_attr & AT_SELECT )
			n++;
	} 
	return( n );
}

int	countstrandresidues( MOLECULE_T *m, int strandnum )
{
	STRAND_T	*sp;
	int	i;
	for( i = 1, sp = m->m_strands; (sp) && (i < strandnum); sp = sp->s_next ){
		i++;
	}
	return( sp->s_nresidues );
}

int	countmolres( MOLECULE_T *m, char *aex )
{
	int	r, n;
	STRAND_T	*sp;
	RESIDUE_T	*res;

	select_atoms( m, aex );
	for( n = 0, sp = m->m_strands; sp; sp = sp->s_next ){
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			if( res->r_attr & AT_SELECT )
				n++;
		}
	} 
	return( n );
}

int	countmolatoms( MOLECULE_T *m, char *aex )
{
	int	r, a, n;
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;

	select_atoms( m, aex );
	for( n = 0, sp = m->m_strands; sp; sp = sp->s_next ){
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			for( a = 0; a < res->r_natoms; a++ ){
				ap = &res->r_atoms[ a ];
				if( ap->a_attr & AT_SELECT )
					n++;
			}
		}
	} 
	return( n );
}

REAL_T	dist( MOLECULE_T *m, char aex1[], char aex2[] )
{
	POINT_T	p1, p2;

	setpoint( m, aex1, p1 );
	setpoint( m, aex2, p2 );
	return( distp( p1, p2 ) );
}

REAL_T	distp( POINT_T pi, POINT_T pj )
{
	REAL_T	dx, dy, dz;

	dx = pi[ 0 ] - pj[ 0 ];
	dy = pi[ 1 ] - pj[ 1 ];
	dz = pi[ 2 ] - pj[ 2 ];
	return( sqrt( dx * dx + dy * dy + dz * dz ) );
}

REAL_T	angle( MOLECULE_T *m, char aex1[], char aex2[], char aex3[] )
{
	POINT_T	p1, p2, p3;

	setpoint( m, aex1, p1 );
	setpoint( m, aex2, p2 );
	setpoint( m, aex3, p3 );
	return( anglep( p1, p2, p3 ) );
}

REAL_T	anglep( POINT_T p1, POINT_T p2, POINT_T p3 )
{
	REAL_T	x12, x32;
	REAL_T	y12, y32;
	REAL_T	z12, z32;
	REAL_T	l12, l32, dp;

	x12 = p1[ 0 ] - p2[ 0 ];
	y12 = p1[ 1 ] - p2[ 1 ];
	z12 = p1[ 2 ] - p2[ 2 ];
	x32 = p3[ 0 ] - p2[ 0 ];
	y32 = p3[ 1 ] - p2[ 1 ];
	z32 = p3[ 2 ] - p2[ 2 ];
	l12 = sqrt( x12 * x12 + y12 * y12 + z12 * z12 );
	l32 = sqrt( x32 * x32 + y32 * y32 + z32 * z32 );
	if( l12 == 0.0 ){
		fprintf( stderr,
			"anglep: p1, p2 are coincident, returning 0.0\n" );
		return( 0.0 );
	}
	if( l32 == 0.0 ){
		fprintf( stderr,
			"anglep: p2, p3 are coincident, returning 0.0\n" );
		return( 0.0 );
	}
	dp = x12 * x32 + y12 * y32 + z12 * z32;
	return( R2D * acos( dp / ( l12 * l32 ) ) );
}

REAL_T	torsion( MOLECULE_T *mol, char aei[], char aej[], char aek[], char ael[] )
{
	POINT_T	pi, pj, pk, pl;

	setpoint( mol, aei, pi );
	setpoint( mol, aej, pj );
	setpoint( mol, aek, pk );
	setpoint( mol, ael, pl );

	return( torsionp( pi, pj, pk, pl ) );
}

REAL_T	torsionp( POINT_T pi, POINT_T pj, POINT_T pk, POINT_T pl )
{
	REAL_T	xij, yij, zij;
	REAL_T	xkj, ykj, zkj;
	REAL_T	xkl, ykl, zkl;
	REAL_T	dx, dy, dz;
	REAL_T	gx, gy, gz;
	REAL_T	bi, bk;
	REAL_T	ct, d, ap, app; 

	xij = pi[ 0 ] - pj[ 0 ];
	yij = pi[ 1 ] - pj[ 1 ];
	zij = pi[ 2 ] - pj[ 2 ];
	xkj = pk[ 0 ] - pj[ 0 ];
	ykj = pk[ 1 ] - pj[ 1 ];
	zkj = pk[ 2 ] - pj[ 2 ];
	xkl = pk[ 0 ] - pl[ 0 ];
	ykl = pk[ 1 ] - pl[ 1 ];
	zkl = pk[ 2 ] - pl[ 2 ];

	dx = yij * zkj - zij * ykj;
        dy = zij * xkj - xij * zkj;
        dz = xij * ykj - yij * xkj;
        gx = zkj * ykl - ykj * zkl;
        gy = xkj * zkl - zkj * xkl;
        gz = ykj * xkl - xkj * ykl;

        bi = dx * dx + dy * dy + dz * dz;
        bk = gx * gx + gy * gy + gz * gz;
        ct = dx * gx + dy * gy + dz * gz;
        ct = ct / sqrt( bi * bk );
        if( ct < -1.0 )
		ct = -1.0;
        else if( ct > 1.0 )
		ct = 1.0;
 
	ap = acos( ct );
	d  = xkj*(dz*gy-dy*gz) + ykj*(dx*gz-dz*gx) + zkj*(dy*gx-dx*gy);
        if( d < 0.0 )
		ap = -ap;
        ap = PI - ap;
	app = 180.0 * ap / PI;
	if( app > 180.0 )
		app = app - 360.0;

	return( app );
}

char	*getresname( RESIDUE_T *res )
{
	static	char	*rname;

	if( ( rname = ( char * )malloc( sizeof( res->r_resname ) ) ) == NULL ){
		return( NULL );
	}
	strcpy( rname, res->r_resname );
	return( rname );
}

int	cap( MOLECULE_T *mol, char *aex, int five, int three )
{
	STRAND_T	*sp;

	select_atoms( mol, aex );
	for( sp = mol->m_strands; sp; sp = sp->s_next ){
		if( sp->s_nresidues > 0 && ( sp->s_attr & AT_SELECT ) ){
			if( five )
				cvt_p2hb( sp->s_residues[ 0 ] );
			if( three )
				add_he2o3( sp->s_residues[sp->s_nresidues-1] );
		}
	}
	mol->m_nvalid = 0;
	return( 0 );
}

EXTBOND_T	*copyextbonds( RESIDUE_T *res )
{
	EXTBOND_T	*ep, *ep1, *epo, *epn;

	for( epn = ep1 = NULL, epo = res->r_extbonds; epo; epo = epo->eb_next ){
		if( ( ep = ( EXTBOND_T * )malloc( sizeof( EXTBOND_T ) ) ) ==
			NULL ){
			sprintf( e_msg, "copied external bonds" );
			rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
			return( NULL );
		}
		ep->eb_next = NULL;
		ep->eb_anum = epo->eb_anum;
		ep->eb_rnum = epo->eb_rnum;
		ep->eb_ranum = epo->eb_ranum;
		if( epn == NULL )
			epn = ep;
		if( ep1 != NULL )
			ep1->eb_next = ep;
		ep1 = ep;
	}

	return( epn );
}

int	setreskind( MOLECULE_T *m, char aexp[], char rkind[] )
{
	STRAND_T	*sp;
	RESIDUE_T	*res;
	int	r;
	int	rk;
	char	*rp, *tp, trkind[ 10 ];

	for( tp = trkind, rp = rkind; *rp; rp++ )
		*tp++ = isupper( *rp ) ? tolower( *rp ) : *rp;
	*tp = '\0';

	if( !strcmp( trkind, "dna" ) )
		rk = RT_DNA;
	else if( !strcmp( trkind, "rna" ) )
		rk = RT_RNA;
	else if( !strcmp( trkind, "aa" ) )
		rk = RT_AA;
	else{
		fprintf( stderr,
			"setreskind: ERROR: unknown rkind %s\n", rkind );
		rk = RT_UNDEF;
	}

	select_atoms( m, aexp );
	for( sp = m->m_strands; sp; sp = sp->s_next ){
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			if( res->r_attr & AT_SELECT )
				res->r_kind = rk;
		}
	}
	return( 0 );
}

int	setxyz_from_mol( MOLECULE_T **m, char **aex, POINT_T xyz[] )
{
	int		r, a, n;
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;

	select_atoms( *m, aex ? *aex : NULL );
	for( n = 0, sp = (*m)->m_strands; sp; sp = sp->s_next ){
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			for( a = 0; a < res->r_natoms; a++ ){
				ap = &res->r_atoms[ a ];
				if( ap->a_attr & AT_SELECT ){
					xyz[ n ][ 0 ] = ap->a_pos[ 0 ];
					xyz[ n ][ 1 ] = ap->a_pos[ 1 ];
					xyz[ n ][ 2 ] = ap->a_pos[ 2 ];
					n++;
				}
			}
		}
	} 
	return( n );
}

int	setxyzw_from_mol( MOLECULE_T **m, char **aex, REAL_T xyzw[] )
{
	int		r, a, n;
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;

	select_atoms( *m, aex ? *aex : NULL );
	for( n = 0, sp = (*m)->m_strands; sp; sp = sp->s_next ){
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			for( a = 0; a < res->r_natoms; a++ ){
				ap = &res->r_atoms[ a ];
				if( ap->a_attr & AT_SELECT ){
					xyzw[ 4*n + 0 ] = ap->a_pos[ 0 ];
					xyzw[ 4*n + 1 ] = ap->a_pos[ 1 ];
					xyzw[ 4*n + 2 ] = ap->a_pos[ 2 ];
					xyzw[ 4*n + 3 ] = ap->a_w;
					n++;
				}
			}
		}
	} 
	return( n );
}

int	setmol_from_xyz( MOLECULE_T **m, char **aex, REAL_T xyz[] )
{
	int		r, a, n;
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;

	select_atoms( *m, aex ? *aex : NULL );
	for( n = 0, sp = (*m)->m_strands; sp; sp = sp->s_next ){
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			for( a = 0; a < res->r_natoms; a++ ){
				ap = &res->r_atoms[ a ];
				if( ap->a_attr & AT_SELECT ){
					ap->a_pos[ 0 ] = xyz[ 3*n + 0 ];
					ap->a_pos[ 1 ] = xyz[ 3*n + 1 ];
					ap->a_pos[ 2 ] = xyz[ 3*n + 2 ];
					n++;
				}
			}
		}
	} 
	return( n );
}

int	setmol_from_xyzw( MOLECULE_T **m, char **aex, REAL_T xyzw[] )
{
	int		r, a, n;
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;

	select_atoms( *m, aex ? *aex : NULL );
	for( n = 0, sp = (*m)->m_strands; sp; sp = sp->s_next ){
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			for( a = 0; a < res->r_natoms; a++ ){
				ap = &res->r_atoms[ a ];
				if( ap->a_attr & AT_SELECT ){
					ap->a_pos[ 0 ] = xyzw[ 4*n + 0 ];
					ap->a_pos[ 1 ] = xyzw[ 4*n + 1 ];
					ap->a_pos[ 2 ] = xyzw[ 4*n + 2 ];
					ap->a_w        = xyzw[ 4*n + 3 ];
					n++;
				}
			}
		}
	} 
	return( n );
}

int	NAB_ainit( char *a[], int s )
{
	int	i;

	for( i = 0; i < s; i++ )
		a[ i ] = NULL;
	return( 0 );
}

void	NAB_initres( RESIDUE_T *res, int init_str )
{

	res->r_next = NULL;
	res->r_num = 0;
	res->r_tresnum = 0;
	res->r_resnum = 0;
	if( init_str ){
		res->r_resname = NULL;
		res->r_resid = NULL;
	}
	res->r_attr = 0;
	res->r_kind = RT_UNDEF;
	res->r_atomkind = RAT_UNDEF;
	res->r_strand = NULL;
	res->r_extbonds = NULL;
	res->r_nintbonds = 0;
	res->r_intbonds = NULL;
	res->r_atomkind = RAT_UNDEF;
	res->r_nchiral = 0;
	res->r_chiral = NULL;
	res->r_natoms = 0;
	res->r_aindex = NULL;
	res->r_atoms = NULL;
}

void	NAB_initatom( ATOM_T *ap, int init_str  )
{
	int	i;

	if( init_str )
		ap->a_atomname = NULL;
	if( init_str )
		ap->a_atomtype = NULL;
	ap->a_attr     = 0;
	ap->a_nconnect = 0;
	for( i = 0; i < A_CONNECT_SIZE; i++ )
		ap->a_connect[ i ] = 0;
	ap->a_residue  = NULL;
	ap->a_charge   = 0;
	ap->a_radius   = 0;
	ap->a_bfact    = 0;
	ap->a_occ      = 1.0;
	if( init_str )
		ap->a_element = NULL;
	ap->a_int1     = 0;
	ap->a_float1   = 0;
	ap->a_float2   = 0;
	ap->a_tatomnum = 0;
	ap->a_atomnum  = 0;
	if( init_str )
		ap->a_fullname = NULL;
	ap->a_pos[ 0 ] = 0;
	ap->a_pos[ 1 ] = 0;
	ap->a_pos[ 2 ] = 0;
	ap->a_w        = 0;
}

ATOM_T	*NAB_mnext( MOLECULE_T *mol, ATOM_T *cap )
{
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;
	int		r, nr, a;

	if( !cap ){
		for( sp = mol->m_strands;
			sp && sp->s_nresidues == 0; sp = sp->s_next )
			;
		if( !sp )
			return( NULL );
		res = sp->s_residues[ 0 ];
		ap = &res->r_atoms[ 0 ];
		return( ap );
	}else{
		res = cap->a_residue;
		a = cap - res->r_atoms + 1;
		if( a < res->r_natoms ){
			ap = &res->r_atoms[ a ];
			return( ap );
		}
		/* try next residue	*/
		sp = res->r_strand;
		for( nr = sp->s_nresidues, r = 0; r < sp->s_nresidues; r++ ){
			if( sp->s_residues[ r ] == res ){
				nr = r + 1; 
				break;
			}
		}
		if( nr < sp->s_nresidues ){
			res = sp->s_residues[ nr ];
			ap = &res->r_atoms[ 0 ];
			return( ap );
		}
		/* try next strand */
		for( sp = sp->s_next;
			sp && sp->s_nresidues == 0; sp = sp->s_next )
			;
		if( !sp )
			return( NULL );
		res = sp->s_residues[ 0 ];
		ap = &res->r_atoms[ 0 ];
		return( ap );
	}
}

RESIDUE_T	*NAB_rnext( MOLECULE_T *mol, RESIDUE_T *crp )
{
	STRAND_T	*sp;
	RESIDUE_T	*res;
	int		r, nr;

	if( !crp ){
		for( sp = mol->m_strands;
			sp && sp->s_nresidues == 0; sp = sp->s_next )
			;
		if( !sp )
			return( NULL );
		res = sp->s_residues[ 0 ];
		return( res );
	}else{
		/* get next residue	*/
		sp = crp->r_strand;
		for( nr = sp->s_nresidues, r = 0; r < sp->s_nresidues; r++ ){
			if( sp->s_residues[ r ] == crp ){
				nr = r + 1; 
				break;
			}
		}
		if( nr < sp->s_nresidues ){
			res = sp->s_residues[ nr ];
			return( res );
		}
		/* try next strand */
		for( sp = sp->s_next;
			sp && sp->s_nresidues == 0; sp = sp->s_next )
			;
		if( !sp )
			return( NULL );
		res = sp->s_residues[ 0 ];
		return( res );
	}
}

ATOM_T		*NAB_anext( RESIDUE_T *res, ATOM_T *cap )
{
	ATOM_T		*ap;
	int		na;

	if( !cap ){
		ap = res->r_atoms;
		return( ap );
	}else{
		/* get next atom	*/
		na = ( cap - res->r_atoms ) + 1;
		if( na < res->r_natoms ){
			ap = &res->r_atoms[ na ];
			return( ap );
		}else
			return( NULL );
	}
}

int	*NAB_mri( MOLECULE_T *mol, char key[] )
{

	if( !strcmp( key, "nstrands" ) ){
		if( !mol->m_nvalid )
			upd_molnumbers( mol );
		return( &mol->m_nstrands );
	}else if( !strcmp( key, "nresidues" ) ){
		if( !mol->m_nvalid )
			upd_molnumbers( mol );
		return( &mol->m_nresidues );
	}else if( !strcmp( key, "natoms" ) ){
		if( !mol->m_nvalid )
			upd_molnumbers( mol );
		return( &mol->m_natoms );
	}else
		fprintf( stderr, "NAB_mri: unknown key: %s\n", key );
	return( 0 );
}
	
int	*NAB_rri( RESIDUE_T *res, char key[] )
{
	STRAND_T	*sp;
	MOLECULE_T	*mp;
	static	int	rv_err;

	if( !strcmp( key, "resnum" ) ){
		sp = res->r_strand;
		mp = sp->s_molecule;
		if( !mp->m_nvalid )
			upd_molnumbers( mp );
		return( &res->r_resnum );
	}else if( !strcmp( key, "tresnum" ) ){
		sp = res->r_strand;
		mp = sp->s_molecule;
		if( !mp->m_nvalid )
			upd_molnumbers( mp );
		return( &res->r_tresnum );
	}else if( !strcmp( key, "strandnum" ) ){
		sp = res->r_strand;
		mp = sp->s_molecule;
		if( !mp->m_nvalid )
			upd_molnumbers( mp );
		return( &sp->s_strandnum );
	}else
		fprintf( stderr, "NAB_rri: unknown key: %s\n", key );
	rv_err = 0;
	return( &rv_err );
}
	
char	**NAB_rrc( RESIDUE_T *res, char key[] )
{
	STRAND_T	*sp;

	if( !strcmp( key, "resname" ) ){
		return( &res->r_resname );
	}else if( !strcmp( key, "resid" ) ){
		return( &res->r_resid );
	}else if( !strcmp( key, "strandname" ) ){
		sp = res->r_strand;
		return( &sp->s_strandname );
	}else{
		fprintf( stderr, "NAB_rrc: unknown key: %s\n", key );
		return( NULL );
	}
}

int	*NAB_ari( ATOM_T *ap, char key[] )
{
	static	int	rv_err;
	RESIDUE_T	*res;
	STRAND_T	*sp;
	MOLECULE_T	*mp;

	if( !strcmp( key, "strandnum" ) ){
		res = ap->a_residue;
		sp = res->r_strand;
		mp = sp->s_molecule;
		if( !mp->m_nvalid )
			upd_molnumbers( mp );
		return( &sp->s_strandnum );
	}else if( !strcmp( key, "resnum" ) ){
		res = ap->a_residue;
		sp = res->r_strand;
		mp = sp->s_molecule;
		if( !mp->m_nvalid )
			upd_molnumbers( mp );
		return( &res->r_resnum );
	}else if( !strcmp( key, "tresnum" ) ){
		res = ap->a_residue;
		sp = res->r_strand;
		mp = sp->s_molecule;
		if( !mp->m_nvalid )
			upd_molnumbers( mp );
		return( &res->r_tresnum );
	}else if( !strcmp( key, "atomnum" ) ){
		res = ap->a_residue;
		sp = res->r_strand;
		mp = sp->s_molecule;
		if( !mp->m_nvalid )
			upd_molnumbers( mp );
		return( &ap->a_atomnum );
	}else if( !strcmp( key, "tatomnum" ) ){
		res = ap->a_residue;
		sp = res->r_strand;
		mp = sp->s_molecule;
		if( !mp->m_nvalid )
			upd_molnumbers( mp );
		return( &ap->a_tatomnum );
	}else if( !strcmp( key, "int1" ) ){
		return( &ap->a_int1 );
	}else
		fprintf( stderr, "NAB_ari: unknown key: %s\n", key );
	rv_err = 0;
	return( &rv_err );
}

REAL_T	*NAB_arf( ATOM_T *ap, char key[] )
{
	static	REAL_T	f;

	if( !strcmp( key, "x" ) )
		return( &ap->a_pos[ 0 ] );
	else if( !strcmp( key, "y" ) )
		return( &ap->a_pos[ 1 ] );
	else if( !strcmp( key, "z" ) )
		return( &ap->a_pos[ 2 ] );
	else if( !strcmp( key, "charge" ) )
		return( &ap->a_charge );
	else if( !strcmp( key, "radius" ) )
		return( &ap->a_radius );
	else if( !strcmp( key, "float1" ) )
		return( &ap->a_float1 );
	else if( !strcmp( key, "float2" ) )
		return( &ap->a_float2 );
	else{
		fprintf( stderr, "NAB_arf: unknown key: %s\n", key );
		f = 0.0;
		return( &f );
	}
}

char	**NAB_arc( ATOM_T *ap, char key[] )
{
	RESIDUE_T	*res;
	STRAND_T	*sp, *sp1;
	MOLECULE_T	*mp;
	int		s, r;
	char	name[ 100 ];

	if( !strcmp( key, "atomname" ) ){
		return( &ap->a_atomname );
	}else if( !strcmp( key, "resname" ) ){
		res = ap->a_residue;
		return( &res->r_resname );
	}else if( !strcmp( key, "resid" ) ){
		res = ap->a_residue;
		return( &res->r_resid );
	}else if( !strcmp( key, "strandname" ) ){
		res = ap->a_residue;
		sp = res->r_strand;
		return( &sp->s_strandname );
	}else if( !strcmp( key, "fullname" ) ){
		res = ap->a_residue;
		sp = res->r_strand;
		mp = sp->s_molecule;	
		upd_molnumbers( mp );
		for( r = 0; r < sp->s_nresidues; r++ )
			if( sp->s_residues[ r ] == res )
				break;
		for( s = 0, sp1 = mp->m_strands; sp1; sp1 = sp1->s_next ){
			s++;
			if( sp1 == sp )
				break;
		}
		sprintf( name, "%d:%d:%s", s, r + 1, ap->a_atomname );
		if( ap->a_fullname )
			free( ap->a_fullname );
		ap->a_fullname = ( char * )malloc( (strlen( name ) + 1) * sizeof(char) );
		strcpy( ap->a_fullname, name );
		return( &ap->a_fullname );
	}else{
		fprintf( stderr, "NAB_arc: unknown key: %s\n", key );
		return( NULL );
	}
}

POINT_T	*NAB_arp( ATOM_T *ap, char key[] )
{
	void	*temp;

	temp = ap->a_pos;
	return( temp );
}

void	upd_molnumbers( MOLECULE_T *mp )
{
	int	s, r, a, a1;
	int	ta, tr;
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;

	for( ta = tr = s = 0, sp = mp->m_strands; sp; s++, sp = sp->s_next ){
		sp->s_strandnum = s + 1;
		for( a = r = 0; r < sp->s_nresidues; tr++, r++ ){
			res = sp->s_residues[ r ];
			res->r_tresnum = tr + 1;
			res->r_resnum = r + 1;
			for( a1 = 0; a1 < res->r_natoms; a1++, ta++, a++ ){
				ap = &res->r_atoms[ a1 ];
				ap->a_atomnum = a + 1;
				ap->a_tatomnum = ta + 1;
			}
		}
	}
	mp->m_nstrands = s;
	mp->m_nresidues = tr;
	mp->m_natoms = ta;
	mp->m_nvalid = 1;
}

MATRIX_T	*newtransform( REAL_T dx, REAL_T dy, REAL_T dz, REAL_T rx, REAL_T ry, REAL_T rz )
{
	void	*temp;
	static	MATRIX_T	mp;
	MATRIX_T	rmat, r1mat, tmat;

	/* nucleic acid z rotation is reversed from C-lib!	*/
	rz = -rz;

	mk_idmat( mp );
	mp[ 3 ][ 0 ] = dx;
	mp[ 3 ][ 1 ] = dy;
	mp[ 3 ][ 2 ] = dz;

	mk_idmat( rmat );
	if( rx != 0.0 ){
		mk_idmat( r1mat );
		rmat[ 1 ][ 1 ] = cos( D2R * rx );
		rmat[ 1 ][ 2 ] = -sin( D2R * rx );
		rmat[ 2 ][ 1 ] = sin( D2R * rx );
		rmat[ 2 ][ 2 ] = cos( D2R * rx );
		concat_mat( rmat, r1mat, tmat );
		copy_mat( tmat, rmat );
	}
	if( ry != 0.0 ){
		mk_idmat( r1mat );
		rmat[ 0 ][ 0 ] = cos( D2R * ry );
/*
		rmat[ 0 ][ 2 ] = sin( D2R * ry );
		rmat[ 2 ][ 0 ] = -sin( D2R * ry );
*/
		rmat[ 0 ][ 2 ] = -sin( D2R * ry );
		rmat[ 2 ][ 0 ] = sin( D2R * ry );
		rmat[ 2 ][ 2 ] = cos( D2R * ry );
		concat_mat( rmat, r1mat, tmat );
		copy_mat( tmat, rmat );
	}
	if( rz != 0.0 ){
		mk_idmat( r1mat );
		rmat[ 0 ][ 0 ] = cos( D2R * rz );
		rmat[ 0 ][ 1 ] = -sin( D2R * rz );
		rmat[ 1 ][ 0 ] = sin( D2R * rz );
		rmat[ 1 ][ 1 ] = cos( D2R * rz );
		concat_mat( rmat, r1mat, tmat );
		copy_mat( tmat, rmat );
	}

	concat_mat( rmat, mp, tmat );
	copy_mat( tmat, mp );

	temp = mp;
	return( temp );
}

RESIDUE_T	*transformres( MATRIX_T mat, RESIDUE_T *res, char *aexp )
{
	RESIDUE_T	*nres;
	int		a, i;
	ATOM_T		*ap;
	POINT_T		xyz, nxyz;

	nres = copyresidue( res );
	for( a = 0; a < nres->r_natoms; a++ ){
		ap = &nres->r_atoms[ a ];
		for( i = 0; i < 3; i++ )
			xyz[ i ] = ap->a_pos[ i ];
		xfm_xyz( xyz, mat, nxyz );
		for( i = 0; i < 3; i++ )
			ap->a_pos[ i ] = nxyz[ i ];
	}

	return( nres );
}

int	transformmol( MATRIX_T mat, MOLECULE_T *mol, char *aexp )
{
	STRAND_T	*sp;
	RESIDUE_T	*res;
	int		r, a, i, k;
	ATOM_T		*ap;
	POINT_T		xyz, nxyz;

	select_atoms( mol, aexp );

	for( sp = mol->m_strands, k=0; sp; sp = sp->s_next ){
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			for( a = 0; a < res->r_natoms; a++ ){
				ap = &res->r_atoms[ a ];
				if( ap->a_attr & AT_SELECT ){
					k++;
					for( i = 0; i < 3; i++ )
						xyz[ i ] = ap->a_pos[ i ];
					xfm_xyz( xyz, mat, nxyz );
					for( i = 0; i < 3; i++ )
						ap->a_pos[ i ] = nxyz[ i ];
				}
			}
		}
	}

	return( k );
}

int	transformpts( MATRIX_T mat, POINT_T pts[], int npts )
{
	int	i;

	for( i = 0; i < npts; i++ )
		xfm_xyz( pts[ i ], mat, pts[ i ] );
	return( 0 );
}

MATRIX_T	*updtransform( MATRIX_T m1, MATRIX_T m2 )
{
	void	*temp;
	static	MATRIX_T	mr;


	concat_mat( m1, m2, mr );
	temp = mr;
	return( temp );
}

MATRIX_T	*trans4p( POINT_T p1, POINT_T p2, REAL_T d )
{
	void	*temp;
	REAL_T	dx, dy, dz, len;
	static	MATRIX_T	mat;

	dx = p2[ 0 ] - p1[ 0 ];
	dy = p2[ 1 ] - p1[ 1 ];
	dz = p2[ 2 ] - p1[ 2 ];
	if( ( len = sqrt( dx * dx + dy * dy + dz * dz ) ) == 0.0 ){
		NAB_matcpy( mat, newtransform( 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 ) );
	}else{
		dx /= len;
		dy /= len;
		dz /= len;
		NAB_matcpy( mat,
			newtransform( d * dx, d * dy, d * dz, 0.0, 0.0, 0.0 ) );
	}
	temp = mat;
	return( temp );
}

MATRIX_T	*trans4( MOLECULE_T *mol, char aex1[], char aex2[], REAL_T d )
{
	POINT_T	p1, p2;

	setpoint( mol, aex1, p1 );
	setpoint( mol, aex2, p2 );
	return( trans4p( p1, p2, d ) );
}

MATRIX_T	*rot4p( POINT_T p1, POINT_T p2, REAL_T angle )
{
	void	*temp;
	MATRIX_T	mat1, mat2, mat3, mat4;
	static	MATRIX_T	mat5;

	NAB_matcpy( mat1,
		newtransform( -p1[0], -p1[1], -p1[2], 0.0, 0.0, 0.0 ) );

	raa2mat( p2[0]-p1[0], p2[1]-p1[1], p2[2]-p1[2], -angle, mat2 );
	
	NAB_matcpy( mat3, newtransform( p1[0], p1[1], p1[2], 0.0, 0.0, 0.0 ) );

	concat_mat( mat1, mat2, mat4 );
	concat_mat( mat4, mat3, mat5 );

	temp = mat5;
	return( temp );
}

MATRIX_T	*rot4( MOLECULE_T *mol, char aex1[], char aex2[], REAL_T angle )
{
	POINT_T	p1, p2;

	setpoint( mol, aex1, p1 );
	setpoint( mol, aex2, p2 );

	return( rot4p( p1, p2, angle ) );
}

MATRIX_T	*NAB_matcpy( void *mdst, void *msrc )
{

	return( ( MATRIX_T * )memcpy( mdst, msrc, sizeof( MATRIX_T ) ) );
}

static	void	raa2mat( REAL_T x, REAL_T y, REAL_T z, REAL_T angle, MATRIX_T mat )	
{
	REAL_T	axlen;
	REAL_T	a1, a2, a3, a1a1, a1a2, a1a3, a2a2, a2a3, a3a3;
	REAL_T           ca1a2, ca2a3, ca1a3, sa1, sa2, sa3;
	REAL_T  c, s;	/* cosine and sine of angle */

	if( ( axlen = hypot( hypot( x, y ), z ) ) == 0.0 ){
		x = 0.0;
		y = 0.0;
		z = 1.0;
		axlen = 1.0;
	}
	x /= axlen;
	y /= axlen;
	z /= axlen;

	a1 = x;
	a2 = y;
	a3 = z;
	a1a1 = x * x;
	a1a2 = x * y;
	a1a3 = x * z;
	a2a2 = y * y;
	a2a3 = y * z;
	a3a3 = z * z;
	s = sin( D2R * angle );
	c = cos( D2R * angle );

	ca1a2 = c * a1a2;
	ca2a3 = c * a2a3;
	ca1a3 = c * a1a3;
	sa1 = s * a1;
	sa2 = s * a2;
	sa3 = s * a3;

	mat[ 0 ][ 0 ] = DIAG( a1a1 );
	mat[ 0 ][ 1 ] = NDIAG( a1a2, -ca1a2, -sa3 );
	mat[ 0 ][ 2 ] = NDIAG( a1a3, -ca1a3, sa2 );
	mat[ 0 ][ 3 ] = 0.0;

	mat[ 1 ][ 0 ] = NDIAG( a1a2, -ca1a2, sa3 );
	mat[ 1 ][ 1 ] = DIAG( a2a2 );
	mat[ 1 ][ 2 ] = NDIAG( a2a3, -ca2a3, -sa1 );
	mat[ 1 ][ 3 ] = 0.0;

	mat[ 2 ][ 0 ] = NDIAG( a1a3, -ca1a3, -sa2 );
	mat[ 2 ][ 1 ] = NDIAG( a2a3, -ca2a3, sa1 );
	mat[ 2 ][ 2 ] = DIAG( a3a3 );
	mat[ 2 ][ 3 ] = 0.0;

	mat[ 3 ][ 0 ] = 0.0;
	mat[ 3 ][ 1 ] = 0.0;
	mat[ 3 ][ 2 ] = 0.0;
	mat[ 3 ][ 3 ] = 1.0;
}

static	void	mk_idmat( MATRIX_T idmat )
{
	int	i, j;

	for( i = 0; i < 4; i++ ){
		for( j = 0; j < 4; j++ ){
			idmat[ i ][ j ] = ( i == j ) ? 1.0 : 0.0;
		}
	}
}

static	void	concat_mat( MATRIX_T m1, MATRIX_T m2, MATRIX_T m3 )
{
	int	i, j, k;

	for( i = 0; i < 4; i++ ){
		for( j = 0; j < 4; j++ ){
			m3[ i ][ j ] = 0.0;
			for( k = 0; k < 4; k++ ){
				m3[ i ][ j ] += m1[ i ][ k ] * m2[ k ][ j ];
			}
		}
	}
}

static	void	copy_mat( MATRIX_T mold, MATRIX_T mnew )
{
	int	i, j;
	
	for( i = 0; i < 4; i++ ){
		for( j = 0; j < 4; j++ )
			mnew[ i ][ j ] = mold[ i ][ j ];
	}
}

static	void	xfm_xyz( POINT_T oxyz, MATRIX_T mat, POINT_T nxyz )
{
	int	i, j;
	REAL_T	oxyz4[ 4 ], nxyz4[ 4 ];

	for( i = 0; i < 3; i++ )
		oxyz4[ i ] = oxyz[ i ];
	oxyz4[ 3 ] = 1.0;

	for( i = 0; i < 4; i++ ){
		nxyz4[ i ] = 0.0;
		for( j = 0; j < 4; j++ ){
			nxyz4[ i ] += oxyz4[ j ] * mat[ j ][ i ];
		}
	}

	for( i = 0; i < 3; i++ )
		nxyz[ i ] = nxyz4[ i ];
}

static	void	fixextbonds( RESIDUE_T *res, int roff )
{
	EXTBOND_T	*ep;

	for( ep = res->r_extbonds; ep; ep = ep->eb_next )
		ep->eb_rnum += roff;
}

static	void	freestrand( MOLECULE_T *mol, char sname[] )
{
	STRAND_T	*sp, *spl, *sprm;

	for( spl = sprm = NULL, sp = mol->m_strands; sp; sp = sp->s_next ){
		if( strcmp( sp->s_strandname, sname ) == 0 ){
			sprm = sp;
			break;
		}
		spl = sp;
	}

	if( sprm == NULL ){
		rt_errormsg_s( TRUE, E_NOSUCH_STRAND_S, sname );
		return;
	}

	if( spl == NULL )
		mol->m_strands = sprm->s_next;
	else
		spl->s_next = sprm->s_next;
	mol->m_nstrands--;
}

static	void	cvt_p2hb( RESIDUE_T *res )
{
	int	p, o5, oxp;
	REAL_T	dx, dy, dz, f;
	ATOM_T	*ap;

	p = find_atom( res, "P" );
	if( ( o5 = find_atom( res, "O5'" ) ) == UNDEF )
		o5 = find_atom( res, "O5*" );

	if( o5 == UNDEF )
		return;
	else if( p != UNDEF ){
		if( ( oxp = find_atom( res, "O1P" ) ) == UNDEF ){
			if( ( oxp = find_atom( res, "OAP" ) ) != UNDEF ) 
				delete_atom( res, oxp );
		}else
			delete_atom( res, oxp );
		if( ( oxp = find_atom( res, "O2P" ) ) == UNDEF ){
			if( ( oxp = find_atom( res, "OBP" ) ) != UNDEF ) 
				delete_atom( res, oxp );
		}else
			delete_atom( res, oxp );
		ap = &res->r_atoms[ p ];
		strcpy( ap->a_atomname, "HB" );
		if( ( o5 = find_atom( res, "O5'" ) ) == UNDEF )
			o5 = find_atom( res, "O5*" );
		/* adjust length of HB-O5' bond	*/
		dx = res->r_atoms[ p ].a_pos[ 0 ] -
			res->r_atoms[ o5 ].a_pos[ 0 ];
		dy = res->r_atoms[ p ].a_pos[ 1 ] -
			res->r_atoms[ o5 ].a_pos[ 1 ];
		dz = res->r_atoms[ p ].a_pos[ 2 ] -
			res->r_atoms[ o5 ].a_pos[ 2 ];
		f = HBE_DIST / sqrt( dx * dx + dy * dy + dz * dz );
		res->r_atoms[ p ].a_pos[ 0 ] = f * dx + 
			res->r_atoms[ o5 ].a_pos[ 0 ];
		res->r_atoms[ p ].a_pos[ 1 ] = f * dy + 
			res->r_atoms[ o5 ].a_pos[ 1 ];
		res->r_atoms[ p ].a_pos[ 2 ] = f * dz + 
			res->r_atoms[ o5 ].a_pos[ 2 ];
	}
}

static	void	add_he2o3( RESIDUE_T *res )
{
	int	c3, o3, he;
	REAL_T	dx, dy, dz, dist;
	REAL_T	cx, cy, cz;
	REAL_T	sx, sy, sz;
	REAL_T	f;
	ATOM_T	*aph, *apo;

	if( find_atom( res, "HE" ) != UNDEF )
		return;
	if( ( o3 = find_atom( res, "O3'" ) ) == UNDEF )
		o3 = find_atom( res, "O3*" );
	if( ( c3 = find_atom( res, "C3'" ) ) == UNDEF )
		c3 = find_atom( res, "C3*" );

	if( c3 == UNDEF || o3 == UNDEF )
		return;

	add_atom( res, "HE" );
	he = find_atom( res, "HE" );
	dx = res->r_atoms[ o3 ].a_pos[ 0 ] -
		res->r_atoms[ c3 ].a_pos[ 0 ];
	dy = res->r_atoms[ o3 ].a_pos[ 1 ] -
		res->r_atoms[ c3 ].a_pos[ 1 ];
	dz = res->r_atoms[ o3 ].a_pos[ 2 ] -
		res->r_atoms[ c3 ].a_pos[ 2 ];
	dist = sqrt( dx * dx + dy * dy + dz * dz );
	f = HBE_DIST * cos( D2R * ( 180.0 - HBE_ANGLE ) ) / dist;
	cx = f * dx;
	cy = f * dy;
	cz = f * dz;
	if( cy != 0.0 ){
		sx = 1.0;
		sy = -cx / cy;
		sz = 0.0;
	}else{
		sx = 0.0;
		sy = 1.0;
		sz = 0.0;
	}
	dist = sqrt( sx * sx + sy * sy + sz * sz );
	f = HBE_DIST * sin( D2R * ( 180.0 - HBE_ANGLE ) ) / dist;
	sx = f * sx;
	sy = f * sy;
	sz = f * sz;
	
	apo = &res->r_atoms[ o3 ];
	aph = &res->r_atoms[ he ];
	aph->a_pos[ 0 ] = apo->a_pos[ 0 ] + cx + sx;
	aph->a_pos[ 1 ] = apo->a_pos[ 1 ] + cy + sy;
	aph->a_pos[ 2 ] = apo->a_pos[ 2 ] + cz + sz;
}

static	int	find_atom( RESIDUE_T *res, char aname[] )
{
	int	a;
	ATOM_T	*ap;

	for( a = 0, ap = res->r_atoms; a < res->r_natoms; a++, ap++ ){
		if( strcmp( ap->a_atomname, aname ) == 0 )
			return( a );	
	}
	return( UNDEF );
}

static	int	delete_atom( RESIDUE_T *res, int anum )
{
	int	a, ac, c, c1, c2;
	ATOM_T	*ap, *apc;

	if( anum < 0 || anum >= res->r_natoms )
		return( 1 );
	ap = &res->r_atoms[ anum ];
	for( c = 0; c < ap->a_nconnect; c++ ){
		ac = ap->a_connect[ c ];
		apc = &res->r_atoms[ ac ];
		for( c1 = 0; c1 < apc->a_nconnect; c1++ ){
			if( apc->a_connect[ c1 ] == anum ){
				for( c2 = c1; c2 < apc->a_nconnect - 1; c2++ ){
					apc->a_connect[ c2 ] = 	
						apc->a_connect[ c2 + 1 ];
				}
				apc->a_nconnect--;
				for( c2 = apc->a_nconnect;
					c2 < A_CONNECT_SIZE; c2++ )
					apc->a_connect[ c2 ] = UNDEF;
				break;
			}
		}
	}
	for( a = anum; a < res->r_natoms - 1; a++ )
		res->r_atoms[ a ] = res->r_atoms[ a + 1 ];
	res->r_natoms--;
	for( a = 0; a < res->r_natoms - 1; a++ ){
		ap = &res->r_atoms[ a ];
		for( c = 0; c < ap->a_nconnect; c++ ){
			if( ap->a_connect[ c ] > anum )
				ap->a_connect[ c ]--;
		}
	}
	return(0);
}

static	int	add_atom( RESIDUE_T *res, char aname[] )
{
	int	na, a, c;
	ATOM_T	*ap, *nap, *oap;
	int	*nai, *oai;
	char	*anp;

	na = res->r_natoms + 1;
	if( ( nap = ( ATOM_T * )malloc( na * sizeof( ATOM_T ) ) ) == NULL ){
		fprintf( stderr, "Can't create new atom array\n" );
		return 1;
	}
	if( ( nai = ( int * )malloc( na * sizeof( int ) ) ) == NULL ){
		fprintf( stderr, "Can't create new atom index array\n" );
		return 1;
	}
	oap = res->r_atoms;
	for( a = 0; a < na - 1; a++ )
		nap[ a ] = oap[ a ];
	ap = &nap[ na - 1 ];
	anp = ( char * )malloc( strlen( ap->a_atomname ) + 1 );
	if( anp == NULL ){
		fprintf( stderr, "add_atom: can't allocate anp.\n" );
		exit( 1 );
	}
	ap->a_atomname = anp;
	strcpy( ap->a_atomname, aname );
	ap->a_attr = 0;
	ap->a_nconnect = 0;
	for( c = 0; c < A_CONNECT_SIZE; c++ )
		ap->a_connect[ c ] = UNDEF;
	ap->a_residue = oap[ 0 ].a_residue;
	ap->a_charge = 0.0;
	ap->a_radius = 1.0;
	ap->a_pos[ 0 ] = 0.0;
	ap->a_pos[ 1 ] = 0.0;
	ap->a_pos[ 2 ] = 0.0;
	ap->a_w = 0.0;
	if( (oai = res->r_aindex) ){
		for( a = 0; a < na - 1; a++ )
			nai[ a ] = oai[ a ];
		nai[ na - 1 ] = na - 1;
	}else{
		for( a = 0; a < na; a++ )
			nai[ a ] = a;
	}
	res->r_aindex = nai;

	res->r_natoms++;
	res->r_atoms = nap;

	if( oai )
		free( oai );
	free( oap );
	return(0);
}

#if 0
static	int	ae2xyz( MOLECULE_T *m, char aex[], REAL_T p[] )
{
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;
	int		r, a, na;
	REAL_T		x, y, z;

	select_atoms( m, aex );
	for( x = y = z = 0.0, na = 0, sp = m->m_strands; sp; sp = sp->s_next ){
		if( !( sp->s_attr & AT_SELECT ) )
			continue;
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			if( !( res->r_attr & AT_SELECT ) )
				continue;
			for( a = 0; a < res->r_natoms; a++ ){
				ap = &res->r_atoms[ a ];
				if( ap->a_attr & AT_SELECT ){
					na++;
					x += ap->a_pos[ 0 ];
					y += ap->a_pos[ 1 ];
					z += ap->a_pos[ 2 ];
				}
			}
		}
	}
	if( !na ){
		fprintf( stderr, "%s not in molecule\n", aex );
		return( 1 );
	}
	p[ 0 ] = x / na;
	p[ 1 ] = y / na;
	p[ 2 ] = z / na;
	return( 0 );
}
#endif

int	set_belly_mask( m, aex, frozen )
MOLECULE_T	*m;
char		*aex;
int			*frozen;
{
	int	i, j, k, l, ib, r, a, n, nfrozen, nb, ibig, ismall, ka, la;
	int *iptmp;
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;
	PARMSTRUCT_T *prm;

	select_atoms( m, aex );
	nfrozen = 0;
	for( n = 0, sp = m->m_strands; sp; sp = sp->s_next ){
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			for( a = 0; a < res->r_natoms; a++ ){
				ap = &res->r_atoms[ a ];
				if( ap->a_attr & AT_SELECT ) frozen[n] = 0;
					else {frozen[n] = 1; nfrozen++; }
				n++;
			}
		}
	} 

/*  if a prm object is available, remove all of the internal coordinates
    for frozen-frozen pairs:  */
	prm = m->m_prm;
	if( prm ){

		/*  remove frozen-frozen bonds:  */
		for( nb=0, ib=0; ib<prm->Nbonh; ib++ ){
			i = prm->BondHAt1[ib]/3;
			j = prm->BondHAt2[ib]/3;
			if( !frozen[i] || !frozen[j] ){
				prm->BondHAt1[nb] = prm->BondHAt1[ib];
				prm->BondHAt2[nb] = prm->BondHAt2[ib];
				prm->BondHNum[nb] = prm->BondHNum[ib];
				nb++;
			}
		}
		prm->Nbonh = nb;
		for( nb=0, ib=0; ib<prm->Nbona; ib++ ){
			i = prm->BondAt1[ib]/3;
			j = prm->BondAt2[ib]/3;
			if( !frozen[i] || !frozen[j] ){
				prm->BondAt1[nb] = prm->BondAt1[ib];
				prm->BondAt2[nb] = prm->BondAt2[ib];
				prm->BondNum[nb] = prm->BondNum[ib];
				nb++;
			}
		}
		prm->Nbona = nb;
		prm->Mbona = nb;

		/*  remove frozen-frozen angles:  */
		for( nb=0, ib=0; ib<prm->Ntheth; ib++ ){
			i = prm->AngleHAt1[ib]/3;
			j = prm->AngleHAt2[ib]/3;
			k = prm->AngleHAt3[ib]/3;
			if( !frozen[i] || !frozen[j] || !frozen[k] ){
				prm->AngleHAt1[nb] = prm->AngleHAt1[ib];
				prm->AngleHAt2[nb] = prm->AngleHAt2[ib];
				prm->AngleHAt3[nb] = prm->AngleHAt3[ib];
				prm->AngleHNum[nb] = prm->AngleHNum[ib];
				nb++;
			}
		}
		prm->Ntheth = nb;
		for( nb=0, ib=0; ib<prm->Ntheta; ib++ ){
			i = prm->AngleAt1[ib]/3;
			j = prm->AngleAt2[ib]/3;
			k = prm->AngleAt3[ib]/3;
			if( !frozen[i] || !frozen[j] || !frozen[k] ){
				prm->AngleAt1[nb] = prm->AngleAt1[ib];
				prm->AngleAt2[nb] = prm->AngleAt2[ib];
				prm->AngleAt3[nb] = prm->AngleAt3[ib];
				prm->AngleNum[nb] = prm->AngleNum[ib];
				nb++;
			}
		}
		prm->Ntheta = nb;

		/*  remove frozen dihedrals and 1-4's:  */
#define ABS(x) x > 0 ? x : -x 
		for( i=0; i<prm->Natom; i++ ) prm->N14pairs[i] = 0;
		iptmp = (int *) get(sizeof(int)*12*prm->Natom);
		for( nb=0, ib=0; ib<prm->Nphih; ib++ ){
			i = prm->DihHAt1[ib]/3;
			j = prm->DihHAt2[ib]/3;
			k = prm->DihHAt3[ib]/3;
			l = prm->DihHAt4[ib]/3;
			ka = ABS( k );
			la = ABS( l );
			if( !frozen[i] || !frozen[j] || !frozen[ka] || !frozen[la] ){
				prm->DihHAt1[nb] = prm->DihHAt1[ib];
				prm->DihHAt2[nb] = prm->DihHAt2[ib];
				prm->DihHAt3[nb] = prm->DihHAt3[ib];
				prm->DihHAt4[nb] = prm->DihHAt4[ib];
				prm->DihHNum[nb] = prm->DihHNum[ib];
				nb++;
				if( k >= 0 && l >= 0 ){
					ismall = i < l ? i : l;
					ibig   = i > l ? i : l;
					iptmp[12*ismall + prm->N14pairs[ismall]++] = ibig;
				}
			}
		}
		prm->Nphih = nb;
		for( nb=0, ib=0; ib<prm->Nphia; ib++ ){
			i = prm->DihAt1[ib]/3;
			j = prm->DihAt2[ib]/3;
			k = prm->DihAt3[ib]/3;
			l = prm->DihAt4[ib]/3;
			ka = ABS( k );
			la = ABS( l );
			if( !frozen[i] || !frozen[j] || !frozen[ka] || !frozen[la] ){
				prm->DihAt1[nb] = prm->DihAt1[ib];
				prm->DihAt2[nb] = prm->DihAt2[ib];
				prm->DihAt3[nb] = prm->DihAt3[ib];
				prm->DihAt4[nb] = prm->DihAt4[ib];
				prm->DihNum[nb] = prm->DihNum[ib];
				nb++;
				if( k >= 0 && l >= 0 ){
					ismall = i < l ? i : l;
					ibig   = i > l ? i : l;
					iptmp[12*ismall + prm->N14pairs[ismall]++] = ibig;
				}
			}
		}
		prm->Nphia = nb;
		prm->Mphia = nb;
		j = 0;
		for( i=0; i<prm->Natom - 1; i++ ){
			for( k=0; k<prm->N14pairs[i]; k++ )
				 prm->N14pairlist[j++] = iptmp[12*i + k];
		}
		free( iptmp );
	}
	return( nfrozen );
}

int	set_cons_mask( m, aex, cons )
MOLECULE_T	*m;
char		*aex;
int			*cons;
{
	int	r, a, n, ncons;
	STRAND_T	*sp;
	RESIDUE_T	*res;
	ATOM_T		*ap;

	select_atoms( m, aex );
	ncons = 0;
	for( n = 0, sp = m->m_strands; sp; sp = sp->s_next ){
		for( r = 0; r < sp->s_nresidues; r++ ){
			res = sp->s_residues[ r ];
			for( a = 0; a < res->r_natoms; a++ ){
				ap = &res->r_atoms[ a ];
				if( ap->a_attr & AT_SELECT ) {cons[n] = 1; ncons++; }
					else {cons[n] = 0;}
				n++;
			}
		}
	} 
	return( ncons );
}

#define	PI	3.14159265358979323844
#define	EPS	1e-9

static REAL_T vdot (REAL_T *x1, REAL_T * x2)
{
  return (x1[0]*x2[0] + x1[1]*x2[1] + x1[2]*x2[2]);
}

#if 0
static REAL_T vnorm (REAL_T *x)
{
  return sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2]);
}

#define SQ(X) ((X)*(X))
static void vcross (REAL_T *x, REAL_T *y , REAL_T *res)
{
  res[0] = x[1]*y[2] - x[2]*y[1];
  res[1] = x[2]*y[0] - x[0]*y[2];
  res[2] = x[0]*y[1] - x[1]*y[0];

  return;
}
#endif

INT_T	circle( REAL_T *p1, REAL_T *p2, REAL_T *p3, REAL_T *pc )
{

  /* idea: finding the center of the circumcircle of a triangle turns out  */
  /* to be a "scalar problem": We (arbitrarily) calculate a line bisecting */
  /* the line from p1 to p2. Then we solve a linear equation to find a     */
  /* point on that line that is equidistant to p1 and p3 (equidistance to  */
  /* p1 and p2 is given from the beginning).                               */

  /* returns 0 on success; 1 if the three input points are (nearly) collinear */

  INT_T i;
  REAL_T v[3]; /* intermediate vector in line search direction*/
  REAL_T ab[3]; /* p2 - p1 */
  REAL_T ac[3]; /* p3 - p1 */
  REAL_T bc[3]; /* p3 - p2 */
  REAL_T m_ab[3]; /* middle point between p1 and p2 */
  REAL_T cm[3]; /* vector from m_ab to p3 */
  REAL_T fac1,fac2;

  for (i=0;i<3;i++) ab[i] = p2[i] - p1[i];
  for (i=0;i<3;i++) ac[i] = p3[i] - p1[i];
  for (i=0;i<3;i++) bc[i] = p3[i] - p2[i];
  for (i=0;i<3;i++) m_ab[i] = 0.5*(p1[i] + p2[i]);
  for (i=0;i<3;i++) cm[i] = p3[i] - m_ab[i];

  fac1 = vdot(ab,ac) / vdot(ab,ab);
  
  for (i=0;i<3;i++) v[i] = ac[i] - fac1 * ab[i];
    
  fac2 = vdot(cm,v);
  if (fabs(fac2 ) < EPS)
    {
      fprintf( stderr,"triangle sides too close to collinear !\n");
      return 1;
    }
  fac2 = 0.5 * vdot(bc,ac)/fac2;

  for (i=0;i<3;i++) pc[i] = m_ab[i] + fac2*v[i];
  return 0;
    
}

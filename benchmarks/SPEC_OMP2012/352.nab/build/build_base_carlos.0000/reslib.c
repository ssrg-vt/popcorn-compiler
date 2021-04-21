#if defined(SPEC)
# define DATDIR "."
#endif
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	<math.h>

#include	"nab.h"
#include	"errormsg.h"
#include	"memutil.h"
#include	"database.h"
#include    "chirvol.h"
#include    "traceback.h"

void    upd_molnumbers( MOLECULE_T * );

#define	D2R	0.01745329251994329576

typedef	struct	reslib_t	{
	struct	reslib_t	*rl_next;
	char	*rl_name;
	int	rl_r_kind;
	int	rl_r_atomkind;
	RESIDUE_T	*rl_rlist;
}RESLIB_T;

#define	A_NAME_SIZE	8
#define	R_NAME_SIZE	8

#define	MAXATOMS	1000
static	char	lr_name[ R_NAME_SIZE ];
static	int	n_atoms;
static	ATOM_T	atoms[ MAXATOMS ];

#define	MAXBONDS	1000
static	int	n_bonds;
static	INTBOND_T	bonds[ MAXBONDS ];

#define	MAXCHI		500
static	int	n_chi;
static	CHIRAL_T	chi[ MAXCHI ];

static	RESLIB_T	*reslibs = NULL;

static	char	e_msg[ 256 ];

void	NAB_initatom( ATOM_T *, int );

char	*getreslibkind( char [] );
int	setreslibkind( char [], char [] );
RESIDUE_T	*getresidue( char [], char [] );
PARMSTRUCT_T	*copyparm( PARMSTRUCT_T * );
RESIDUE_T	*copyresidue( RESIDUE_T * );
STRAND_T	*copystrand( STRAND_T * );
MOLECULE_T	*copymolecule( MOLECULE_T * );

static	RESLIB_T	*known_reslib( char [] );
static	RESLIB_T	*read_reslib( char [] );
static	void	initatoms( void );
static	RESLIB_T	*read_reslib_header( char [], FILE **, FILE **, FILE **, FILE **,
	char [] );
static	void	mk_fname( char [], char [] );
static	void	off2reslib( char [], RESLIB_T * );
static	void	pdb2reslib( FILE *, RESLIB_T * );
static	void	bnd2reslib( FILE *, RESLIB_T * );
static	void	chi2reslib( FILE *, RESLIB_T * );
static	void	qr2reslib( FILE *, RESLIB_T * );
static	void	setrlibattrs( RESLIB_T *, char [] );
static	void	addres2reslib( RESLIB_T * );
static	void	addbonds2reslib( RESLIB_T * );
static	void	addqr2reslib( RESLIB_T * );
static	void	addchi2reslib( RESLIB_T * );
static	ATOM_T	*findatom( RESIDUE_T *, char [] );

EXTBOND_T	*copyextbonds();

char	*getreslibkind( char reslib[] )
{
	char	*nhp;
	RESLIB_T	*rlp;

	if( ( rlp = known_reslib( reslib ) ) == NULL ){
		if( ( rlp = read_reslib( reslib ) ) == NULL ){
			fprintf( stderr, "getreslibkind: unknown reslib %s\n",
				reslib );
			exit( 1 );
		}
	}
	if( rlp->rl_r_kind == RT_UNDEF )
		return( "UNDEF" );
	else if( rlp->rl_r_kind == RT_DNA )
		return( "dna" );
	else if( rlp->rl_r_kind == RT_RNA )
		return( "rna" );
	else if( rlp->rl_r_kind == RT_AA )
		return( "aa" );
	else
		return( "UNDEF" );

}

int	setreslibkind( char reslib[], char kind[] )
{
	char	*nhp;
	RESLIB_T	*rlp;

	if( ( rlp = known_reslib( reslib ) ) == NULL ){
			if( ( rlp = read_reslib( reslib ) ) == NULL ){
					fprintf( stderr, "getreslibkind: unknown reslib %s\n",
							reslib );
					exit( 1 );
			}
	}
	rlp->rl_r_kind = RT_UNDEF;
	if ( !strcmp( kind, "dna" ) || !strcmp( kind, "DNA" ) )
        	rlp->rl_r_kind = RT_DNA;
	else if ( !strcmp( kind, "rna" ) || !strcmp( kind, "RNA" ) )
        	rlp->rl_r_kind = RT_RNA;
	else if ( !strcmp( kind, "aa" ) || !strcmp( kind, "AA" ) )
        	rlp->rl_r_kind = RT_AA;

        return( rlp->rl_r_kind );
}

RESIDUE_T	*getresidue( char rname[], char reslib[] )
{
	char	*nhp, leapname[5];
	RESLIB_T	*rlp;
	RESIDUE_T	*res, *nres;

	if( ( rlp = known_reslib( reslib ) ) == NULL ){
		if( ( rlp = read_reslib( reslib ) ) == NULL ){
			exit( 1 );
		}
	}

	strcpy( leapname, rname );
	if ( rlp->rl_r_kind == RT_DNA ){
		if      ( !strncmp( rname, "ADE", 3 ) ) 
			strcpy( leapname, "DA" );
		else if ( !strncmp( rname, "GUA", 3 ) )
			strcpy( leapname, "DG" );
		else if ( !strncmp( rname, "THY", 3 ) )
			strcpy( leapname, "DT" );
		else if ( !strncmp( rname, "CYT", 3 ) )
			strcpy( leapname, "DC" );
	}
	else if ( rlp->rl_r_kind == RT_RNA ){
		if      ( !strncmp( rname, "ADE", 3 ) )
			strcpy( leapname, "RA" );
		else if ( !strncmp( rname, "GUA", 3 ) )
			strcpy( leapname, "RG" );
		else if ( !strncmp( rname, "URA", 3 ) )
			strcpy( leapname, "RU" );
		else if ( !strncmp( rname, "CYT", 3 ) )
			strcpy( leapname, "RC" );
	}
	if ( strstr( rname, "3" ) != NULL )
		strcat( leapname, "3" );
	else if ( strstr( rname, "5" ) != NULL )
		strcat( leapname, "5" );
	for( res = rlp->rl_rlist; res ; res = res->r_next ){
		if(( strcmp( res->r_resname, rname ) == 0 ) ||
			( strcmp( res->r_resname, leapname ) == 0 ))
			break;
	}

	if( res == NULL ){
		sprintf( e_msg, "%s not in library %s", rname, reslib );
		rt_errormsg_s( TRUE, E_NOSUCH_RESIDUE_S, e_msg );
		return( NULL );
	}

	nres = copyresidue( res );
 
	return( nres );
}

RESIDUE_T	*copyresidue( RESIDUE_T *res )
{
	int	a, b, c, i;
	int	*aip;
	RESIDUE_T	*nres;
	ATOM_T		*ap;
	char		*anp, *rnp;
	INTBOND_T	*bp;
	CHIRAL_T	*cp;

	if((nres = (RESIDUE_T *)malloc(sizeof(RESIDUE_T) ))
		== NULL )
	{
		sprintf( e_msg, "new residue %s", res->r_resname );
		rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
		return( NULL );
	}

	if( ( ap = ( ATOM_T * )malloc(res->r_natoms*sizeof(ATOM_T)))
		== NULL )
	{
		sprintf( e_msg, "atoms in new residue %s", res->r_resname );
		rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
		return( NULL );
	}

	if( ( aip = ( int * )malloc(res->r_natoms*sizeof(int))) == NULL )
	{
		sprintf( e_msg, "atom index in new residue %s",
			res->r_resname );
		rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
		return( NULL );
	}

	if( res->r_nintbonds > 0 ){
		if( ( bp = ( INTBOND_T * )
			malloc(res->r_nintbonds*sizeof(INTBOND_T)))
			== NULL )
		{
			sprintf( e_msg,
				"bonds in new residue %s", res->r_resname );
			rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
			return( NULL );
		}
	}else
		bp = NULL;

	if( res->r_nchiral > 0 ){
		if( ( cp = ( CHIRAL_T * )
			malloc(res->r_nchiral*sizeof(CHIRAL_T)) )
			== NULL )
		{
			sprintf( e_msg,
				"chirality in residue %s", res->r_resname );
			rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
			return( NULL );
		}
	}else
		cp = NULL;

	nres->r_next = NULL;

	rnp = ( char * )malloc(strlen(res->r_resname)+1 );
	if( rnp == NULL ){
		fprintf( stderr,
			"copyresidue: can't allocate new r_resname.\n" );
		exit( 1 );
	}
	strcpy( rnp, res->r_resname );
	nres->r_resname = rnp;

	rnp = ( char * )malloc(strlen(res->r_resid)+1 );
	if( rnp == NULL ){
		fprintf( stderr, "copyresidue: can't allocate new r_resid.\n" );
		exit( 1 );
	}
	strcpy( rnp, res->r_resid );
	nres->r_resid = rnp;

	nres->r_num = res->r_num;
	nres->r_tresnum = 0;
	nres->r_resnum = 0;
	nres->r_attr = res->r_attr;
	nres->r_strand = NULL;
	nres->r_extbonds = copyextbonds( res );
	nres->r_nintbonds = res->r_nintbonds;
	nres->r_intbonds = bp;
	for( b = 0; b < res->r_nintbonds; b++ ){
		nres->r_intbonds[ b ][ 0 ] = res->r_intbonds[ b ][ 0 ];
		nres->r_intbonds[ b ][ 1 ] = res->r_intbonds[ b ][ 1 ];
	}
	nres->r_nchiral = res->r_nchiral;
	nres->r_chiral = cp;
	for( cp = nres->r_chiral, c = 0; c < res->r_nchiral; c++, cp++ ){
		for( a = 0; a < 4; a++ )
			cp->c_anum[ a ] = res->r_chiral[ c ].c_anum[ a ];
		cp->c_dist = res->r_chiral[ c ].c_dist;
	}
	nres->r_kind = res->r_kind;
	nres->r_atomkind = res->r_atomkind;
	nres->r_natoms = res->r_natoms;
	nres->r_aindex = aip;
	if( res->r_aindex ){
		for( a = 0; a < res->r_natoms; a++ )
			nres->r_aindex[ a ] = res->r_aindex[ a ];
	}else{
		for( a = 0; a < res->r_natoms; a++ )
			nres->r_aindex[ a ] = a;
	}
	nres->r_atoms = ap;
	for( a = 0; a < res->r_natoms; a++ ){

		anp = (char *)malloc(strlen( res->r_atoms[a].a_atomname )+1);
		if( anp == NULL ){
			fprintf( stderr, "copyresidue: can't allocate anp.\n" );
			exit( 1 );
		}
		strcpy( anp, res->r_atoms[ a ].a_atomname );
		ap[ a ].a_atomname = anp;
		ap[ a ].a_atomtype = NULL;
		ap[ a ].a_attr = res->r_atoms[ a ].a_attr;
		ap[ a ].a_nconnect = res->r_atoms[ a ].a_nconnect;
		for ( i = 0; i < A_CONNECT_SIZE ; i++ )
			ap[a].a_connect[i] = res->r_atoms[a].a_connect[i];
		ap[ a ].a_residue  = nres;
		ap[ a ].a_charge   = res->r_atoms[ a ].a_charge;
		ap[ a ].a_radius   = res->r_atoms[ a ].a_radius;
		ap[ a ].a_bfact    = res->r_atoms[ a ].a_bfact;
		ap[ a ].a_occ      = res->r_atoms[ a ].a_occ;
		ap[ a ].a_element  = NULL;
		ap[ a ].a_int1     = res->r_atoms[ a ].a_int1;
		ap[ a ].a_float1   = res->r_atoms[ a ].a_float1;
		ap[ a ].a_float2   = res->r_atoms[ a ].a_float2;
		ap[ a ].a_tatomnum = res->r_atoms[ a ].a_tatomnum;
		ap[ a ].a_atomnum  = res->r_atoms[ a ].a_atomnum;
		ap[ a ].a_fullname = NULL;
		ap[ a ].a_pos[ 0 ] = res->r_atoms[ a ].a_pos[ 0 ];		
		ap[ a ].a_pos[ 1 ] = res->r_atoms[ a ].a_pos[ 1 ];		
		ap[ a ].a_pos[ 2 ] = res->r_atoms[ a ].a_pos[ 2 ];		
		ap[ a ].a_w        = res->r_atoms[ a ].a_w;
	}

	return( nres );
}

STRAND_T	*copystrand( STRAND_T *str )
{
	STRAND_T	*newstrand;
	char		*namebuf;
	int	resctr;

	if(( newstrand = ( STRAND_T * )malloc( sizeof( STRAND_T ) ) ) == NULL){
		sprintf( e_msg, "new strand %s", str->s_strandname );
		rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
		return( NULL );
	}

        if( ( newstrand->s_residues = ( RESIDUE_T ** )
		malloc( str->s_nresidues * sizeof( RESIDUE_T * ) ) ) == NULL )
	{
                rt_errormsg_s( TRUE, E_NOMEM_FOR_S, "copystrand pointer array");
                return( NULL );
        } 

	if( ( namebuf = ( char * )malloc( ( strlen( str->s_strandname ) + 1 ) 
		* sizeof( char ) ) ) == NULL )
	{
		sprintf( e_msg, "strandname %s", str->s_strandname );
		rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
		return( NULL );
	}  
	newstrand->s_strandname = namebuf;
	strcpy(newstrand->s_strandname, str->s_strandname);
	newstrand->s_strandnum = str->s_strandnum;
	newstrand->s_attr = str->s_attr;
	newstrand->s_molecule = NULL;
	newstrand->s_next = NULL;
	newstrand->s_nresidues = str->s_nresidues;
	newstrand->s_res_size = str->s_nresidues;
        for ( resctr = 0; resctr < str->s_nresidues; resctr++ ) {
                newstrand->s_residues[ resctr ] =
			copyresidue( str->s_residues[ resctr ] );
                newstrand->s_residues[ resctr ]->r_strand = newstrand;
                if ( resctr > 0 )
                        newstrand->s_residues[ resctr - 1 ]->r_next =
				newstrand->s_residues[ resctr ];
        }
	return( newstrand );
}

MOLECULE_T	*copymolecule( MOLECULE_T *mol )
{
        int     row, col;
        STRAND_T        *strptr, *newstr, *nextstr;
        MOLECULE_T      *newmol;

        upd_molnumbers( mol );

        if(( newmol = ( MOLECULE_T * )malloc( sizeof( MOLECULE_T ) ) ) == NULL){
                sprintf( e_msg, "copymolecule" );
                rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
                return( NULL );
        }

        newmol->m_nstrands = mol->m_nstrands;
        newmol->m_nresidues = mol->m_nresidues;
        newmol->m_natoms = mol->m_natoms;
        newmol->m_nvalid = mol->m_nvalid;

        if ( mol->m_prm != NULL ) {
                newmol->m_prm = copyparm( mol->m_prm );
        }
        else
                newmol->m_prm = NULL;

        for ( row = 0; row <= 3; row++ ) {
                for ( col = 0; col <= 2; col++ ) {
                        newmol->m_frame[row][col] = mol->m_frame[row][col];
                }
        }

        strptr = mol->m_strands;
        if ( strptr != NULL ){
                newstr = copystrand( strptr );
                newstr->s_molecule = newmol;
                newmol->m_strands = newstr;
                strptr = strptr->s_next;
        }

        while ( strptr != NULL ) {
                nextstr = copystrand( strptr );
                if ( newstr )
                        newstr->s_next = nextstr;
                nextstr->s_molecule = newmol;
                newstr = nextstr;
                strptr = strptr->s_next;
        }

        upd_molnumbers( newmol );
        return( newmol );
}

static	RESLIB_T	*known_reslib( char reslib[] )
{
	RESLIB_T	*rlp;

	for( rlp = reslibs; rlp; rlp = rlp = rlp->rl_next ){
		if( strcmp( rlp->rl_name, reslib ) == 0 ){
			return( rlp );
		}
	}
	return( NULL ); 
}

static	RESLIB_T	*read_reslib( char reslib[] )
{
	FILE	*pfp;	/* pdb file containing residue coords	*/
	FILE	*bfp;	/* bond file, as i j pairs		*/
	FILE	*qfp;	/* charge & radius file			*/
	FILE	*cfp;	/* chirality file			*/
	RESLIB_T	*rlp;
	char	offname[ 256 ];	/* object file format name (LEaP)	*/

	if( ( rlp = read_reslib_header( reslib, &pfp, &bfp, &qfp, &cfp,
		offname ) ) == NULL )
	{
		return( NULL );
	}

	initatoms();
	if ( *offname ){
		off2reslib( offname, rlp ) ;
	}else{
		pdb2reslib( pfp, rlp );
		bnd2reslib( bfp, rlp );
		fclose( pfp );
		fclose( bfp );
	}

	if( qfp ){
		qr2reslib( qfp, rlp );
		fclose( qfp );
	}

	if( cfp ){
		chi2reslib( cfp, rlp );
		fclose( cfp );
	}

	return( rlp );
}

static	void	initatoms( void )
{
	static	int	init = 1;
	int	i;
	ATOM_T	*ap;	

	if( !init )
		return;
	init = 0;
	for( ap = atoms, i = 0; i < MAXATOMS; i++, ap++ ){
		NAB_initatom( ap, 1 );
		ap->a_atomname = ( char * )malloc(A_NAME_SIZE * sizeof( char ));
		if( ap->a_atomname == NULL ){
			fprintf( stderr,
				"initatoms: can't allocate a_atomname.\n" );
			exit( 1 );
		}
	}
}

static	RESLIB_T	*read_reslib_header( char reslib[],
	FILE **pfp, FILE **bfp, FILE **qfp, FILE **cfp, char offname[] )
{
	FILE	*rfp;
	RESLIB_T	*rlp;
	int	nw, nsize;
	char	*np;
	char	line[ 128 ], keyword[ 20 ], value[ 100 ], newname[ 100 ];
	char	fname[ 256 ];

	if (strstr( reslib, ".lib" ) != NULL ){
		sprintf( newname, "../leap/lib/%s", reslib );
		mk_fname(newname, fname );
		if( ( rfp = fopen( fname, "r" ) ) == NULL ){
			rt_errormsg_s( TRUE, E_CANT_OPEN_RESLIB_S, fname );
			return( NULL );
		}
	}else{
		mk_fname( reslib, fname );
		if( ( rfp = fopen( fname, "r" ) ) == NULL ){
			rt_errormsg_s( TRUE, E_CANT_OPEN_RESLIB_S, fname );
			return( NULL );
		}
	}
	
	if( ( rlp = ( RESLIB_T * )malloc( sizeof( RESLIB_T ) ) ) == NULL ){
		sprintf( e_msg, "new reslib %s", reslib );
		rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
		return( NULL );
	}

	nsize = strlen( reslib ) + 1;
	if( ( np = ( char * )malloc( nsize * sizeof( char ) ) ) == NULL ){
		sprintf( e_msg, "name for new reslib %s", reslib );
		rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
		return( NULL );
	}
	strcpy( np, reslib );
	rlp->rl_next = reslibs;
	reslibs = rlp;
	rlp->rl_name = np;
	rlp->rl_r_kind = RT_UNDEF;
	rlp->rl_r_atomkind = RAT_UNDEF;
	rlp->rl_rlist = NULL;

	*pfp = NULL;
	*bfp = NULL;
	*qfp = NULL;
	*cfp = NULL;
	*offname = '\0';

	if ( strstr( reslib, ".lib" ) == NULL ){
		while( fgets( line, sizeof( line ), rfp ) ){
			if( *line == '#' )
				continue;
			if( ( nw = sscanf( line, "%s %s\n", keyword, value ) )
				== EOF )
			{
				continue;
			}else if( nw != 2 ){
				rlp = NULL;
				goto clean_up;
			}else if( strcmp( keyword, "type" ) == 0 ){
				if( strcmp( value, "dna" ) == 0 )
					rlp->rl_r_kind = RT_DNA;
				else if( strcmp( value, "rna" ) == 0 )
					rlp->rl_r_kind = RT_RNA;
				else if( strcmp( value, "aa" ) == 0 )
					rlp->rl_r_kind = RT_AA;
				else{
					rlp = NULL;
					goto clean_up;
				}
			}else if( strcmp( keyword, "atomtype" ) == 0 ){
				if( strcmp( value, "united" ) == 0 )
					rlp->rl_r_atomkind = RAT_UNITED;
				else if( strcmp( value, "all" ) == 0 )
					rlp->rl_r_atomkind = RAT_ALLATOM;
				else{
					rlp = NULL;
					goto clean_up;
				}
			}else if( strcmp( keyword, "off" ) == 0 ){
				mk_fname( value, offname );
			}else if( strcmp( keyword, "pdb" ) == 0 ){
				mk_fname( value, fname );
				if( ( *pfp = fopen( fname, "r" ) ) == NULL ){
					sprintf( e_msg, "pdb file %s", value );
					rt_errormsg_s( TRUE,
						E_CANT_OPEN_S, e_msg );
					rlp = NULL;
					goto clean_up;
				}
			}else if( strcmp( keyword, "bnd" ) == 0 ){
				mk_fname( value, fname );
				if( ( *bfp = fopen( fname, "r" ) ) == NULL ){
					sprintf( e_msg, "bnd file %s", value );
					rt_errormsg_s( TRUE,
						E_CANT_OPEN_S, e_msg );
					rlp = NULL;
					goto clean_up;
				}
			}else if( strcmp( keyword, "qr" ) == 0 ){
				mk_fname( value, fname );
				if( ( *qfp = fopen( fname, "r" ) ) == NULL ){
					sprintf( e_msg, "qr file %s", value );
					rt_errormsg_s( TRUE,
						E_CANT_OPEN_S, e_msg );
					rlp = NULL;
					goto clean_up;
				}
			}else if( strcmp( keyword, "chi" ) == 0 ){
				mk_fname( value, fname );
				if( ( *cfp = fopen( fname, "r" ) ) == NULL ){
					sprintf( e_msg, "chi file %s", value );
					rt_errormsg_s( TRUE,
						E_CANT_OPEN_S, e_msg );
					rlp = NULL;
					goto clean_up;
				}
			}else{
				rlp = NULL;
				goto clean_up;
			}
		}
	}else{
		mk_fname( newname, offname );
	}

clean_up : ;
	fclose( rfp );
	if( rlp == NULL ){
		if( *pfp ) fclose( *pfp );
		if( *bfp ) fclose( *bfp );
		if( *qfp ) fclose( *qfp );
		if( *cfp ) fclose( *cfp );
		nsize = strlen( line ) - 1;	/* ignore \n	*/
		nsize = nsize > 20 ? 20 : nsize;
		sprintf( e_msg, "%-*.*s", nsize, nsize, line );
		rt_errormsg_s( TRUE, E_BAD_RESLIB_HEADER_S, e_msg );
	}

	return( rlp );
}

static	void	mk_fname( char sname[], char fname[] )
{

	if( *sname == '/' ||
		!strncmp( "./", sname, 2 ) )
		strcpy( fname, sname );
	else
		sprintf( fname, "%s/reslib/%s", DATDIR, sname );
}

static	void	off2reslib( char offname[], RESLIB_T *rlp )
{
	ATOM_T	*ap;
	int	  n_names, n_resnames, i, ires;
	DATABASE db;
	Bool	bresult;
	int	typex[ 100 ], resx[ 100 ], flags[ 100 ],
		seq[ 100 ], elmnt[ 100 ];
	int	atom1x[ 100 ], atom2x[ 100 ];
	REAL_T	chg[ 100 ], x[ 100 ], y[ 100 ], z[ 100 ];
	char	a_name[ 100 ][ 10 ], type[ 100 ][ 10 ], res_name[ 100 ][ 10 ];
	char	prefix[ 255 ];

	n_atoms = 0;
	db = dbDBRndOpen( offname, OPENREADONLY );

/*  find residue names in the database; still needs error checking  */

	bresult = bDBGetValue( db, "!index", &n_resnames, res_name, 10 );

	for ( ires=0; ires < n_resnames; ires++ ){

/*         get resiude name:   */

		sprintf( prefix, "entry.%s.", res_name[ ires ] );
		DBZeroPrefix( db );
		DBPushPrefix( db, prefix );
		bresult = bDBGetValue( db, "unit.name", &n_names, lr_name, 1 );

/*   get atom names, charges, etc.:   */

		bresult = bDBGetTable( db, "unit.atoms", &n_atoms,
			3, typex, sizeof(int),
			4, resx, sizeof(int),
			5, flags, sizeof(int),
			6, seq, sizeof(int),
			7, elmnt, sizeof(int),
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			8, chg, sizeof(REAL_T),
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			1, a_name, 10,
			2, type, 10,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0 );

/*   get coordinates:    */

		bresult = bDBGetTable( db, "unit.positions", &n_atoms,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			1, x, sizeof(REAL_T),
			2, y, sizeof(REAL_T),
			3, z, sizeof(REAL_T),
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0 );

		for ( i=0; i < n_atoms; i++ ){
			ap = &atoms[ i ];
			NAB_initatom( ap, 0 );
			strcpy( ap->a_atomname, a_name[ i ] );
			ap->a_charge   = chg[ i ];
			ap->a_pos[ 0 ] = x[ i ];
			ap->a_pos[ 1 ] = y[ i ];
			ap->a_pos[ 2 ] = z[ i ];
		}  

		if( n_atoms > 0 )
			addres2reslib( rlp );

/*    get connectivity information:   */

		bresult = bDBGetTable( db, "unit.connectivity", &n_bonds,
			1, atom1x, sizeof(int),
			2, atom2x, sizeof(int),
			3, flags, sizeof(int),
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0,
			0, NULL, 0 );

		for ( i=0; i < n_bonds; i++ ){
			bonds[ i ][ 0 ] = atom1x[ i ];
			bonds[ i ][ 1 ] = atom2x[ i ];
		}  

    	if( n_bonds > 0 )
        	addbonds2reslib( rlp );
	}
}

static	void	pdb2reslib( FILE *rfp, RESLIB_T *rlp )
{
	char	line[ 256 ];
	char	ltype[ 10 ], a_name[ 10 ], r_name[ 10 ];
	int	a_num, r_num;
	REAL_T	x, y, z;
	ATOM_T	*ap;

	n_atoms = 0;
	*lr_name = '\0';
	while( fgets( line, sizeof( line ), rfp ) ){
		if( strncmp( "REMARK", line, 6 ) == 0 ){
			setrlibattrs( rlp, line );
		}else if( strncmp( "ATOM", line, 4 ) == 0 ||
			strncmp( "HETATM", line, 6 ) == 0 ){
#ifdef NAB_DOUBLE_PRECISION
			sscanf( line, "%s %d %s %s %d %lf %lf %lf",
#else
			sscanf( line, "%s %d %s %s %d %f %f %f",
#endif
				ltype,
				&a_num, a_name, r_name, &r_num,
				&x, &y, &z );
			if( strcmp( lr_name, r_name ) != 0 ){
				if( n_atoms > 0 ){
					addres2reslib( rlp );
					n_atoms = 0;
				}
				strcpy( lr_name, r_name );
			}
			ap = &atoms[ n_atoms ];
			NAB_initatom( ap, 0 );
			strcpy( ap->a_atomname, a_name );
			ap->a_pos[ 0 ] = x;
			ap->a_pos[ 1 ] = y;
			ap->a_pos[ 2 ] = z;
			n_atoms++;
		}
	}
	if( n_atoms > 0 )
		addres2reslib( rlp );
}

static	void	bnd2reslib( FILE *bfp, RESLIB_T *rlp )
{
	char	line[ 256 ];
	char	r_name[ 20 ];
	int	llen;
	int	bi, bj;

	n_bonds = 0;
	*r_name = '\0';
	while( fgets( line, sizeof( line ), bfp ) ){
		if( *line == '#' ){
			if( sscanf( line, "# %s", r_name ) != 1 ){
				llen = strlen( line ) - 1;
				llen = llen > 20 ? 20 : llen;
				sprintf( e_msg, "%-*.*s", llen, llen ,line );
				rt_errormsg_s( TRUE, E_BAD_BNDFILE_HEADER_S,
					e_msg );
				return;
			}else if( n_bonds > 0 ){
				addbonds2reslib( rlp );
				strcpy( lr_name, r_name );
				n_bonds = 0;
			}else
				strcpy( lr_name, r_name );
		}else{
			if( sscanf( line, "%d %d", &bi, &bj ) != 2 ){
				llen = strlen( line ) - 1;
				llen = llen > 20 ? 20 : llen;
				sprintf( e_msg, "%-*.*s", llen, llen ,line );
				rt_errormsg_s( TRUE, E_BAD_BNDFILE_DATA_S,
					e_msg );
				return;
			}else{
				bonds[ n_bonds ][ 0 ] = bi;
				bonds[ n_bonds ][ 1 ] = bj;
				n_bonds++;
			}
		}
	}
	if( n_bonds > 0 )
		addbonds2reslib( rlp );
}

static	void	chi2reslib( FILE *cfp, RESLIB_T *rlp )
{
	char	line[ 256 ];
	char	r_name[ 20 ];
	int	llen;
	RESIDUE_T	*res, *res1;
	char	anm1[ 8 ], anm2[ 8 ], anm3[ 8 ], anm4[ 8 ];
	int	a1, a2, a3, a4;
	ATOM_T	*ap;
	CHIRAL_T	*cp;

	n_chi = 0;
	*r_name = '\0';
	while( fgets( line, sizeof( line ), cfp ) ){
		if( *line == '#' ){
			if( sscanf( line, "# %s", r_name ) != 1 ){
				llen = strlen( line ) - 1;
				llen = llen > 20 ? 20 : llen;
				sprintf( e_msg, "%-*.*s", llen, llen ,line );
				fprintf( stderr,
				"chi2reslib: bad line in chirality file: %s\n",
					e_msg );
				return;
			}else{
				if( n_chi > 0 ){
					addchi2reslib( rlp );
					n_chi = 0;
				}
				res1 = NULL;
				for( res = rlp->rl_rlist; res;
					res = res->r_next ){
					if( !strcmp( res->r_resname, r_name ) ){
						res1 = res;
						break;
					}
				}
				if( !res1 ){
					fprintf( stderr,
					"chi2reslib: res %s not in %s\n",
						r_name, rlp->rl_name );
					return;
				}
				strcpy( lr_name, r_name );
			}
		}else{
			sscanf( line, "%s %s %s %s", anm1, anm2, anm3, anm4 );
			if( !( ap = findatom( res, anm1 ) ) ){
				fprintf( stderr,
			"chi2reslib: atom \"%s\" not in residue \"%s\"\n",
					anm1, res->r_resname );
				exit( 1 );
			}else
				a1 = ap - res->r_atoms;
			if( !( ap = findatom( res, anm2 ) ) ){
				fprintf( stderr,
			"chi2reslib: atom \"%s\" not in residue \"%s\"\n",
					anm2, res->r_resname );
				exit( 1 );
			}else
				a2 = ap - res->r_atoms;
			if( !( ap = findatom( res, anm3 ) ) ){
				fprintf( stderr,
			"chi2reslib: atom \"%s\" not in residue \"%s\"\n",
					anm3, res->r_resname );
				exit( 1 );
			}else
				a3 = ap - res->r_atoms;
			if( !( ap = findatom( res, anm4 ) ) ){
				fprintf( stderr,
			"chi2reslib: atom \"%s\" not in residue \"%s\"\n",
					anm4, res->r_resname );
				exit( 1 );
			}else
				a4 = ap - res->r_atoms;
			cp = &chi[ n_chi ];
			n_chi++;
			cp->c_anum[ 0 ] = a1;
			cp->c_anum[ 1 ] = a2;
			cp->c_anum[ 2 ] = a3;
			cp->c_anum[ 3 ] = a4;
			cp->c_dist = 0.0;
		}
	}
	if( n_chi > 0 ){
		addchi2reslib( rlp );
		n_chi = 0;
	}
}

static	void	qr2reslib( FILE *qfp, RESLIB_T *rlp )
{
	char	line[ 256 ];
	char	ltype[ 10 ], a_name[ 10 ], r_name[ 10 ];
	int	a_num, r_num;
	REAL_T	q, r;
	ATOM_T	*ap;

	n_atoms = 0;
	*lr_name = '\0';
	while( fgets( line, sizeof( line ), qfp ) ){
		if( strncmp( "REM", line, 3 ) == 0 ){
			continue;
		}else if( strncmp( "ATOM", line, 4 ) == 0 ||
			strncmp( "HETATM", line, 6 ) == 0 ){
#ifdef NAB_DOUBLE_PRECISION
			sscanf( line, "%s %d %s %s %d %lf %lf",
#else
			sscanf( line, "%s %d %s %s %d %f %f",
#endif
				ltype,
				&a_num, a_name, r_name, &r_num, &q, &r );
			if( strcmp( lr_name, r_name ) != 0 ){
				if( n_atoms > 0 ){
					addqr2reslib( rlp );
					n_atoms = 0;
				}
				strcpy( lr_name, r_name );
			}
			ap = &atoms[ n_atoms ];
			NAB_initatom( ap, 0 );
			strcpy( ap->a_atomname, a_name );
			ap->a_charge = q;
			ap->a_radius = r;
			n_atoms++;
		}
	}
	if( n_atoms > 0 )
		addqr2reslib( rlp );
}

static	void	setrlibattrs( RESLIB_T *rlp, char line[] )
{
	char	rkind[ 100 ], rakind[ 100 ];

	if( sscanf( line, "REMARK RESLIB %s %s", rkind, rakind ) != 2 )
		return;
	if( strcmp( rkind, "DNA" ) == 0 )
		rlp->rl_r_kind = RT_DNA;
	else if( strcmp( rkind, "RNA" ) == 0 )
		rlp->rl_r_kind = RT_RNA;
	else if( strcmp( rkind, "AA" ) == 0 )
		rlp->rl_r_kind = RT_AA;
	if( strcmp( rakind, "UNITED" ) == 0 )
		rlp->rl_r_atomkind = RAT_UNITED;
	else if( strcmp( rakind, "ALLATOM" ) == 0 )
		rlp->rl_r_atomkind = RAT_ALLATOM;
}

static	void	addres2reslib( RESLIB_T *rlp )
{
	RESIDUE_T	*res;
	ATOM_T		*ap;
	char		*anp, *rnp;
	int	a, i;

	if( ( res = ( RESIDUE_T * )malloc( sizeof( RESIDUE_T ) ) ) == NULL ){
		sprintf( e_msg, "residue %s in reslib %s",
			lr_name, rlp->rl_name );
		rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
		return;
	}
	if( ( ap = ( ATOM_T * )malloc( n_atoms * sizeof( ATOM_T ) ) ) == NULL ){
		sprintf( e_msg, "atoms in residue %s in reslib %s",
			lr_name, rlp->rl_name );
		rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
		return;
	}
	res->r_next = rlp->rl_rlist;
	rlp->rl_rlist = res;

	rnp = ( char * )malloc( strlen( lr_name ) + 1 );
	if( rnp == NULL ){
		fprintf( stderr,
			"addres2reslib: can't allocate new r_resname.\n" );
		exit( 1 );
	}
	strcpy( rnp, lr_name );
	res->r_resname = rnp;

	rnp = ( char * )malloc( strlen( "" ) + 1 );
	if( rnp == NULL ){
		fprintf( stderr,
			"addres2reslib: can't allocate new r_resid.\n" );
		exit( 1 );
	}
	strcpy( rnp, "" );
	res->r_resid = rnp;

	res->r_num = 0;
	res->r_tresnum = 0;
	res->r_resnum = 0;
	res->r_attr = 0;
	res->r_kind = rlp->rl_r_kind;
	res->r_atomkind = rlp->rl_r_atomkind;
	res->r_strand = NULL;
	res->r_extbonds = NULL;
	res->r_nintbonds = 0;
	res->r_intbonds = NULL;
	res->r_nchiral = 0;
	res->r_chiral = NULL;
	res->r_natoms = n_atoms;
	res->r_aindex = NULL;
	res->r_atoms = ap;
	for( a = 0; a < n_atoms; a++ ){
		anp = ( char * )malloc( strlen( atoms[ a ].a_atomname ) + 1 );
		if( anp == NULL ){
			fprintf( stderr,
				"addres2reslib: can't allocate anp.\n" );
			exit( 1 );
		}
		/* FIX ME! */
		strcpy( anp, atoms[ a ].a_atomname );
		res->r_atoms[ a ].a_atomname = anp;
		res->r_atoms[ a ].a_attr     = 0;
		res->r_atoms[ a ].a_nconnect = 0;
		for( i = 0; i < A_CONNECT_SIZE; i ++ )
			res->r_atoms[ a ].a_connect[ i ] = UNDEF;
		res->r_atoms[ a ].a_residue  = res;
		res->r_atoms[ a ].a_charge   = atoms[ a ].a_charge;
		res->r_atoms[ a ].a_radius   = atoms[ a ].a_radius;
		res->r_atoms[ a ].a_bfact    = atoms[ a ].a_bfact;
		res->r_atoms[ a ].a_occ      = atoms[ a ].a_occ;
		res->r_atoms[ a ].a_int1     = atoms[ a ].a_int1;
		res->r_atoms[ a ].a_float1   = atoms[ a ].a_float1;
		res->r_atoms[ a ].a_float2   = atoms[ a ].a_float2;
		res->r_atoms[ a ].a_atomnum  = 0;
		res->r_atoms[ a ].a_fullname = NULL;
		for( i = 0; i < 3; i ++ )
			res->r_atoms[ a ].a_pos[ i ] = atoms[ a ].a_pos[ i ];
		res->r_atoms[ a ].a_w        = atoms[ a ].a_w;
	}
}

static	void	addbonds2reslib( RESLIB_T *rlp )
{
	RESIDUE_T	*res;
	INTBOND_T	*bp;
	int	b;
	int	a, ai, aj;
	ATOM_T	*api, *apj;

	for( res = rlp->rl_rlist; res; res = res->r_next ){
		if( strcmp( res->r_resname, lr_name ) == 0 ){
			if( ( bp = ( INTBOND_T * )
				malloc( n_bonds * sizeof( INTBOND_T ) ) )
				== NULL ){
				sprintf( e_msg,
					"bonds of residue %s", lr_name );
				rt_errormsg_s( TRUE, E_NOMEM_FOR_S, e_msg );
				return;
			}
			for( b = 0; b < n_bonds; b++ ){
				ai = bp[ b ][ 0 ] = bonds[ b ][ 0 ];
				aj = bp[ b ][ 1 ] = bonds[ b ][ 1 ];
				ai--;
				aj--;
				api = &res->r_atoms[ ai ];
				for( a = 0; a < A_CONNECT_SIZE; a++ ){
					if( api->a_connect[ a ] == aj )
						break; 
					else if( api->a_connect[ a ] == UNDEF ){
						api->a_connect[ a ] = aj;
						api->a_nconnect++;
						break; 
					}
				}
				apj = &res->r_atoms[ aj ];
				for( a = 0; a < A_CONNECT_SIZE; a++ ){
					if( apj->a_connect[ a ] == ai )
						break; 
					else if( apj->a_connect[ a ] == UNDEF ){
						apj->a_connect[ a ] = ai;
						apj->a_nconnect++;
						break; 
					}
				}
			}
			res->r_nintbonds = n_bonds;
			res->r_intbonds = bp;
			return;
		}
	}
	sprintf( e_msg, "%s not in reslib %s\n", lr_name, rlp->rl_name );
	rt_errormsg_s( TRUE, E_NOSUCH_RESIDUE_S, e_msg );
	return;
}

static	void	addqr2reslib( RESLIB_T *rlp )
{
	RESIDUE_T	*res;
	int		a;
	ATOM_T		*ap, *apr;

	for( res = rlp->rl_rlist; res; res = res->r_next ){
		if( strcmp( res->r_resname, lr_name ) == 0 ){
			for( a = 0, ap = atoms; a < n_atoms; a++, ap++ ){
				if( (apr = findatom( res, ap->a_atomname )) ){
					/* FIX ME */
					apr->a_charge = ap->a_charge;
					apr->a_radius = ap->a_radius;
					apr->a_bfact  = ap->a_bfact;
					apr->a_occ    = ap->a_occ;
					apr->a_int1   = ap->a_int1;
					apr->a_float1 = ap->a_float1;
					apr->a_float2 = ap->a_float2;
				}
			}
			return;
		}
	}
	sprintf( e_msg, "%s not in reslib %s\n", lr_name, rlp->rl_name );
	rt_errormsg_s( TRUE, E_NOSUCH_RESIDUE_S, e_msg );
	return;
}

static	void	addchi2reslib( RESLIB_T *rlp )
{
	RESIDUE_T	*res;
	CHIRAL_T	*cp;
	int		a, c, ca;
	POINT_T		pos[ 4 ];
	POINT_T		dvol[ 4 ];
	REAL_T		vol;

	for( res = rlp->rl_rlist; res; res = res->r_next ){
		if( !strcmp( res->r_resname, lr_name ) ){
			if( ( cp = ( CHIRAL_T * )
				malloc( n_chi * sizeof( CHIRAL_T ) ) )
				== NULL ){
				fprintf( stderr,
				"addchi2reslib: can't alloc r_chiral for %s\n",
					res->r_resname );
				return;
			}
			res->r_nchiral = n_chi;
			res->r_chiral = cp;
			for( c = 0; c < n_chi; c++ ){
				for( a = 0; a < 4; a++ ){
					ca = cp->c_anum[a] = chi[c].c_anum[a]; 
					pos[a][0] = res->r_atoms[ca].a_pos[0];
					pos[a][1] = res->r_atoms[ca].a_pos[1];
					pos[a][2] = res->r_atoms[ca].a_pos[2];
				}
				chirvol( 3, 0, 1, 2, 3, (REAL_T*) pos, (REAL_T*) dvol, &vol );
				cp->c_dist = vol;
				cp++;
			}
			return;
		}
	}
	fprintf( stderr, "addchi2reslib: res %s not reslib %s\n",
		res->r_resname, rlp->rl_name );
}

static	ATOM_T	*findatom( RESIDUE_T *res, char aname[] )
{
	int	a;

	for( a = 0; a < res->r_natoms; a++ ){
		if( !strcmp( res->r_atoms[ a ].a_atomname, aname ) )
			return( &res->r_atoms[ a ] ); 
	}
	return( NULL );
}

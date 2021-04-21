#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "nabcode.h"

/* Storage allocation/deallocation functions. */

REAL_T *vector( size_t, size_t);
void free_vector( REAL_T*, size_t, size_t );

/* External variables that may be set directly instead of via mm_options(). */

extern INT_T gb, blocksize, ntpr, ntpr_md;

/* Local variables. */

static MOLECULE_T *m;
static INT_T ier, mytaskid, numtasks, zero=0;
static REAL_T *m_xyz,  *f_xyz,  *v_xyz;
static REAL_T dgrad, fret;
static POINT_T dummy;
static char filename[256];

/* Here are some variables that in the full NAB are defined in nab2c.c */

int cg_emsg_lineno = 1;
char cg_nfname[256] = "";

/* Here is a function that in the full NAB is defined in cgen.c */

int CG_exit( int i ) {
  exit (1);
}

FILE *nabout; /* gets force-field output */

int main( int argc, char *argv[] )
{
#if defined(SPEC)
  int seed = 0;
#endif
  nabout = stdout; /*default*/

  /* Always call mpiinit(), which does nothing if MPI isn't used. */

  mpiinit( &argc, argv, &mytaskid, &numtasks);

  /* Check for the correct number of command-line arguments. */

  if( argc != 3 ){
    if( mytaskid == 0 ){
      printf( "Usage: %s <directory> <PRNG seed>\n", argv[0] );
      fflush( stdout );
    }
    ier =  - 1;
  } else {
    ier = 0;
  }
  if( ( mpierror( ier ) ) != 0 ){
    if( mytaskid == 0 ){
      printf( "Error in mpierror!\n" );
      fflush( stdout );
    }
    exit( 1 );
  }

#if defined(SPEC)
  /* Get the random seed and use it */
  seed = atoi( argv[2] );
  setseed( &seed );
#endif

  /* Echo the command line. */

  if( mytaskid == 0 ){
#if defined(SPEC)
    printf( "nabmd %s %d\n\n", argv[1], seed );
#else
    printf( "%s %s\n\n", argv[0], argv[1] );
#endif
  }

  /* Get the molecular topology from the .pdb file. */

  filename[0] = '\0';
  strcat(filename, argv[1]);
  strcat(filename, "/");
  strcat(filename, argv[1]);
  strcat(filename, ".pdb");
  if( mytaskid == 0) printf( "Reading .pdb file (%s)\n", filename);
  m = getpdb( filename, NULL );

  /* Get the molecular force field from the .prm file. */

  filename[0] = '\0';
  strcat(filename, argv[1]);
  strcat(filename, "/");
  strcat(filename, argv[1]);
  strcat(filename, ".prm");
  readparm( m, filename );

  /* Allocate arrays for coordinates, gradients and velocities. */

  m_xyz = vector( 0, 3*(m->m_prm->Natom) );
  f_xyz = vector( 0, 3*(m->m_prm->Natom) );
  v_xyz = vector( 0, 3*(m->m_prm->Natom) );

  /* Get the molecular geometry from either the .pdb file. */

  setxyz_from_mol(  &m, NULL, m_xyz );

  /* Specify Generalized Born solvation energy. */

  gb = 1;

  /* Initialize MME and get the initial energy. */

  mme_init( m, NULL, "::ZZZZ", dummy, NULL );
  fret = mme( m_xyz, f_xyz, &zero );
  if( mytaskid == 0 ) printf( "Initial energy is %f0\n", fret );

  /* Perform 1000 steps of molecular dynamics. */

  if( mytaskid == 0 )
    printf( "Starting molecular dynamics with Born solvation energy...\n\n" );
  ier = md( 3*(m->m_prm->Natom), 1000, m_xyz, f_xyz, v_xyz, mme );
  if( mytaskid == 0 ) printf( "\n...Done, md returns %d\n", ier );

#if !defined(SPEC)
  /* Print out the execution times. */

  mme_timer(  );
#endif

  /* Specify no solvation energy. */

  gb = 0;

  /* Initialize MME and get the initial energy. */

  mme_init( m, NULL, "::ZZZZ", dummy, NULL );
  fret = mme( m_xyz, f_xyz, &zero );
  if( mytaskid == 0 ) printf( "Initial energy is %f0\n", fret );

  /* Perform 1000 steps of molecular dynamics. */

  if( mytaskid == 0 )
    printf( "Starting molecular dynamics with in vaccuo non-bonded energy...\n\n" );
  ier = md( 3*(m->m_prm->Natom), 1000, m_xyz, f_xyz, v_xyz, mme );
  if( mytaskid == 0 ) printf( "\n...Done, md returns %d\n", ier );

#if !defined(SPEC)
  /* Print out the execution times. */

  mme_timer(  );
#endif

  /* Free the coordinate, gradient and velocity arrays. */

  free_vector( m_xyz, 0, 3*(m->m_prm->Natom) );
  free_vector( f_xyz, 0, 3*(m->m_prm->Natom) );
  free_vector( v_xyz, 0, 3*(m->m_prm->Natom) );

  /* Always call mpifinialize, even for OpenMP execution. */

  mpifinalize();

  return( 0 );
}

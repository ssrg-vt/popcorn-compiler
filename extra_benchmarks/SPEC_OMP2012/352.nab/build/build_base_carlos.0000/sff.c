/*
 * sff.c: simple force field: implement routines to read a "prmtop"
 * file from AMBER, and calculate the energy.  Implements only
 * some of AMBER's functionality: bonds, angles, dihedrals, and
 * nonbonded interactions with a distance-dependent dielectric.
 *
 * Does not (yet) include support for period systems.
 *
 * Main interface is through routines "mme", "mme4", "mme_init" and "md".
 *
 * Modifications to mme_init, mme, mme4 and md were added by
 * Russ Brown (russ.brown@sun.com).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <ctype.h>
#if !defined(SPEC)
#include <time.h>
#endif /* !SPEC */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
#include <omp.h>
#endif

#if !defined(SPEC)
#if defined(OPENMP) || defined(MPI) || defined(SCALAPACK)
#include <sys/time.h>
#endif
#endif /* !SPEC */

#include "nab.h"
#include "memutil.h"
#include "debug.h"
#include "errormsg.h"
#include "traceback.h"

#if defined(MPI) || defined(SCALAPACK)
#include "mpi.h"
#endif

INT_T nblist(INT_T * lpears, INT_T * upears, INT_T ** pearlist, REAL_T * x,
             INT_T context_PxQ, INT_T derivs, REAL_T cutoff, int natom,
             int dim, int *frozen);

char *ggets(char *, int, FILE *);


/* Here are offsets into the ScaLAPACK descriptor array. */

#define DTYPE_ (0)
#define CTXT_ (1)
#define M_ (2)
#define N_ (3)
#define MB_ (4)
#define NB_ (5)
#define RSRC_ (6)
#define CSRC_ (7)
#define LLD_ (8)
#define DLEN_ (9)


/* Here are global timer variables shared with newton.c, conjgrad.c and nmode.c. */

REAL_T tnewton, tlevel, tcholesky, tdiag, tdiagd, tdgeev;
REAL_T tmmetwo, tnmode, tconjgrad, tmmeone;

/*
 * The following are MPI variables that will be set by mpiinit.
 * They are initialized below at compile time to default values
 * that are correct for single-threaded execution so that it is
 * not necessary to call mpiinit unless MPI or ScaLAPACK is used.
 */

static int mytaskid = 0, numtasks = 1;

/* Here are the static timer variables. */

static REAL_T tcons, tnonb, tpair, tbond, tangl, tphi, tborn, tmme, tmd;
static REAL_T tcons2, tnonb2, tbond2, tangl2, tphi2, tborn2, tmme2;
static REAL_T tb2g1, tb2g2, tb2g3, tloop, treduce;

/* Some of the following variables are not static so that they may be set in main(). */

static int dim = 3;             /* dimension: 3 or 4 */
static char *gopts;             /* points to the mm_options string */
static REAL_T cut = 20.0;       /* non-bonded cutoff */
static REAL_T cutnp = 10.0;     /* non-bonded cutoff for GBSA nonpolar */
static char *chknm = NULL;      /* checkpoint file name */
int ntpr = 100;                 /* print frequency */
static int nsnb = 10;           /* non-bonded update frequency */
static int nsnp = 10;           /* non-polar update frequency */
static int nchk = 10000;        /* checkpoint update frequency for mme34 */
static int nchk2 = 10000;       /* checkpoint update frequency for mme2 */
static REAL_T scnb = 2.0;       /* scale factor for 1-4 nonbonds */
static REAL_T scee = 1.2;       /* scale factor for 1-4 electrostatics  */

static REAL_T rgbmax = 20.0;    /*  maximum dist. for gb radius calculation */
static REAL_T gbalpha, gbbeta, gbgamma;

static PARMSTRUCT_T *prm;

static int *frozen = NULL;      /* frozen atoms */
static int nfrozen = 0;         /* number of frozen atoms */

static int *constrained = NULL;
static int nconstrained = 0;
static REAL_T *x0;              /*  will hold coordinates to be constrained  */
static REAL_T wcons = 0.0;      /*  weight of constraints    */

/* Here are the standard pair list and N14 pair list. */

static int *upairs = NULL;      /* array of the number of pairs in upper triangle */
static int *lpairs = NULL;      /* array of the number of pairs in lower triangle */
static int **pairlist = NULL;   /* array of pair list arrays */
static int nb_pairs = -1;       /* the total number of pairs */
static int **N14pearlist = NULL;        /* array of N14 pair list arrays */

/* Here is the standard pair list for egb2 when used with SCALAPACK or MPI. */

#if defined(SCALAPACK) || defined(MPI)
static int *upairs2 = NULL;     /* upairs array for the second derivatives */
static int *lpairs2 = NULL;     /* lpairs array for the second derivatives */
static int **pairlist2 = NULL;  /* pairlist array for the second derivatives */
static int nb_pairs2 = -1;      /* the total number of pairs for pairlist2 */
#endif

/* Here is the pair list to be used for non-polar egb calculations. */

static int *upairsnp = NULL;    /* array of the number of pairs in upper triangle */
static int *lpairsnp = NULL;    /* array of the number of pairs in lower triangle */
static int **pairlistnp = NULL; /* array of pair list arrays */
static int np_pairs = -1;       /* the total number of pairs */

/* Here is the pair list to be used for non-polar egb2 calculations. */

static int *upairs2np = NULL;   /* array of the number of pairs in upper triangle */
static int *lpairs2np = NULL;   /* array of the number of pairs in lower triangle */
static int **pairlist2np = NULL;        /* array of pair list arrays */
static int np_pairs2 = -1;      /* the total number of pairs */

static int **IexclAt = NULL;    /* array of excluded atom arrays for atom i */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP)) || defined(SCALAPACK)
static int **JexclAt = NULL;    /* array of excluded atom arrays for atom j */
static int *Jblo;               /* number of excluded atoms for atom j */
#endif

int gb = 0;              /* non-zero if generalized Born is to be run */
static int gbsa = 0;            /* non-zero if GB surface area is to be run */
static int gb_debug = 0;        /* non-zero if extra printout for GB 1st deriv.  */
static int nr_debug = 0;        /* non-zero if extra printout for Newt. Raph. */
static int gb2_debug = 0;       /* non-zero if extra printout for GB 2nd deriv.  */
static int gbsa_debug = 0;      /* non-zero if extra printout for GBSA  */
static int e_debug = 0;         /* non-zero if extra printout for energies   */

/*LCPO stuff */

    /* augmented VdW radii, parameters (see JCC 20,2,217 ff.) */
static REAL_T *P0 = NULL, *P1 = NULL, *P2 = NULL, *P3 = NULL, *P4 = NULL;
static int *ineighbor;          /* neighborlist-array */

static REAL_T surften = 0.005;  /* 'surface tension' for GBSA=1 */
static REAL_T dradius = 0.5;    /* per-atom radius augmentation for GBSA */
static REAL_T deltar = 0.5;     /* interatomic radius delta for GBSA */
static REAL_T rwater = 1.4;     /* radius of water for GB nonpolar vdW */
static REAL_T alphanp = 0.75;   /* average alpha_sub_i for GB nonpolar vdW */
static REAL_T kappanp = 2.227;  /* sphere Gaussian diffuseness for GBSA */
static REAL_T min_volume = 0.01;        /* volume limit for GBSA "depth-first" */
int blocksize = 8;              /* block size for parallel loops */
static int max_set_size = 20;   /* maximum spheres per set for GBSA */
static int dynamic_loops = 1;   /* schedule GBSA loop dynamically for MPI */
static int MPI_min_tasks = 8;   /* minimum required tasks for dynamic loops */
static int cull_np_lists = 1;   /* cull nonpolar pair lists for GBSA */
static int use_lower_tri = 0;   /* use lower and upper triangle pair lists */
static int static_arrays = 1;   /* allocate large arrays once */

static REAL_T epsext = 78.5;    /* exterior dielectric constant for GB */
static REAL_T kappa = 0.0;      /* Debye-Huckel screeing parameter for GB */

static int dield = 1;           /* dielectric function to be used */
static REAL_T dielc = 1.0;      /* dielectric constant for non-GB sim */
static REAL_T k4d = 0.0;

static REAL_T t = 0.;           /* initial time */
static REAL_T dt = 0.001;       /* time step, ps. */
static REAL_T tautp = 0.4;      /* temp. coupling parm., ps */
static REAL_T gamma_ln = 0.0;   /* Langevin collison parameter, in ps^-1  */
static REAL_T temp0 = 100.;     /* target temperature, K */
static REAL_T boltz2 = 9.93595e-4;      /* one-half the Boltzmann constant */
static REAL_T vlimit = 10.0;    /* maximum velocity component */
static REAL_T genmass = 10.;    /* general masses, for now all the same */
int ntpr_md = 100;              /* print frequency for KE,PE,temp */
static int ntwx = 0;            /* trajectory snapshot frequency  */
static FILE *binposfp;          /* file pointer for trajectories  */
static int zerov = 0;           /* if true, use zero initial velocities */
static REAL_T tempi = 50.0;      /* initial temperature */
static int idum = -1;           /* random number seed */


#ifndef MORT
int set_belly_mask(MOLECULE_T *, char *, int *);
int set_cons_mask(MOLECULE_T *, char *, int *);
#endif

int writebinposhdr(FILE *);

int writebinposfrm(int, REAL_T *, FILE *);

REAL_T gauss(REAL_T *, REAL_T *);

REAL_T second(void);

int getxyzw(char **, int *, REAL_T *, int);

int putxyzw(char **, int *, REAL_T *, int);

void blacs_pinfo_(int *, int *);

void blacs_gridinfo_(int *, int *, int *, int *, int *);

void blacs_barrier_(int *, char *);

void blacs_exit_(int *);


/***********************************************************************
                            SECONDS()
************************************************************************/

/* Here is a timer function: can something better than clock() be used? */

REAL_T seconds(void)
{
#if defined(SPEC)
   return 0.1;  /* Everything takes a little time */
#else
   return (clock()/CLOCKS_PER_SEC);
#endif
}

/***********************************************************************
                            MYROC()
************************************************************************/

/*
 * Determine whether the address selects the processor row or column.
 * If the process is not active on the process grid, return 0.
 */

int myroc(int i, int mb, int nprow, int myrow)
{
   if (myrow < 0) {
      return (0);
   } else {
      return ((i / mb) % nprow == myrow);
   }
}

/***********************************************************************
                            GET_NR_DEBUG()
************************************************************************/

/* Return the state of nr_debug. */

int get_nr_debug(void)
{
   return nr_debug;
}

/***********************************************************************
                            GET_BLOCKSIZE()
************************************************************************/

/* Accessor for blocksize so that it doesn't have to be a global variable. */

int get_blocksize(void)
{
   return blocksize;
}

/***********************************************************************
                            GET_MYTASKID()
************************************************************************/

/* Accessor for mytaskid so that it doesn't have to be a global variable. */

int get_mytaskid(void)
{
   return mytaskid;
}

/***********************************************************************
                            GET_NUMTASKS()
************************************************************************/

/* Accessor for numtasks so that it doesn't have to be a global variable. */

int get_numtasks(void)
{
   return numtasks;
}

/***********************************************************************
                            MPIERROR()
************************************************************************/

/*
 * Reduce error codes from all tasks.
 *
 * If any task has -1 (instead of 0) all tasks will exit unless
 * an MPI or SCALAPACK error occurs in which case the MPI error
 * code is returned.  Note use of MPI_MIN in MPI_Allreduce.
 */

int mpierror(int myerror)
{
   int allerror;

#if defined(MPI) || defined(SCALAPACK)

   int ier = 0;
   if ((ier = MPI_Allreduce(&myerror, &allerror, 1, MPI_INT,
                            MPI_MIN, MPI_COMM_WORLD)) != MPI_SUCCESS) {
      return ier;
   }
#else

   allerror = myerror;

#endif

   if (allerror < 0) {

#ifdef SCALAPACK

      blacs_exit_(&ier);
      if (ier != MPI_SUCCESS) {
         return ier;
      }
#elif defined(MPI)

      if ((ier = MPI_Finalize()) != MPI_SUCCESS) {
         return ier;
      }
#endif

      exit(1);
   }
   return (0);
}

/***********************************************************************
                            REDUCERROR()
************************************************************************/

/* A front end to mpierror */

int reducerror(int myerror)
{
   return mpierror(myerror);
}

/***********************************************************************
                            MPIFINALIZE()
************************************************************************/

/*
 * Shut down mpi and return the MPI error code if MPI is defined;
 * otherwise, do nothing and return zero.
 * If SCALAPACK is defined, exit the BLACS.
 */

int mpifinalize(void)
{
   int ier = 0;

#ifdef SCALAPACK

#if 0
   blacs_exit_(&ier);
#else
   ier = MPI_Finalize();
#endif

#elif defined(MPI)

   ier = MPI_Finalize();

#endif

   return ier;
}

/***********************************************************************
                            MPIINIT()
************************************************************************/

/*
 * If MPI or SCALAPACK is defined, initialize MPI.
 *
 * Return mytaskid and numtasks via the call-by-reference parameters
 * rank and size, respectively.  If MPI or SCALAPACK is not defined,
 * the default values of mytaskid=0 and numtasks=1 will be returned.
 *
 * Return any MPI error code upon MPI error; otherwise, return zero.
 *
 */

int mpiinit(int *argc, char *argv[], int *rank, int *size)
{
   int ier = 0;

#ifdef SCALAPACK

   if ((ier = MPI_Init(argc, &argv)) != MPI_SUCCESS) {
      return ier;
   }
   blacs_pinfo_(&mytaskid, &numtasks);

#elif defined(MPI)

   if ((ier = MPI_Init(argc, &argv)) != MPI_SUCCESS) {
      return ier;
   }
   if ((ier = MPI_Comm_rank(MPI_COMM_WORLD, &mytaskid)) != MPI_SUCCESS) {
      return ier;
   }
   if ((ier = MPI_Comm_size(MPI_COMM_WORLD, &numtasks)) != MPI_SUCCESS) {
      return ier;
   }
#endif

   *rank = mytaskid;
   *size = numtasks;

   return ier;
}

/***********************************************************************
                            GETXYZ()
************************************************************************/

/* Supply dim and call getxyzw. */

int getxyz(char **fname, int *natom, REAL_T * x)
{
   return getxyzw(fname, natom, x, dim);
}

/***********************************************************************
                            PUTXYZ()
************************************************************************/

/* Supply dim and call putxyzw. */

int putxyz(char **fname, int *natom, REAL_T * x)
{
   return putxyzw(fname, natom, x, dim);
}

/***********************************************************************
                            STRINDEX()
************************************************************************/

/* Return index of t in s, -1 if none.  From Kernighan & Ritchie p. 69. */

int strindex(char s[], char t[])
{
   int i, j, k;

   for (i = 0; s[i] != '\0'; i++) {
      for (j = i, k = 0; t[k] != '\0' && s[j] == t[k]; j++, k++);
      if (k > 0 && t[k] == '\0')
         return i;
   }
   return -1;
}

/***********************************************************************
                            GETXYZW()
************************************************************************/

/*
 * Get the molecular topology and geometry from a Cartesian file.
 * Task 0 opens, reads and closes the file, and broadcasts the results
 * to all other tasks.
 */

int getxyzw(char **fname, int *natom, REAL_T * x, int dim)
{
   FILE *fp;
   int i, ier;
   char line[82], field[21];

   if (!*fname || !**fname)
      fp = stdin;
   else if (!strcmp(*fname, "-"))
      fp = stdin;
   else {
      ier = 0;
      if (get_mytaskid() == 0) {
         if ((fp = fopen(*fname, "r")) == NULL) {
            fprintf(stderr, "getx: can't open file %s\n", *fname);
            ier = -1;
         }
      }
      reducerror(ier);

      /* The file is open.  Set fp to NULL for all tasks but task 0. */

      if (get_mytaskid() != 0) {
         fp = NULL;
      }
   }

   ggets(line, sizeof(line), fp);       /* line with natoms  */

   sscanf(line, "%d", natom);

   ggets(line, sizeof(line), fp);
   for (i = 0; i < dim * (*natom); i++) {
      strncpy(field, line + 20 * (i % dim), 20);
      field[20] = '\0';
      x[i] = atof(field);
      if (i % dim == dim - 1 && i < dim * (*natom) - 1)
         ggets(line, sizeof(line), fp);
   }

   /*  old:
    *  for( i=0; i<3*(*natom); i++ ){ fscanf( fp, "%lf", &x[i] ); }
    */

   if (fp != stdin) {
      if (get_mytaskid() == 0) {
         fclose(fp);
      }
   }

   return (0);
}

/***********************************************************************
                            PUTXYZW()
************************************************************************/

/*
 * Put the molecular topology and geometry to a Cartesian file
 * via task 0 only.
 */

int putxyzw(char **fname, int *natom, REAL_T * x, int dim)
{
   FILE *fp;
   int i, ier;

   ier = 0;
   if (get_mytaskid() == 0) {
      if (!strcmp(*fname, "-"))
         fp = stdout;
      else if ((fp = fopen(*fname, "w")) == NULL) {
         rt_errormsg_s(TRUE, E_CANT_OPEN_S, *fname);
         ier = -1;
      }
      if (ier >= 0) {
         fprintf(fp, "%6d\n", *natom);
         for (i = 0; i < dim * (*natom); i += dim) {
            if (dim == 3) {
               fprintf(fp, "%20.15f%20.15f%20.15f\n", x[i], x[i + 1],
                       x[i + 2]);
            } else {
               fprintf(fp, "%20.15f%20.15f%20.15f%20.15f\n",
                       x[i], x[i + 1], x[i + 2], x[i + 3]);
            }
         }
         if (fp != stdout)
            fclose(fp);
      }
   }
   reducerror(ier);

   return (0);
}

/***********************************************************************
                            GETXV()
************************************************************************/

/*
 * Get the molecular topology and geometry from an Amber restart file.
 * Task 0 opens, reads and closes the file, and broadcasts the results
 * to all other tasks.
 */

int getxv(char **fname, int *natom, REAL_T * start_time, REAL_T * x,
          REAL_T * v)
{
   FILE *fp;
   int i, ier;
   char line[82], field[13];

   if (!*fname || !**fname)
      fp = stdin;
   else if (!strcmp(*fname, "-"))
      fp = stdin;
   else {
      ier = 0;
      if (get_mytaskid() == 0) {
         if ((fp = fopen(*fname, "r")) == NULL) {
            fprintf(stderr, "getxv: can't open file %s\n", *fname);
            ier = -1;
         }
      }
      reducerror(ier);

      /* The file is open.  Set fp to NULL for all tasks but task 0. */

      if (get_mytaskid() != 0) {
         fp = NULL;
      }
   }

   ggets(line, sizeof(line), fp);       /* title line  */
   ggets(line, sizeof(line), fp);       /* line with natoms and (possibly)
                                           the starting time  */

   *start_time = 0.0;           /* default, in case the next line does not assign it */
   sscanf(line, "%d %lf", natom, start_time);

   ggets(line, sizeof(line), fp);
   for (i = 0; i < 3 * (*natom); i++) {
      strncpy(field, line + 12 * (i % 6), 12);
      field[12] = '\0';
      x[i] = atof(field);
      if (i % 6 == 5 && i < 3 * (*natom) - 1)
         ggets(line, sizeof(line), fp);
   }

   if (ggets(line, sizeof(line), fp)) {
      for (i = 0; i < 3 * (*natom); i++) {
         strncpy(field, line + 12 * (i % 6), 12);
         field[12] = '\0';
         v[i] = atof(field);
         if (i % 6 == 5)
            ggets(line, sizeof(line), fp);
      }
   } else {
      for (i = 0; i < 3 * (*natom); i++)
         v[i] = 0.0;
      fprintf(nabout, "no velocities were found\n");
   }

   /*  old:
    *  for( i=0; i<3*(*natom); i++ ){ fscanf( fp, "%lf", &x[i] ); }
    *  for( i=0; i<3*(*natom); i++ ){ fscanf( fp, "%lf", &v[i] ); }
    */

   if (fp != stdin) {
      if (get_mytaskid() == 0) {
         fclose(fp);
      }
   }

   return (0);
}

/***********************************************************************
                            PUTXV()
************************************************************************/

/*
 * Put the molecular topology and geometry to an Amber restart file
 * via task 0 only.
 */

int putxv(char **fname, char **title, int *natom, REAL_T * start_time,
          REAL_T * x, REAL_T * v)
{
   FILE *fp;
   int i, ier;

   ier = 0;
   if (get_mytaskid() == 0) {
      if (!strcmp(*fname, "-"))
         fp = stdout;
      else if ((fp = fopen(*fname, "w")) == NULL) {
         rt_errormsg_s(TRUE, E_CANT_OPEN_S, *fname);
         ier = -1;
      }
      if (ier >= 0) {
         fprintf(fp, "%s\n", *title);
         fprintf(fp, "%6d%15.5f\n", *natom, *start_time);
         for (i = 0; i < 3 * (*natom); i += 6) {
            if (i + 3 < 3 * (*natom))
               fprintf(fp, "%12.7f%12.7f%12.7f%12.7f%12.7f%12.7f\n",
                       x[i], x[i + 1], x[i + 2], x[i + 3], x[i + 4],
                       x[i + 5]);
            else
               fprintf(fp, "%12.7f%12.7f%12.7f\n", x[i], x[i + 1],
                       x[i + 2]);
         }
         for (i = 0; i < 3 * (*natom); i += 6) {
            if (i + 3 < 3 * (*natom))
               fprintf(fp, "%12.7f%12.7f%12.7f%12.7f%12.7f%12.7f\n",
                       v[i], v[i + 1], v[i + 2], v[i + 3], v[i + 4],
                       v[i + 5]);
            else
               fprintf(fp, "%12.7f%12.7f%12.7f\n", v[i], v[i + 1],
                       v[i + 2]);
         }
         if (fp != stdout)
            fclose(fp);
      }
   }
   reducerror(ier);

   return (0);
}

/***********************************************************************
                            CHECKPOINT()
************************************************************************/

/*
 * Write a checkpoint file.  If the filename contains one or more %d
 * replace the leftmost %d with the iteration number; otherwise, append
 * the iteration number to the filename.
 */

void checkpoint(char *fname, int natom, REAL_T * x, int iter)
{
   char *filename, *buf;
   int i, j, k;

   /* Do nothing if the iteration number is negative. */

   if (iter < 0) {
      return;
   }

   /* Count the number of digits in the iteration number. */

   j = 0;
   i = iter;
   do {
      j++;
      i /= 10;
   } while (i > 0);

   /* Convert the iteration number to ascii. */

   buf = (char *) malloc(j + 1);
   sprintf(buf, "%d\0", iter);

   k = strlen(fname);
   if ((i = strindex(fname, "%d")) < 0) {
      filename = (char *) malloc(j + k + 1);
      strncpy(filename, fname, k);
      strncpy(&filename[k], buf, j);
      filename[j + k] = '\0';
   } else {
      filename = (char *) malloc(j + k - 1);
      strncpy(filename, fname, i);
      strncpy(&filename[i], buf, j);
      strncpy(&filename[i + j], &fname[i + 2], k - i - 2);
      filename[j + k - 2] = '\0';
   }
   putxyz(&filename, &natom, x);
   free(filename);
   free(buf);
}

/*
 * Here are the energy functions.  Order of these #includes is important,
 * in that eff.c should precede sff2.c.  Also, they should follow the
 * seconds(), reducerror() and checkpoint() function definitions, which
 * are immediately above.
 */

#include "eff.c"
#if !defined(DOCK) && !defined(NOPERFLIB)
#  include "sff2.c"
#endif
#ifdef MORT
#  include "mask.c"
#endif

/***********************************************************************
                            MME_INIT()
************************************************************************/

/* Initialize many variables and data structures for mme, mme2, md, etc. */

#ifdef MORT
int mme_init(PARMSTRUCT_T * prm_in, int *frozen_in, int *constrained_in,
             REAL_T * x0i, FILE * bfpi)
#else
int mme_init(MOLECULE_T * m, char *aexp, char *aexp2,
             REAL_T * x0i, FILE * bfpi)
#endif
{
   int i, j, at1, at2, iexcl, npairs, *numb;
   REAL_T scale;
   char atypi[3];
   static int nold = 0;         /* number of atoms on last invocation */

   /* These data structures are for inversion of the prm->ExclAt array. */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP)) || defined(SCALAPACK)
   int jexcl, jexcl_last, nexcl, *Istart;

   typedef struct atomstr {     /* Structure for temporary atom storage. */
      INT_T num;
      struct atomstr *nxt;
   } ATOMSTR;

   ATOMSTR *Iatoms, *Iatom;     /* Temporary array and pointer for storing atom names */
   ATOMSTR **Jatoms;            /* Array of pointers into the temporary array */
#endif

   dim = 3;
#ifdef MORT
   prm = prm_in;
#else
   prm = m->m_prm;
#endif
   x0 = x0i;
   binposfp = bfpi;

   /* Initialize the old LCPO data structures:  */
   if (gbsa == 1) {
      numb = ivector(0, prm->Natom);
      for (i = 0; i < prm->Natom; i++)
         numb[i] = 0;

      for (i = 0; i < prm->Nbona; i++) {
         at1 = prm->BondAt1[i] / 3;
         at2 = prm->BondAt2[i] / 3;
         numb[at1] += 1;
         numb[at2] += 1;
      }

      free_vector(P0, 0, prm->Natom);
      free_vector(P1, 0, prm->Natom);
      free_vector(P2, 0, prm->Natom);
      free_vector(P3, 0, prm->Natom);
      free_vector(P4, 0, prm->Natom);
      P0 = vector(0, prm->Natom);
      P1 = vector(0, prm->Natom);
      P2 = vector(0, prm->Natom);
      P3 = vector(0, prm->Natom);
      P4 = vector(0, prm->Natom);
      ineighbor = ivector(0, 30 * prm->Natom);
      /* 30 seems sufficient. this should, however, be changed */
      /* if it proves unsafe */

      for (i = 0; i < prm->Natom; i++) {
         atypi[0] = toupper(prm->AtomSym[i * 4]);
         atypi[1] = toupper(prm->AtomSym[i * 4 + 1]);
         atypi[2] = '\0';

         if (strncmp(atypi, "CT", 2) == 0) {
            if (numb[i] == 1) {
               P0[i] = 1.70 + 1.40;
               P1[i] = 0.7887;
               P2[i] = -0.28063;
               P3[i] = -0.0012968;
               P4[i] = 0.00039328;
            } else if (numb[i] == 2) {
               P0[i] = 1.70 + 1.4;
               P1[i] = 0.56482;
               P2[i] = -0.19608;
               P3[i] = -0.0010219;
               P4[i] = 0.0002658;
            } else if (numb[i] == 3) {
               P0[i] = 1.70 + 1.4;
               P1[i] = 0.23348;
               P2[i] = -0.072627;
               P3[i] = -0.00020079;
               P4[i] = 0.00007967;
            } else if (numb[i] == 4) {
               P0[i] = 1.70 + 1.4;
               P1[i] = P2[i] = P3[i] = P4[i] = 0.0;
            } else {
               fprintf(stdout, "bad number of bonds to CT: %d %d; ", i,
                       numb[i]);
               fprintf(stdout, "using default carbon parameters\n");
               P0[i] = 1.70 + 1.4;
               P1[i] = 0.51245;
               P2[i] = -0.15966;
               P3[i] = -0.00019781;
               P4[i] = 0.00016392;
            }

         } else if (strncmp(atypi, "C", 1) == 0) {
            if (numb[i] == 2) {
               P0[i] = 1.70 + 1.4;
               P1[i] = 0.51245;
               P2[i] = -0.15966;
               P3[i] = -0.00019781;
               P4[i] = 0.00016392;
            } else if (numb[i] == 3) {
               P0[i] = 1.70 + 1.4;
               P1[i] = 0.070344;
               P2[i] = -0.019015;
               P3[i] = -0.000022009;
               P4[i] = 0.000016875;
            } else if (numb[i] == 4) {
               P0[i] = 1.70 + 1.4;
               P1[i] = P2[i] = P3[i] = P4[i] = 0.0;
            } else {
               fprintf(stdout, "bad number of bonds to C: %d %d; ", i,
                       numb[i]);
               fprintf(stdout, "using default carbon parameters\n");
               P0[i] = 1.70 + 1.4;
               P1[i] = 0.51245;
               P2[i] = -0.15966;
               P3[i] = -0.00019781;
               P4[i] = 0.00016392;
            }

         } else if (strncmp(atypi, "O ", 2) == 0) {
            P0[i] = 1.6 + 1.4;
            P1[i] = 0.68563;
            P2[i] = -0.1868;
            P3[i] = -0.00135573;
            P4[i] = 0.00023743;

         } else if (strncmp(atypi, "O2", 2) == 0) {
            P0[i] = 1.6 + 1.4;
            P1[i] = 0.88857;
            P2[i] = -0.33421;
            P3[i] = -0.0018683;
            P4[i] = 0.00049372;

         } else if (strncmp(atypi, "O", 1) == 0) {
            if (numb[i] == 1) {
               P0[i] = 1.6 + 1.4;
               P1[i] = 0.77914;
               P2[i] = -0.25262;
               P3[i] = -0.0016056;
               P4[i] = 0.00035071;
            } else if (numb[i] == 2) {
               P0[i] = 1.6 + 1.4;
               P1[i] = 0.49392;
               P2[i] = -0.16038;
               P3[i] = -0.00015512;
               P4[i] = 0.00016453;
            } else {
               fprintf(stdout, "bad number of bonds to O*: %d %d; ", i,
                       numb[i]);
               fprintf(stdout, "using default oxygen parameters\n");
               P0[i] = 1.6 + 1.4;
               P1[i] = 0.68563;
               P2[i] = -0.1868;
               P3[i] = -0.00135573;
               P4[i] = 0.00023743;
            }

         } else if (strncmp(atypi, "N3", 2) == 0) {
            if (numb[i] == 1) {
               P0[i] = 1.65 + 1.4;
               P1[i] = 0.078602;
               P2[i] = -0.29198;
               P3[i] = -0.0006537;
               P4[i] = 0.00036247;
            } else if (numb[i] == 2) {
               P0[i] = 1.65 + 1.4;
               P1[i] = 0.22599;
               P2[i] = -0.036648;
               P3[i] = -0.0012297;
               P4[i] = 0.000080038;
            } else if (numb[i] == 3) {
               P0[i] = 1.65 + 1.4;
               P1[i] = 0.051481;
               P2[i] = -0.012603;
               P3[i] = -0.00032006;
               P4[i] = 0.000024774;
            } else {
               fprintf(stdout, "bad number of bonds to N3: %d %d; ", i,
                       numb[i]);
               fprintf(stdout, "using default nitrogen parameters\n");
               P0[i] = 1.65 + 1.4;
               P1[i] = 0.73511;
               P2[i] = -0.22116;
               P3[i] = -0.00089148;
               P4[i] = 0.0002523;
            }

         } else if (strncmp(atypi, "N", 1) == 0) {
            if (numb[i] == 1) {
               P0[i] = 1.65 + 1.4;
               P1[i] = 0.73511;
               P2[i] = -0.22116;
               P3[i] = -0.00089148;
               P4[i] = 0.0002523;
            } else if (numb[i] == 2) {
               P0[i] = 1.65 + 1.4;
               P1[i] = 0.41102;
               P2[i] = -0.12254;
               P3[i] = -0.000075448;
               P4[i] = 0.00011804;
            } else if (numb[i] == 3) {
               P0[i] = 1.65 + 1.4;
               P1[i] = 0.062577;
               P2[i] = -0.017874;
               P3[i] = -0.00008312;
               P4[i] = 0.000019849;
            } else {
               fprintf(stdout, "bad number of bonds to N: %d %d; ", i,
                       numb[i]);
               fprintf(stdout, "using default nitrogen parameters\n");
               P0[i] = 1.65 + 1.4;
               P1[i] = 0.73511;
               P2[i] = -0.22116;
               P3[i] = -0.00089148;
               P4[i] = 0.0002523;
            }

         } else if (strncmp(atypi, "SH", 2) == 0) {
            P0[i] = 1.9 + 1.4;
            P1[i] = 0.7722;
            P2[i] = -0.26393;
            P3[i] = 0.0010629;
            P4[i] = 0.0002179;

         } else if (strncmp(atypi, "S", 1) == 0) {
            P0[i] = 1.9 + 1.4;
            P1[i] = 0.54581;
            P2[i] = -0.19477;
            P3[i] = -0.0012873;
            P4[i] = 0.00029247;

         } else if (strncmp(atypi, "P", 1) == 0) {
            if (numb[i] == 3) {
               P0[i] = 1.9 + 1.4;
               P1[i] = 0.3865;
               P2[i] = -0.18249;
               P3[i] = -0.0036598;
               P4[i] = 0.0004264;
            } else if (numb[i] == 4) {
               P0[i] = 1.9 + 1.4;
               P1[i] = 0.03873;
               P2[i] = -0.0089339;
               P3[i] = 0.0000083582;
               P4[i] = 0.0000030381;
            } else {
               fprintf(stdout, "bad number of bonds to P: %d %d; ", i,
                       numb[i]);
               fprintf(stdout, "using default phosphorus parameters\n");
               P0[i] = 1.9 + 1.4;
               P1[i] = 0.3865;
               P2[i] = -0.18249;
               P3[i] = -0.0036598;
               P4[i] = 0.0004264;
            }

         } else if (strncmp(atypi, "H", 1) == 0) {
            P0[i] = 1.40;
            P1[i] = P2[i] = P3[i] = P4[i] = 0.0;

         } else {
            P0[i] = 1.70 + 1.4;
            P1[i] = 0.51245;
            P2[i] = -0.15966;
            P3[i] = -0.00019781;
            P4[i] = 0.00016392;
            if (mytaskid == 0) {
               fprintf(stdout, "Using carbon SA parms for atom type %s\n",
                       atypi);
            }
         }
         if (P0[i] > 5.0)
            printf("bad p0: %d %12.6f %s\n", i, P0[i], atypi);
      }
      free_ivector(numb, 0, prm->Natom);

   }
   /*GBSA initialization ends here */
   /*  alter the charges if dielc is not unity  */
   if (dielc != 1.0) {
      assert(prm != NULL);
      scale = 1.0 / sqrt(dielc);
      if (mytaskid == 0) {
         fprintf(nabout, "scaling charges by %8.3f\n", scale);
      }
      for (i = 0; i < prm->Natom; i++) {
         prm->Charges[i] *= scale;
      }
   }
#ifndef MORT
   /* Free (if allocated) then reallocate the frozen and constrained vectors. */

   free_ivector(frozen, 0, prm->Natom);
   free_ivector(constrained, 0, prm->Natom);

   frozen = ivector(0, prm->Natom);
   constrained = ivector(0, prm->Natom);
#endif

   if (binposfp != NULL)
      writebinposhdr(binposfp);

#ifdef MORT
   constrained = constrained_in;
   nconstrained = 0;
   for (i = 0; i < prm->Natom; ++i) {
      if (constrained[i])
         nconstrained++;
   }
   if (nconstrained)
      printf("constrained %d atoms from input array\n", nconstrained);
#else
   nconstrained = set_cons_mask(m, aexp2, constrained);
   if (nconstrained) {
      if (aexp2 == NULL) {
         if (mytaskid == 0) {
            fprintf(nabout, "constrained all %d atoms\n", nconstrained);
         }
      } else {
         if (mytaskid == 0) {
            fprintf(nabout, "constrained %d atoms using expression %s\n",
                    nconstrained, aexp2);
         }
      }
   }
#endif

   /*  set up GB/OBC parameters:  */
   if (gb == 2) {
      gbalpha = 0.8;
      gbbeta = 0.0;
      gbgamma = 2.909125;
   } else if (gb == 5) {
      gbalpha = 1.0;
      gbbeta = 0.8;
      gbgamma = 4.85;
   }

   /*
    * Free (if allocated) then reallocate the non-polar pairlistnp and pair count
    * arrays.  The pair list array is an array of arrays, and could contain NULL
    * elements if mme_init were called a second time prior to initialization
    * of the pair lists by nblist.
    */

   if (pairlistnp != NULL) {
      for (i = 0; i < nold; i++) {
         free_ivector(pairlistnp[i], 0, 1);
      }
      free(pairlistnp);
   }
   np_pairs = -1;

   free_ivector(upairsnp, 0, prm->Natom);
   free_ivector(lpairsnp, 0, prm->Natom);

   upairsnp = ivector(0, prm->Natom);
   lpairsnp = ivector(0, prm->Natom);

   if ((pairlistnp = (int **) malloc(prm->Natom * sizeof(int *))) == NULL) {
      fprintf(nabout, "Error allocating pairlistnp array in mme_init!\n");
      fflush(nabout);
      mpierror(-1);
   }

   for (i = 0; i < prm->Natom; i++) {
      pairlistnp[i] = NULL;
      lpairsnp[i] = upairsnp[i] = 0;
   }

   /*
    * Free (if allocated) then reallocate the non-polar pairlist2np and pair count
    * arrays.  The pair list array is an array of arrays, and could contain NULL
    * elements if mme_init were called a second time prior to initialization
    * of the pair lists by nblist.
    */

   if (pairlist2np != NULL) {
      for (i = 0; i < nold; i++) {
         free_ivector(pairlist2np[i], 0, 1);
      }
      free(pairlist2np);
   }
   np_pairs2 = -1;

   free_ivector(upairs2np, 0, prm->Natom);
   free_ivector(lpairs2np, 0, prm->Natom);

   upairs2np = ivector(0, prm->Natom);
   lpairs2np = ivector(0, prm->Natom);

   if ((pairlist2np = (int **) malloc(prm->Natom * sizeof(int *))) == NULL) {
      fprintf(nabout, "Error allocating pairlist2np array in mme_init!\n");
      fflush(nabout);
      mpierror(-1);
   }

   for (i = 0; i < prm->Natom; i++) {
      pairlist2np[i] = NULL;
      lpairs2np[i] = upairs2np[i] = 0;
   }

#if defined(SCALAPACK) || defined(MPI)

   /*
    * Free (if allocated) then reallocate the non-bonded pairlist2 and pair count
    * arrays.  The pair list array is an array of arrays, and could contain NULL
    * elements if mme_init were called a second time prior to initialization
    * of the pair lists by nblist.
    */

   if (pairlist2 != NULL) {
      for (i = 0; i < nold; i++) {
         free_ivector(pairlist2[i], 0, 1);
      }
      free(pairlist2);
   }
   nb_pairs2 = -1;

   free_ivector(upairs2, 0, prm->Natom);
   free_ivector(lpairs2, 0, prm->Natom);

   upairs2 = ivector(0, prm->Natom);
   lpairs2 = ivector(0, prm->Natom);

   if ((pairlist2 = (int **) malloc(prm->Natom * sizeof(int *))) == NULL) {
      fprintf(nabout, "Error allocating pairlist2 array in mme_init!\n");
      fflush(nabout);
      mpierror(-1);
   }

   for (i = 0; i < prm->Natom; i++) {
      pairlist2[i] = NULL;
      lpairs2[i] = upairs2[i] = 0;
   }

#endif

   /*
    * Free (if allocated) then reallocate the non-bonded pairlist and pair count
    * arrays.  The pair list array is an array of arrays, and could contain NULL
    * elements if mme_init were called a second time prior to initialization
    * of the pair lists by nblist.
    */

   if (pairlist != NULL) {
      for (i = 0; i < nold; i++) {
         free_ivector(pairlist[i], 0, 1);
      }
      free(pairlist);
   }
   nb_pairs = -1;

   free_ivector(upairs, 0, prm->Natom);
   free_ivector(lpairs, 0, prm->Natom);

   upairs = ivector(0, prm->Natom);
   lpairs = ivector(0, prm->Natom);

   if ((pairlist = (int **) malloc(prm->Natom * sizeof(int *))) == NULL) {
      fprintf(nabout, "Error allocating pairlist array in mme_init!\n");
      fflush(nabout);
      mpierror(-1);
   }

   for (i = 0; i < prm->Natom; i++) {
      pairlist[i] = NULL;
      lpairs[i] = upairs[i] = 0;
   }

#ifdef MORT
   frozen = frozen_in;
   nfrozen = set_belly_mask(prm->Natom, frozen);
   if (nfrozen)
      printf("freezing %d atoms from input array, and updated prm\n",
             nfrozen);
#else
   nfrozen = set_belly_mask(m, aexp, frozen);
   if (nfrozen) {
      if (mytaskid == 0) {
         fprintf(nabout,
                 "freezing %d atoms using expression %s for moving atoms\n",
                 nfrozen, aexp);
      }
   }
#endif

   /*
    * The prm->N14pairlist array contains pair lists for all atoms,
    * contiguously packed.  Convert this data structure to the
    * N14pearlist array which is an array of pair lists where each
    * pair list comprises pair atoms for one atom only.
    *
    * Free (if allocated) then reallocate and construct the N14pearlist array.
    * Construction of the N14pearlist array must be accomplished after the call
    * to set_belly_mask() because that function modifies prm->N14pairlist.
    */

   if (N14pearlist != NULL) {
      for (i = 0; i < nold; i++) {
         free_ivector(N14pearlist[i], 0, 1);
      }
      free(N14pearlist);
   }

   if ((N14pearlist = (int **) malloc(prm->Natom * sizeof(int *))) == NULL) {
      fprintf(nabout, "Error allocating N14pearlist array in mme_init!\n");
      fflush(nabout);
      mpierror(-1);
   }

   npairs = 0;
   for (i = 0; i < prm->Natom; i++) {
      N14pearlist[i] = NULL;
      if (prm->N14pairs[i] > 0) {
         N14pearlist[i] = ivector(0, prm->N14pairs[i]);
         for (j = 0; j < prm->N14pairs[i]; j++) {
            N14pearlist[i][j] = prm->N14pairlist[npairs + j];
         }
         npairs += prm->N14pairs[i];
      }
   }

   /*
    * The excluded atom list for atom i is a contiguous set of atom numbers in
    * the prm->ExclAt array that are excluded from pair interactions with atom i.
    * These atom numbers are all greater that i and they are sorted in increasing
    * order.  The number of atoms that are excluded from atom i is stored in the
    * prm->Iblo array.  Convert the prm->ExclAt array to the IexclAt array,
    * which is an array of arrays of per-atom excluded atom lists.
    *
    * Free (if allocated) then reallocate the IexclAt array.  Allocate
    * per-atom excluded atom lists and copy from prm->ExclAt into them.
    */

   if (IexclAt != NULL) {
      for (i = 0; i < nold; i++) {
         free_ivector(IexclAt[i], 0, 1);
      }
      free(IexclAt);
   }

   if ((IexclAt = (int **) malloc(prm->Natom * sizeof(int *))) == NULL) {
      fprintf(nabout, "Error allocating IexclAt array in mme_init!\n");
      fflush(nabout);
      mpierror(-1);
   }

   iexcl = 0;
   for (i = 0; i < prm->Natom; i++) {
      IexclAt[i] = NULL;
      if (prm->Iblo[i] > 0) {
         IexclAt[i] = ivector(0, prm->Iblo[i]);
         for (j = 0; j < prm->Iblo[i]; j++) {
            IexclAt[i][j] = prm->ExclAt[iexcl + j];
         }
      }
      iexcl += prm->Iblo[i];
   }

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP)) || defined(SCALAPACK)

   /*
    * Create another excluded atom list for atom j where the atom
    * numbers are all less than j and sort them in increasing order.
    * Store this list in the JexclAt array, which is an array of
    * arrays of per-atom excluded atom lists.  Store into the Jblo
    * array the number of atoms that are excluded from atom j.
    *
    * Free (if allocated) then reallocate the JexclAt and Jblo arrays.
    * Allocate the Jatoms array that will point to the linked lists to be
    * processed.
    */

   if (JexclAt != NULL) {
      for (j = 0; j < prm->Natom; j++) {
         if (JexclAt[j] != NULL) {
            if (Jblo != NULL) {
               free_ivector(JexclAt[j], 0, Jblo[j]);
            } else {
               fprintf(nabout,
                       "Error deallocating JexclAt[%d] in mme_init!\n", j);
               fflush(nabout);
               mpierror(-1);
            }
         }
      }
      free(JexclAt);
   }

   if (Jblo != NULL) {
      free_ivector(Jblo, 0, prm->Natom);
   }

   Jblo = ivector(0, prm->Natom);

   if ((JexclAt = (int **) malloc(prm->Natom * sizeof(int *))) == NULL) {
      fprintf(nabout, "Error allocating JexclAt array in mme_init!\n");
      fflush(nabout);
      mpierror(-1);
   }

   if ((Jatoms =
        (ATOMSTR **) malloc(prm->Natom * sizeof(ATOMSTR *))) == NULL) {
      fprintf(nabout, "Error allocating Jatoms array in mme_init!\n");
      fflush(nabout);
      mpierror(-1);
   }

   /*
    * Allocate the Istart array then walk the prm->Iblo array to load
    * Istart and to obtain the total length of the prm->ExclAt array.
    */

   Istart = ivector(0, prm->Natom);
   iexcl = 0;
   for (i = 0; i < prm->Natom; i++) {
      Istart[i] = iexcl;
      iexcl += prm->Iblo[i];
   }

   /*
    * Allocate the Iatoms array to have the same number of entries
    * as prm->ExclAt.  This array will store the linked lists.
    */

   if ((Iatoms = (ATOMSTR *) malloc(iexcl * sizeof(ATOMSTR))) == NULL) {
      fprintf(nabout, "Error allocating Iatoms array in mme_init!\n");
      fflush(nabout);
      mpierror(-1);
   }

   /* Initialize the Jatoms and Jblo arrays. */

   for (i = 0; i < prm->Natom; i++) {
      Jatoms[i] = NULL;
      Jblo[i] = 0;
   }

   /*
    * Expand the excluded atom list from largest i to smallest i,
    * and get the atom numbers of all atoms j that are excluded
    * from atom i.  For each such atom j that is greater than zero,
    * allocate a record in the Iatoms array and store atom i into
    * the record.  Catenate the record onto the head of the linked
    * list that is stored in the jth element of the Jatom array.
    * Count the number of records that are catenated onto each linked
    * list and store this count in the jth element of the Jblo array.
    *
    * Each linked list will be ordered by smallest i to largest i.
    *
    * Atom numbers in the prm->ExclAt array lie in the range 1..n
    * whereas array indices of Jatoms and Jblo lie in the range 0..n-1.
    */

   Iatom = Iatoms;
   for (i = prm->Natom - 1; i >= 0; i--) {
      jexcl = Istart[i];
      jexcl_last = jexcl + prm->Iblo[i];
      for (j = jexcl; j < jexcl_last; j++) {
         nexcl = prm->ExclAt[j] - 1;
         if ((nexcl >= 0) && (nexcl < prm->Natom)) {
            Iatom->num = i;
            Iatom->nxt = Jatoms[nexcl];
            Jatoms[nexcl] = Iatom++;
            Jblo[nexcl]++;
         }
      }
   }

   /*
    * Walk the Jatoms array from smallest j to largest j, allocate
    * per-atom excluded atom lists, and copy the atom numbers from
    * each linked list of Jatoms into a per-atom excluded atom list
    * that is accessed via JexclAt.
    *
    * Atom numbers in the JexclAt array lie in the range 1..n whereas
    * whereas atom numbers in the Jatoms array lie in the range 0..n-1.
    */

   for (j = 0; j < prm->Natom; j++) {
      JexclAt[j] = NULL;
      if (Jatoms[j] != NULL) {
         JexclAt[j] = ivector(0, Jblo[j]);
         for (i = 0; i < Jblo[j]; i++) {
            JexclAt[j][i] = Jatoms[j]->num + 1;
            Jatoms[j] = Jatoms[j]->nxt;
         }
      }
   }

   /* Free the temporary arrays. */

   free(Iatoms);
   free(Jatoms);
   free_ivector(Istart, 0, prm->Natom);

#endif

   /* Initialize the timing variables. */

   tnonb = tbond = tangl = tphi = tborn = tcons = tmme = tpair = 0.0;
   tnonb2 = tbond2 = tangl2 = tphi2 = tborn2 = tcons2 = tmme2 = 0.0;
   tconjgrad = tmd = tcholesky = tnewton = tlevel = tdiag = tdiagd = 0.0;
   tmmeone = tmmetwo = tb2g1 = tb2g2 = tb2g3 = tloop = tdgeev = 0.0;
   treduce = 0.0;

   /*  if the size has grown, call mme34() to free up the storage:  */
   i = -3;
   if (prm->Natom > nold)
      mme34(x0i, x0i, &i);

   /*  save the number of atoms used, in case it changes later: */
   nold = prm->Natom;

   return (0);
}

/***********************************************************************
                            MME()
************************************************************************/

/* Here is the mme function for 3D. */

REAL_T mme(REAL_T * x, REAL_T * f, int *iter)
{
   dim = 3;
   return mme34(x, f, iter);
}

/***********************************************************************
                            MME4()
************************************************************************/

/* Here is the mme function for 4D. */

REAL_T mme4(REAL_T * x, REAL_T * f, int *iter)
{
   dim = 4;
   return mme34(x, f, iter);
}

/***********************************************************************
                            GET_MASSES()
************************************************************************/

/* Get the masses for md.  Note: dim = 3 or 4, for 3D or 4D problem. */

static
int get_masses(REAL_T * minv)
{

   int i, k;
   REAL_T am;

   for (k = 0, i = 0; i < prm->Natom; i++) {
      am = 1. / prm->Masses[i];
      minv[k + 0] = am;
      minv[k + 1] = am;
      minv[k + 2] = am;
      if (dim == 4) {
         minv[k + 3] = am;
      }
      k += dim;
   }
   return (0);
}

/***********************************************************************
                            MM_SET_CHECKPOINT()
************************************************************************/

/* Set the checkpoint filename. */

void mm_set_checkpoint(char **fname)
{
   chknm = (char *) strdup(*fname);
   if (mytaskid == 0) {
      fprintf(nabout, "\tcheckpoint:  %s\n", *fname);
      fflush(nabout);
   }
}

/***********************************************************************
                            MD()
************************************************************************/

/* Here is the molecular dynamics function. */

int md(int n, int maxstep, REAL_T * x, REAL_T * f, REAL_T * v,
       REAL_T(*func) (REAL_T *, REAL_T *, int *))
{
   REAL_T dtx, dt5, rndf, dttp, ekin0, ekin, epot, etot, tscal, temp, dt2i,
       sd;
   REAL_T gammai, c_implic, c_explic, c_ave, sdfac, zero, invmass, t1, t2,
       tempv;
   int nstep, i;

   static REAL_T *xold = NULL, *sqrmass = NULL, *accel = NULL, *minv =
       NULL;
   static int nold = 0;         /* save previous size, in case it increases */

   t1 = seconds();

   /*
    * Note: the following allocations assume that the dimensionality
    * of the problem does not change during one invocation of NAB.
    * If, for example, md were called with dim==3 and then with dim==4,
    * these allocations would not be repeated for the larger value
    * of n that would be necessitated by dim==4.
    */

   /*  space for holding inverse masses  */
   if (minv == NULL || n > nold) {
      free(minv);
      minv = (REAL_T *) calloc(n, sizeof(REAL_T));
      if (minv == NULL) {
         fprintf(stderr, "unable to allocate space for minv\n");
         fflush(stderr);
         mpierror(-1);
      }
   }

   /*  space for holding extra set of coords  */
   if ((xold == NULL || n > nold)) {
      free(xold);
      xold = (REAL_T *) calloc(n, sizeof(REAL_T));
      if (xold == NULL) {
         fprintf(stderr, "unable to allocate space for xold\n");
         fflush(stderr);
         mpierror(-1);
      }
   }

   /*  space for holding square root of mass */
   if ((gamma_ln != 0.0) && (sqrmass == NULL || n > nold)) {
      free(sqrmass);
      sqrmass = (REAL_T *) calloc(n, sizeof(REAL_T));
      if (sqrmass == NULL) {
         fprintf(stderr, "unable to allocate space for sqrmass\n");
         fflush(stderr);
         mpierror(-1);
      }
   }

   /*  space for holding the accelerations */
   if (accel == NULL || n > nold) {
      free(accel);
      accel = (REAL_T *) calloc(n, sizeof(REAL_T));
      if (accel == NULL) {
         fprintf(stderr, "unable to allocate space for accel\n");
         fflush(stderr);
         mpierror(-1);
      }
   }
   nold = n;

   dtx = dt * 20.455;
   dt5 = 0.5 * dtx;
   rndf = n - 3 * nfrozen;
   ekin0 = boltz2 * rndf * temp0;       /* target kinetic energy  */
   dttp = dt / tautp;
   zero = 0.0;
   gammai = gamma_ln / 20.455;
   c_implic = 1.0 / (1.0 + gammai * dt5);
   c_explic = 1.0 - gammai * dt5;
   c_ave = 1.0 + gammai * dt5;
   dt2i = 1. / (dtx * dtx);
   sdfac = sqrt(4.0 * gammai * boltz2 * temp0 / dtx);

   invmass = 1. / genmass;
   if (prm) {
      get_masses(minv);
   } else {                     /* if no prmtop file is present, see all masses to genmass */
      for (i = 0; i < n; i++)
         minv[i] = invmass;
   }

   if (zerov) {
      for (i = 0; i < n; i++)
         v[i] = 0.0;
      ekin = 0.0;

   } else if (tempi > 0.0) {
      ekin = 0.0;
      for (i = 0; i < n; i++) {
         if (frozen[i / dim]) {
            v[i] = 0.0;
         } else {
            sd = sqrt(2. * boltz2 * tempi * minv[i]);
            v[i] = gauss(&zero, &sd);
            ekin += v[i] * v[i] / minv[i];
         }
      }
      ekin *= 0.5;
      temp = ekin / (boltz2 * rndf);

   } else {
      for (ekin = 0., i = 0; i < n; i++)
         ekin += v[i] * v[i] * minv[i];
      ekin *= 0.5;
   }

   /*  main loop for the dynamics:  Use velocity Verlet algorithm  */

   nstep = 0;

   t2 = seconds();
   tmd += t2 - t1;

   epot = (*func) (x, f, &nstep);

   t1 = seconds();

   if (gammai == 0.0) {
      for (i = 0; i < n; i++) {
         accel[i] = -f[i] * minv[i] * dt5;
      }
   } else {
      for (i = 0; i < n; i++) {
         if (frozen[i / dim]) {
            accel[i] = 0.0;
         } else {
            sqrmass[i] = 1.0 / sqrt(minv[i]);
            sd = sdfac * sqrmass[i];
            accel[i] = (-f[i] + gauss(&zero, &sd)) * minv[i] * dt5;
         }
      }
   }

   for (nstep = 1; nstep <= maxstep; nstep++) {

      if (ekin > 0.01)
         tscal = sqrt(1. + dttp * (ekin0 / ekin - 1.));
      else
         tscal = 1.0;
      ekin = 0.0;

      for (i = 0; i < n; i++)
         xold[i] = x[i];

      /*  update v[i] by half-timestep, then x[] by a full timestep:  */
      if (gammai == 0.0) {
         for (i = 0; i < n; i++) {
            v[i] = (v[i] + accel[i]) * tscal;
            v[i] = v[i] > vlimit ? vlimit : v[i];
            v[i] = v[i] < -vlimit ? -vlimit : v[i];
            x[i] += v[i] * dtx;
         }
      } else {
         for (i = 0; i < n; i++) {
            v[i] = c_explic * v[i] + accel[i];
            v[i] = v[i] > vlimit ? vlimit : v[i];
            v[i] = v[i] < -vlimit ? -vlimit : v[i];
            x[i] += v[i] * dtx;
         }
      }

      /*  get the new energies and forces:  */

      t2 = seconds();
      tmd += t2 - t1;

      epot = (*func) (x, f, &nstep);

      t1 = seconds();

      /*  update the velocities by the second half-timestep:  */
      if (gammai == 0.0) {
         for (i = 0; i < n; i++) {
            accel[i] = -f[i] * minv[i] * dt5;
            v[i] = (v[i] + accel[i]) * tscal;
         }
      } else {
         for (i = 0; i < n; i++) {
            if (!frozen[i / dim]) {
               sd = sdfac * sqrmass[i];
               accel[i] = (-f[i] + gauss(&zero, &sd)) * minv[i] * dt5;
               v[i] = (v[i] + accel[i]) * c_implic;
            }
         }
      }

      /*  get the kinetic and total energies; update the time:  */

      /*  For Langevin integrators, there are choices about how the velocities
       *  (and hence the kinetic energies and temperatures) should be defined.
       *  For a discussion, see Pastor, Brooks & Szabo, Mol. Phys. 65:1409,
       *  1988 (PBS).
       *
       *  Here is the "strightforward" code from earlier versions of NAB;
       *  it should yield a temperature that is too low about about a factor
       *  of (1 + 0.5*gammai*dtx):
       */

      for (i = 0; i < n; i++)
         ekin += v[i] * v[i] / minv[i];
      ekin *= 0.5;

      /*
       * Here is what Amber does to "adjust" velocities (using ekin from above):
       * (This is from Loncharich, Brooks and Pastor, Biopolymers 32:523-535
       * (1992), Eq. 11.  See also Eq. 3.5c of PBS.  This makes the kinetic
       * energy and temperature "look" right, but doesn't change the trajectory.
       */

      if (gammai > 0.)
         ekin *= c_ave;

      /*
       * Following is from Eq. 2.8a of PBS.  It should give a temperature that
       * is slightly too low, but the amount by which it is too low is
       * independent of gammai:
       * 
       *    if( gammai == 0. ){ 
       *       for (i = 0; i < n; i++) ekin += v[i]*v[i]/minv[i];
       *       ekin *= 0.5;
       *    } else {
       *       for (i = 0; i < n; i++){
       *          tempv = (x[i] - xold[i])*sqrmass[i];
       *          ekin += tempv*tempv;
       *       }
       *       ekin *= 0.5*dt2i;
       *    }
       */

      etot = ekin + epot;
      temp = ekin / (boltz2 * rndf);
      t += dt;

      /* Print the energies and temperature but only for task zero. */

      if (mytaskid == 0) {
         if (nstep % ntpr_md == 0 || nstep == 1) {
            fprintf(nabout,
                    "md:       %5d %10.3f %10.2f %10.2f %10.2f %10.2f\n",
                    nstep, t, ekin, epot, ekin + epot, temp);
            fflush(nabout);
         }
      }
      if (ntwx > 0 && nstep % ntwx == 0 && binposfp != NULL)
         writebinposfrm(n / 3, x, binposfp);
   }

   /* Free the static gradient vector from within func. */

   nstep = -3;
   (*func) (x, f, &nstep);

   t2 = seconds();
   tmd += t2 - t1;

   return (0);
}

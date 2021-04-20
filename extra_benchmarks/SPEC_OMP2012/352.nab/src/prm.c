/*
 * COPYRIGHT 1992, REGENTS OF THE UNIVERSITY OF CALIFORNIA
 *
 *  prm.c - read information from an amber PARM topology file: 
 *	atom/residue/bond/charge info, plus force field data. 
 *	This file and the accompanying prm.h may be distributed 
 *	provided this notice is retained unmodified and provided 
 *	that any modifications to the rest of the file are noted 
 *	in comments.
 *
 *	Bill Ross, UCSF 1994
 *
 *  MPI function calls added by Russ Brown (russ.brown@sun.com).
 *
 *  With MPI, input is accomplished by task zero, and the results
 *  are broadcast to the other tasks.  Output is accomplished by
 *  task zero.  The MPI datatypes assume that INT_T is of type int
 *  and that REAL_T is of type double.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(SPEC_WINDOWS) || defined(SPEC_NO_SYS_ERRNO_H)
# include <errno.h>
#else
#include <sys/errno.h>
#endif
#include <stdlib.h>
#if !defined(SPEC)
#include <time.h>
#endif
#include <math.h>

#include "nab.h"
#include "errormsg.h"

#ifndef MORT
ATOM_T *NAB_mnext();
#endif

#if defined(MPI) || defined(SCALAPACK)
#include "mpi.h"
#endif

extern int errno;

static int compressed = 0;
static char e_msg[256];

static char SsFormat[81];       /* holds the current Fortran format  */
static int SiOnLine, SbWroteNothing, SiPerLine;
static FILE *SfFile;

/*
 *       fortran formats 
 *       9118 FORMAT(12I6)
 *       9128 FORMAT(5E16.8)
 */

char *f9118 = "%6d%6d%6d%6d%6d%6d%6d%6d%6d%6d%6d%6d\n";

int get_mytaskid(void);
int rt_errormsg_s(int, char[], char[]);
void reducerror(int);

/***********************************************************************
                            GGETS()
************************************************************************/

/* ggets() - call fgets for task 0 only; broadcast result to other tasks */

char *ggets(char *line, int count, FILE * file)
{
   char *result;
   int inul;

   /*
    * It is not possible for MPI to broadcast a char* pointer
    * so broadcast the result in the inul integer instead.
    */

   inul = 0;
   if (get_mytaskid() == 0) {
      result = fgets(line, count, file);
      if (result == NULL) {
         inul = -1;
      }
   }
#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(&inul, 1, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   /*
    * If the result was NULL return NULL; otherwise,
    * broadcast and return the line buffer.  This
    * approach mimics the normal and EOF return of the
    * fgets function, because upon EOF the line buffer
    * is unchanged.  And upon error the line buffer is
    * indeterminate so no reasonable action is possible.
    */

   if (inul < 0) {
      return (NULL);
   } else {

#if defined(MPI) || defined(SCALAPACK)
      MPI_Bcast(line, count, MPI_CHAR, 0, MPI_COMM_WORLD);
#endif

      return (line);
   }
}

/***********************************************************************
                            SKIPEOLN()
************************************************************************/

/* Skip to end of line; exit if end-of-file encountered. */

static void skipeoln(FILE * file)
{
   int i, ier;

   ier = 0;
   if (get_mytaskid() == 0) {
      while ((i = getc(file)) != 10) {
         if (i == EOF) {
            fprintf(nabout, "unexpected end in parm file\n");
            ier = -1;
            break;
         }
      }
   }
   reducerror(ier);
}

/***********************************************************************
                            ISCOMPRESSED()
************************************************************************/

/* iscompressed() - look for .Z at end of name  */

static int iscompressed(char *name)
{
#if !defined(SPEC)
   int i;

   i = strlen(name) - 1;        /* last char in name */

   if (i < 0) {
      if (get_mytaskid() == 0) {
         fprintf(stderr, "programming error: name w/ length %d\n", i);
      }
      reducerror(-1);
   }
   if (i < 3)
      return (0);
   if (name[i] == 'Z' && name[i - 1] == '.')
      return (1);
#endif
   return (0);
}

/***********************************************************************
 			     GENOPEN()
************************************************************************/

/* genopen() - fopen regular or popen compressed file for task 0 only */

static FILE *genopen(char *name)
{
   struct stat buf;
   char cbuf[120], pcmd[120];
   int length, ier;
   FILE *fp;

   length = strlen(name);
   compressed = iscompressed(name);
   strcpy(cbuf, name);

#if !defined(SPEC)
   /* If file doesn't exist, maybe it has been compressed/decompressed. */

   if (stat(cbuf, &buf) == -1) {
      switch (errno) {
      case ENOENT:{
            if (!compressed) {
               strcat(cbuf, ".Z");
               if (stat(cbuf, &buf) == -1) {
                  if (get_mytaskid() == 0) {
                     fprintf(nabout, "%s, %s: does not exist\n", name,
                             cbuf);
                  }
                  return (NULL);
               }
               compressed++;
               strcat(name, ".Z");      /* TODO: add protection */
            } else {
               cbuf[length - 2] = '\0';
               if (stat(cbuf, &buf) == -1) {
                  if (get_mytaskid() == 0) {
                     fprintf(nabout, "%s, %s: does not exist\n", name,
                             cbuf);
                  }
                  return (NULL);
               }
               compressed = 0;
            }
            break;
         }
      default:
         if (get_mytaskid() == 0) {
            fprintf(nabout, "%s: sys err\n", name);
         }
         return (NULL);
      }
   }

   /* Open the compressed file. */

   if (compressed) {
      sprintf(pcmd, "zcat %s", cbuf);
      ier = 0;
      if (get_mytaskid() == 0) {
         if ((fp = popen(pcmd, "r")) == NULL) {
            perror(pcmd);
            ier = -1;
         }
      }
      reducerror(ier);

      /* The file is open.  Set fp to NULL for all tasks but task 0. */

      if (get_mytaskid() != 0) {
         fp = NULL;
      }

      /* Open the uncompressed file. */

   } else {
#else
    {
#endif /* SPEC */
      ier = 0;
      if (get_mytaskid() == 0) {
         if ((fp = fopen(cbuf, "r")) == NULL) {
            perror(cbuf);
            ier = -1;
         }
      }
      reducerror(ier);

      /* The file is open.  Set fp to NULL for all tasks but task 0. */

      if (get_mytaskid() != 0) {
         fp = NULL;
      }
   }
   return (fp);
}

/***********************************************************************
 			     GENCLOSE()
************************************************************************/

/* genclose() - close fopened or popened file for task 0 only */

static void genclose(FILE * fileptr, int popn)
{
   if (get_mytaskid() != 0) {
      return;
   }
#if !defined(SPEC)
   if (popn) {
      if (pclose(fileptr) == -1)
         perror("pclose");
   } else {
#else
   {
#endif /* SPEC */
      if (fclose(fileptr) == -1)
         perror("fclose");
   }
}


/***********************************************************************
 			     GET()
************************************************************************/

#ifdef MORT
char *get(size_t size)
#else
static char *get(size_t size)
#endif
{
   char *ptr;

   if (size == 0) {
      return ((char *) NULL);
   }
   if ((ptr = (char *) malloc(size * sizeof(char))) == NULL) {
      if (get_mytaskid() == 0) {
         fprintf(nabout, "malloc %lu\n", size);
         fflush(nabout);
         perror("malloc err:");
      }
      reducerror(-1);
   }
   return (ptr);
}

/***********************************************************************
 			     PREADLN()
************************************************************************/

/* preadln() - read to end-of-line; replace \n with \0 */

static void preadln(FILE * file, char *name, char *string)
{
   int i, j, ier;

   ier = 0;
   if (get_mytaskid() == 0) {
      for (i = 0; i < 81; i++) {
         if ((j = getc(file)) == EOF) {
            fprintf(nabout, "Error: unexpected EOF in %s\n", name);
            ier = -1;
         } else {
            string[i] = (char) j;
            if (string[i] == '\n') {
               string[i] = '\0';
               break;
            }
         }
      }
   }
   reducerror(ier);
   ier = 0;
   if (get_mytaskid() == 0) {
      if ((i == 81) && (string[i] != '\0')) {
         fprintf(nabout, "Error: line too long in %s:\n%.81s", name,
                 string);
         ier = -1;
      }
   }
   reducerror(ier);
#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(&i, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(string, i, MPI_CHAR, 0, MPI_COMM_WORLD);
#endif
}

/***********************************************************************
 			     PFIND()
************************************************************************/

/* pfind() - find next section in the prmtop file */

static void pfind(FILE * file, int newparm, char *label)
{
   char line[81];

   if (!newparm)
      return;

   /* First check if the next section is immediately next in the file. */

   preadln(file, label, line);
   if (!strncmp(line + 6, label, strlen(label))) {
      preadln(file, label, line);       /* skip the format line  */
      return;
   }

   /* Section not found on next line; rewind and search. */

   if (get_mytaskid() == 0) {
      rewind(file);
   }
   while (1) {
      preadln(file, label, line);
      if (!strncmp(line + 6, label, strlen(label))) {
         preadln(file, label, line);    /* skip the format line  */
         return;
      }
   }
}

/***************************************************************************
			     READPARM()
****************************************************************************/

/*
 * readparm() - instantiate a given PARMSTRUCT_T
 */

#ifdef MORT
PARMSTRUCT_T *readparm(char *name)
#else
int readparm(MOLECULE_T * mol, char *name)
#endif
{
   REAL_T *H, *atype, sigmaw3, sigma_iw6, epsilon_iw;
   REAL_T si_tmp;
   int i, j, k, idum, res, ifpert, iat, kat, lat, newparm, ier, iaci;
   int ismall, ibig;
   int *iptmp;
   FILE *file;
   PARMSTRUCT_T *prm;
   char line[81];
   char atsymb, atsymbp;
   int i10_12 = 0;
#ifndef MORT
   int ai;
   ATOM_T *a;
#endif

   /* Open the input file.  The result is NULL for all but task 0. */

   if (get_mytaskid() == 0) {
      fprintf(nabout, "Reading .prm file (%s)\n", name);
   }
   ier = 0;
   if ((file = genopen(name)) == NULL) {
      if (get_mytaskid() == 0) {
         fprintf(stderr, "Cannot read parm file %s\n", name);
         ier = -1;
      }
   }
   reducerror(ier);

   prm = (PARMSTRUCT_T *) get(sizeof(PARMSTRUCT_T));

   /*  Read title, and see if this is a "new" format parm file:    */

   preadln(file, name, line);
   newparm = 0;
   if (!strncmp(line, "%VERSION", 8)) {
      newparm = 1;
      pfind(file, newparm, "TITLE");
      preadln(file, name, prm->ititl);
   } else {
      strncpy(prm->ititl, line, 81);
   }
   if (get_mytaskid() == 0) {
      fprintf(nabout, "title:\n%s\n", prm->ititl);
   }

   /* READ CONTROL INTEGERS */

   pfind(file, newparm, "POINTERS");

   if (get_mytaskid() == 0) {
      fscanf(file,
             "%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d",
             &prm->Natom, &prm->Ntypes, &prm->Nbonh, &prm->Mbona,
             &prm->Ntheth, &prm->Mtheta, &prm->Nphih, &prm->Mphia,
             &prm->Nhparm, &prm->Nparm, &prm->Nnb, &prm->Nres, &prm->Nbona,
             &prm->Ntheta, &prm->Nphia, &prm->Numbnd, &prm->Numang,
             &prm->Nptra, &prm->Natyp, &prm->Nphb, &ifpert, &idum, &idum,
             &idum, &idum, &idum, &idum, &prm->IfBox, &prm->Nmxrs,
             &prm->IfCap);
   }
#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(&prm->Natom, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Ntypes, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Nbonh, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Mbona, 1, MPI_INT, 0, MPI_COMM_WORLD);

   MPI_Bcast(&prm->Ntheth, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Mtheta, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Nphih, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Mphia, 1, MPI_INT, 0, MPI_COMM_WORLD);

   MPI_Bcast(&prm->Nhparm, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Nparm, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Nnb, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Nres, 1, MPI_INT, 0, MPI_COMM_WORLD);

   MPI_Bcast(&prm->Nbona, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Ntheta, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Nphia, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Numbnd, 1, MPI_INT, 0, MPI_COMM_WORLD);

   MPI_Bcast(&prm->Numang, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Nptra, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Natyp, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Nphb, 1, MPI_INT, 0, MPI_COMM_WORLD);

   MPI_Bcast(&ifpert, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->IfBox, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->Nmxrs, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(&prm->IfCap, 1, MPI_INT, 0, MPI_COMM_WORLD);

   MPI_Bcast(&idum, 1, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   if (newparm) {
      if (get_mytaskid() == 0) {
         fscanf(file, "%d", &prm->Numextra);
      }
#if defined(MPI) || defined(SCALAPACK)
      MPI_Bcast(&prm->Numextra, 1, MPI_INT, 0, MPI_COMM_WORLD);
#endif
   }
   skipeoln(file);

   /* ALLOCATE MEMORY */

   prm->Nat3 = 3 * prm->Natom;
   prm->Ntype2d = prm->Ntypes * prm->Ntypes;
   prm->Nttyp = prm->Ntypes * (prm->Ntypes + 1) / 2;

   /*
    * get most of the indirect stuff; some extra allowed for char arrays
    */

   prm->AtomNames = (char *) get((size_t) 4 * prm->Natom + 81);
   prm->Charges = (REAL_T *) get(sizeof(REAL_T) * prm->Natom);
   prm->Masses = (REAL_T *) get(sizeof(REAL_T) * prm->Natom);
   prm->Iac = (int *) get(sizeof(int) * prm->Natom);
   prm->Iblo = (int *) get(sizeof(int) * prm->Natom);
   prm->Cno = (int *) get(sizeof(int) * prm->Ntype2d);
   prm->ResNames = (char *) get((size_t) 4 * prm->Nres + 81);
   prm->Ipres = (int *) get(sizeof(int) * (prm->Nres + 1));
   prm->Rk = (REAL_T *) get(sizeof(REAL_T) * prm->Numbnd);
   prm->Req = (REAL_T *) get(sizeof(REAL_T) * prm->Numbnd);
   prm->Tk = (REAL_T *) get(sizeof(REAL_T) * prm->Numang);
   prm->Teq = (REAL_T *) get(sizeof(REAL_T) * prm->Numang);
   prm->Pk = (REAL_T *) get(sizeof(REAL_T) * prm->Nptra);
   prm->Pn = (REAL_T *) get(sizeof(REAL_T) * prm->Nptra);
   prm->Phase = (REAL_T *) get(sizeof(REAL_T) * prm->Nptra);
   prm->Solty = (REAL_T *) get(sizeof(REAL_T) * prm->Natyp);
   prm->Cn1 = (REAL_T *) get(sizeof(REAL_T) * prm->Nttyp);
   prm->Cn2 = (REAL_T *) get(sizeof(REAL_T) * prm->Nttyp);
   prm->BondHAt1 = (int *) get(sizeof(int) * prm->Nbonh);
   prm->BondHAt2 = (int *) get(sizeof(int) * prm->Nbonh);
   prm->BondHNum = (int *) get(sizeof(int) * prm->Nbonh);
   prm->BondAt1 = (int *) get(sizeof(int) * prm->Nbona);
   prm->BondAt2 = (int *) get(sizeof(int) * prm->Nbona);
   prm->BondNum = (int *) get(sizeof(int) * prm->Nbona);
   prm->AngleHAt1 = (int *) get(sizeof(int) * prm->Ntheth);
   prm->AngleHAt2 = (int *) get(sizeof(int) * prm->Ntheth);
   prm->AngleHAt3 = (int *) get(sizeof(int) * prm->Ntheth);
   prm->AngleHNum = (int *) get(sizeof(int) * prm->Ntheth);
   prm->AngleAt1 = (int *) get(sizeof(int) * prm->Ntheta);
   prm->AngleAt2 = (int *) get(sizeof(int) * prm->Ntheta);
   prm->AngleAt3 = (int *) get(sizeof(int) * prm->Ntheta);
   prm->AngleNum = (int *) get(sizeof(int) * prm->Ntheta);
   prm->DihHAt1 = (int *) get(sizeof(int) * prm->Nphih);
   prm->DihHAt2 = (int *) get(sizeof(int) * prm->Nphih);
   prm->DihHAt3 = (int *) get(sizeof(int) * prm->Nphih);
   prm->DihHAt4 = (int *) get(sizeof(int) * prm->Nphih);
   prm->DihHNum = (int *) get(sizeof(int) * prm->Nphih);
   prm->DihAt1 = (int *) get(sizeof(int) * prm->Nphia);
   prm->DihAt2 = (int *) get(sizeof(int) * prm->Nphia);
   prm->DihAt3 = (int *) get(sizeof(int) * prm->Nphia);
   prm->DihAt4 = (int *) get(sizeof(int) * prm->Nphia);
   prm->DihNum = (int *) get(sizeof(int) * prm->Nphia);
   prm->ExclAt = (int *) get(sizeof(int) * prm->Nnb);
   prm->HB12 = (REAL_T *) get(sizeof(REAL_T) * prm->Nphb);
   prm->HB10 = (REAL_T *) get(sizeof(REAL_T) * prm->Nphb);
   prm->AtomSym = (char *) get((size_t) 4 * prm->Natom + 81);
   prm->AtomTree = (char *) get((size_t) 4 * prm->Natom + 81);
   prm->TreeJoin = (int *) get(sizeof(int) * prm->Natom);
   prm->AtomRes = (int *) get(sizeof(int) * prm->Natom);
   prm->N14pairs = (int *) get(sizeof(int) * prm->Natom);
   prm->N14pairlist = (int *) get(sizeof(int) * 10 * prm->Natom);
   iptmp = (int *) get(sizeof(int) * 12 * prm->Natom);
   prm->Rborn = (REAL_T *) get(sizeof(REAL_T) * prm->Natom);
   prm->Fs = (REAL_T *) get(sizeof(REAL_T) * prm->Natom);
   prm->Gvdw = (REAL_T *) get(sizeof(REAL_T) * prm->Natom);

   /* 
    * READ ATOM NAMES -IH(M04)
    */

   pfind(file, newparm, "ATOM_NAME");
   for (i = 0; i < (prm->Natom / 20 + (prm->Natom % 20 ? 1 : 0)); i++)
      preadln(file, "", &prm->AtomNames[i * 80]);

   /* 
    * READ ATOM CHARGES -X(L15)
    *    (pre-multiplied by an energy factor of 18.2223 == sqrt(332)
    *     for faster force field calculations)
    */

   pfind(file, newparm, "CHARGE");
   for (i = 0; i < prm->Natom; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf", &prm->Charges[i]);
#else
         fscanf(file, " %f", &prm->Charges[i]);
#endif
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Charges, prm->Natom, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ ATOM MASSES -X(L20)
    */

   pfind(file, newparm, "MASS");
   for (i = 0; i < prm->Natom; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %le", &prm->Masses[i]);
#else
         fscanf(file, " %e", &prm->Masses[i]);
#endif
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Masses, prm->Natom, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ ATOM L-J TYPES -IX(I04)
    */

   pfind(file, newparm, "ATOM_TYPE_INDEX");
   for (i = 0; i < prm->Natom; i++) {
      if (get_mytaskid() == 0) {
         fscanf(file, " %d", &prm->Iac[i]);
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Iac, prm->Natom, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ ATOM INDEX TO 1st IN EXCLUDED ATOM LIST "NATEX" -IX(I08)
    */

   pfind(file, newparm, "NUMBER_EXCLUDED_ATOMS");
   for (i = 0; i < prm->Natom; i++) {
      if (get_mytaskid() == 0) {
         fscanf(file, " %d", &prm->Iblo[i]);
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Iblo, prm->Natom, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ TYPE INDEX TO N-B TYPE -IX(I06)
    */

   pfind(file, newparm, "NONBONDED_PARM_INDEX");
   for (i = 0; i < prm->Ntype2d; i++) {
      if (get_mytaskid() == 0) {
         fscanf(file, " %d", &prm->Cno[i]);
         if (prm->Cno[i] < 0 && !i10_12) {
            fprintf(nabout,
                    "     Parameter topology includes 10-12 terms:\n");
            fprintf(nabout,
                    "     These are assumed to be zero here (e.g. from TIP3P water)\n");
            i10_12 = 1;
         }
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Cno, prm->Ntype2d, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ RES NAMES (4 chars each, 4th blank) -IH(M02)
    */

   pfind(file, newparm, "RESIDUE_LABEL");
   for (i = 0; i < (prm->Nres / 20 + (prm->Nres % 20 ? 1 : 0)); i++)
      preadln(file, "", &prm->ResNames[i * 80]);

   /* 
    * READ RES POINTERS TO 1st ATOM              -IX(I02)
    */

   pfind(file, newparm, "RESIDUE_POINTER");
   for (i = 0; i < prm->Nres; i++) {
      if (get_mytaskid() == 0) {
         fscanf(file, " %d", &prm->Ipres[i]);
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Ipres, prm->Nres, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   prm->Ipres[prm->Nres] = prm->Natom + 1;
   skipeoln(file);

   /* 
    * READ BOND FORCE CONSTANTS                  -RK()
    */

   pfind(file, newparm, "BOND_FORCE_CONSTANT");
   for (i = 0; i < prm->Numbnd; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf", &prm->Rk[i]);
#else
         fscanf(file, " %f", &prm->Rk[i]);
#endif
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Rk, prm->Numbnd, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ BOND LENGTH OF MINIMUM ENERGY                 -REQ()
    */

   pfind(file, newparm, "BOND_EQUIL_VALUE");
   for (i = 0; i < prm->Numbnd; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf", &prm->Req[i]);
#else
         fscanf(file, " %f", &prm->Req[i]);
#endif
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Req, prm->Numbnd, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ BOND ANGLE FORCE CONSTANTS (following Rk nomen) -TK()
    */

   pfind(file, newparm, "ANGLE_FORCE_CONSTANT");
   for (i = 0; i < prm->Numang; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf", &prm->Tk[i]);
#else
         fscanf(file, " %f", &prm->Tk[i]);
#endif
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Tk, prm->Numang, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ BOND ANGLE OF MINIMUM ENERGY (following Req nomen) -TEQ()
    */

   pfind(file, newparm, "ANGLE_EQUIL_VALUE");
   for (i = 0; i < prm->Numang; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf", &prm->Teq[i]);
#else
         fscanf(file, " %f", &prm->Teq[i]);
#endif
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Teq, prm->Numang, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ DIHEDRAL PEAK MAGNITUDE               -PK()
    */

   pfind(file, newparm, "DIHEDRAL_FORCE_CONSTANT");
   for (i = 0; i < prm->Nptra; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf", &prm->Pk[i]);
#else
         fscanf(file, " %f", &prm->Pk[i]);
#endif
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Pk, prm->Nptra, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ DIHEDRAL PERIODICITY                  -PN()
    */

   pfind(file, newparm, "DIHEDRAL_PERIODICITY");
   for (i = 0; i < prm->Nptra; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf", &prm->Pn[i]);
#else
         fscanf(file, " %f", &prm->Pn[i]);
#endif
         if( prm->Pn[i] == 0 ){
            fprintf( stderr, 
               "Found an invalid periodicity in the prmtop file: %d\n", i );
            exit(1);
         }
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Pn, prm->Nptra, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ DIHEDRAL PHASE                        -PHASE()
    */

   pfind(file, newparm, "DIHEDRAL_PHASE");
   for (i = 0; i < prm->Nptra; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf", &prm->Phase[i]);
#else
         fscanf(file, " %f", &prm->Phase[i]);
#endif
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Phase, prm->Nptra, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * ?? "RESERVED"                              -SOLTY()
    */

   pfind(file, newparm, "SOLTY");
   for (i = 0; i < prm->Natyp; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf", &prm->Solty[i]);
#else
         fscanf(file, " %f", &prm->Solty[i]);
#endif
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Solty, prm->Natyp, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ L-J R**12 FOR ALL PAIRS OF ATOM TYPES         -CN1()
    *    (SHOULD BE 0 WHERE H-BONDS)
    */

   pfind(file, newparm, "LENNARD_JONES_ACOEF");
   for (i = 0; i < prm->Nttyp; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf", &prm->Cn1[i]);
#else
         fscanf(file, " %f", &prm->Cn1[i]);
#endif
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Cn1, prm->Nttyp, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ L-J R**6 FOR ALL PAIRS OF ATOM TYPES  -CN2()
    *    (SHOULD BE 0 WHERE H-BONDS)
    */

   pfind(file, newparm, "LENNARD_JONES_BCOEF");
   for (i = 0; i < prm->Nttyp; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf", &prm->Cn2[i]);
#else
         fscanf(file, " %f", &prm->Cn2[i]);
#endif
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->Cn2, prm->Nttyp, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ COVALENT BOND W/ HYDROGEN (3*(atnum-1)): 
    *    IBH = ATOM1             -IX(I12)
    *    JBH = ATOM2             -IX(I14)
    *    ICBH = BOND ARRAY PTR   -IX(I16)
    */

   pfind(file, newparm, "BONDS_INC_HYDROGEN");
   for (i = 0; i < prm->Nbonh; i++) {
      if (get_mytaskid() == 0) {
         fscanf(file, " %d %d %d",
                &prm->BondHAt1[i], &prm->BondHAt2[i], &prm->BondHNum[i]);
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->BondHAt1, prm->Nbonh, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->BondHAt2, prm->Nbonh, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->BondHNum, prm->Nbonh, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ COVALENT BOND W/OUT HYDROGEN (3*(atnum-1)):
    *    IB = ATOM1              -IX(I18)
    *    JB = ATOM2              -IX(I20)
    *    ICB = BOND ARRAY PTR    -IX(I22)
    */

   pfind(file, newparm, "BONDS_WITHOUT_HYDROGEN");
   for (i = 0; i < prm->Nbona; i++) {
      if (get_mytaskid() == 0) {
         fscanf(file, " %d %d %d",
                &prm->BondAt1[i], &prm->BondAt2[i], &prm->BondNum[i]);
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->BondAt1, prm->Nbona, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->BondAt2, prm->Nbona, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->BondNum, prm->Nbona, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ ANGLE W/ HYDROGEN: 
    *    ITH = ATOM1                     -IX(I24)
    *    JTH = ATOM2                     -IX(I26)
    *    KTH = ATOM3                     -IX(I28)
    *    ICTH = ANGLE ARRAY PTR          -IX(I30)
    */

   pfind(file, newparm, "ANGLES_INC_HYDROGEN");
   for (i = 0; i < prm->Ntheth; i++) {
      if (get_mytaskid() == 0) {
         fscanf(file, " %d %d %d %d",
                &prm->AngleHAt1[i], &prm->AngleHAt2[i],
                &prm->AngleHAt3[i], &prm->AngleHNum[i]);
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->AngleHAt1, prm->Ntheth, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->AngleHAt2, prm->Ntheth, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->AngleHAt3, prm->Ntheth, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->AngleHNum, prm->Ntheth, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ ANGLE W/OUT HYDROGEN: 
    *    IT = ATOM1                      -IX(I32)
    *    JT = ATOM2                      -IX(I34)
    *    KT = ATOM3                      -IX(I36)
    *    ICT = ANGLE ARRAY PTR           -IX(I38)
    */

   pfind(file, newparm, "ANGLES_WITHOUT_HYDROGEN");
   for (i = 0; i < prm->Ntheta; i++) {
      if (get_mytaskid() == 0) {
         fscanf(file, " %d %d %d %d",
                &prm->AngleAt1[i], &prm->AngleAt2[i],
                &prm->AngleAt3[i], &prm->AngleNum[i]);
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->AngleAt1, prm->Ntheta, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->AngleAt2, prm->Ntheta, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->AngleAt3, prm->Ntheta, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->AngleNum, prm->Ntheta, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ DIHEDRAL W/ HYDROGEN: 
    *    ITH = ATOM1                     -IX(40)
    *    JTH = ATOM2                     -IX(42)
    *    KTH = ATOM3                     -IX(44)
    *    LTH = ATOM4                     -IX(46)
    *    ICTH = DIHEDRAL ARRAY PTR       -IX(48)
    */

   pfind(file, newparm, "DIHEDRALS_INC_HYDROGEN");
   for (i = 0; i < prm->Nphih; i++) {
      if (get_mytaskid() == 0) {
         fscanf(file, " %d %d %d %d %d",
                &prm->DihHAt1[i], &prm->DihHAt2[i], &prm->DihHAt3[i],
                &prm->DihHAt4[i], &prm->DihHNum[i]);
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->DihHAt1, prm->Nphih, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->DihHAt2, prm->Nphih, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->DihHAt3, prm->Nphih, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->DihHAt4, prm->Nphih, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->DihHNum, prm->Nphih, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /* 
    * READ DIHEDRAL W/OUT HYDROGEN: 
    *    IT = ATOM1
    *    JT = ATOM2
    *    KT = ATOM3
    *    LT = ATOM4
    *    ICT = DIHEDRAL ARRAY PTR
    */

   pfind(file, newparm, "DIHEDRALS_WITHOUT_HYDROGEN");
   for (i = 0; i < prm->Nphia; i++) {
      if (get_mytaskid() == 0) {
         fscanf(file, " %d %d %d %d %d",
                &prm->DihAt1[i], &prm->DihAt2[i], &prm->DihAt3[i],
                &prm->DihAt4[i], &prm->DihNum[i]);
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->DihAt1, prm->Nphia, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->DihAt2, prm->Nphia, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->DihAt3, prm->Nphia, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->DihAt4, prm->Nphia, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast(prm->DihNum, prm->Nphia, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /*
    * READ EXCLUDED ATOM LIST    -IX(I10)
    */

   pfind(file, newparm, "EXCLUDED_ATOMS_LIST");
   for (i = 0; i < prm->Nnb; i++) {
      if (get_mytaskid() == 0) {
         fscanf(file, " %d", &prm->ExclAt[i]);
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->ExclAt, prm->Nnb, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /*
    * READ H-BOND R**12 TERM FOR ALL N-B TYPES   -ASOL()
    */

   pfind(file, newparm, "HBOND_ACOEF");
   for (i = 0; i < prm->Nphb; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf", &prm->HB12[i]);
#else
         fscanf(file, " %f", &prm->HB12[i]);
#endif
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->HB12, prm->Nphb, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /*
    * READ H-BOND R**10 TERM FOR ALL N-B TYPES   -BSOL()
    */

   pfind(file, newparm, "HBOND_BCOEF");
   for (i = 0; i < prm->Nphb; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf", &prm->HB10[i]);
#else
         fscanf(file, " %f", &prm->HB10[i]);
#endif
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->HB10, prm->Nphb, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /*
    * READ H-BOND CUTOFF (NOT USED) ??           -HBCUT()
    */

   pfind(file, newparm, "HBCUT");
   H = (REAL_T *) get(prm->Nphb * sizeof(REAL_T));
   for (i = 0; i < prm->Nphb; i++) {
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf", &H[i]);
#else
         fscanf(file, " %f", &H[i]);
#endif
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(H, prm->Nphb, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

   if (H)
      free((char *) H);

   skipeoln(file);

   /*
    * READ ATOM SYMBOLS (FOR ANALYSIS PROGS)     -IH(M06)
    */

   pfind(file, newparm, "AMBER_ATOM_TYPE");
   for (i = 0; i < (prm->Natom / 20 + (prm->Natom % 20 ? 1 : 0)); i++)
      preadln(file, "", &prm->AtomSym[i * 80]);

   /*
    * READ TREE SYMBOLS (FOR ANALYSIS PROGS)     -IH(M08)
    */

   pfind(file, newparm, "TREE_CHAIN_CLASSIFICATION");
   for (i = 0; i < (prm->Natom / 20 + (prm->Natom % 20 ? 1 : 0)); i++)
      preadln(file, "", &prm->AtomTree[i * 80]);

   /*
    * READ TREE JOIN INFO (FOR ANALYSIS PROGS)   -IX(I64)
    */

   pfind(file, newparm, "JOIN_ARRAY");
   for (i = 0; i < prm->Natom; i++) {
      if (get_mytaskid() == 0) {
         fscanf(file, " %d", &prm->TreeJoin[i]);
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->TreeJoin, prm->Natom, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);

   /*
    * READ PER-ATOM RES NUMBER                   -IX(I66)
    *    NOTE: this appears to be something entirely different
    *    NOTE: overwriting this with correct PER-ATOM RES NUMBERs
    */

   pfind(file, newparm, "IROTAT");
   for (i = 0; i < prm->Natom; i++) {
      if (get_mytaskid() == 0) {
         fscanf(file, " %d", &prm->AtomRes[i]);
      }
   }

#if defined(MPI) || defined(SCALAPACK)
   MPI_Bcast(prm->AtomRes, prm->Natom, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   skipeoln(file);
   res = 0;
   for (i = 0; i < prm->Natom; i++) {
      if (i + 1 == prm->Ipres[res + 1]) /* atom is 1st of next res */
         res++;
      prm->AtomRes[i] = res;
   }

   /*
    * BOUNDARY CONDITION STUFF
    */

   if (!prm->IfBox) {
      prm->Nspm = 1;
      prm->Boundary = (int *) get(sizeof(int) * prm->Nspm);
      prm->Boundary[0] = prm->Natom;
   } else {
      if (get_mytaskid() == 0) {
         fprintf(nabout, "periodic prmtop found, not supported by NAB\n");
      }
      exit(1);
#if 0
      /*  (potentially for later use:)  */

      pfind(file, newparm, "SOLVENT_POINTERS");
      if (get_mytaskid() == 0) {
         fscanf(file, " %d %d %d", &prm->Iptres, &prm->Nspm, &prm->Nspsol);
      }
#if defined(MPI) || defined(SCALAPACK)
      MPI_Bcast(&prm->Iptres, 1, MPI_INT, 0, MPI_COMM_WORLD);
      MPI_Bcast(&prm->Nspm, 1, MPI_INT, 0, MPI_COMM_WORLD);
      MPI_Bcast(&prm->Nspsol, 1, MPI_INT, 0, MPI_COMM_WORLD);
#endif
      skipeoln(file);
      prm->Boundary = (int *) get(sizeof(int) * prm->Nspm);

      pfind(file, newparm, "ATOMS_PER_MOLECULE");
      for (i = 0; i < prm->Nspm; i++) {
         if (get_mytaskid() == 0) {
            fscanf(file, " %d", &prm->Boundary[i]);
         }
      }

#if defined(MPI) || defined(SCALAPACK)
      MPI_Bcast(prm->Boundary, prm->Nspm, MPI_INT, 0, MPI_COMM_WORLD);
#endif
      skipeoln(file);

      pfind(file, newparm, "BOX_DIMENSIONS");
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf %lf %lf",
                &prm->Box[0], &prm->Box[1], &prm->Box[2]);
#else
         fscanf(file, " %f %f %f",
                &prm->Box[0], &prm->Box[1], &prm->Box[2]);
#endif
      }
#if defined(MPI) || defined(SCALAPACK)
      MPI_Bcast(prm->Box, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif
      skipeoln(file);
      if (prm->Iptres)
         prm->Ipatm = prm->Ipres[prm->Iptres] - 1;
      /* IF(IPTRES.GT.0) IPTATM = IX(I02+IPTRES-1+1)-1 */
#endif
   }

   /*
    * ----- LOAD THE CAP INFORMATION IF NEEDED -----
    */

   if (prm->IfCap) {

      pfind(file, newparm, "CAP_INFO");
      if (get_mytaskid() == 0) {
         fscanf(file, " %d ", &prm->Natcap);
      }
#if defined(MPI) || defined(SCALAPACK)
      MPI_Bcast(&prm->Natcap, 1, MPI_INT, 0, MPI_COMM_WORLD);
#endif

      pfind(file, newparm, "CAP_INFO2");
      if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
         fscanf(file, " %lf %lf %lf %lf",
                &prm->Cutcap, &prm->Xcap, &prm->Ycap, &prm->Zcap);
#else
         fscanf(file, " %f %f %f %f",
                &prm->Cutcap, &prm->Xcap, &prm->Ycap, &prm->Zcap);
#endif
      }
#if defined(MPI) || defined(SCALAPACK)
      MPI_Bcast(&prm->Cutcap, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
      MPI_Bcast(&prm->Xcap, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
      MPI_Bcast(&prm->Ycap, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
      MPI_Bcast(&prm->Zcap, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif
   }


   /*
    * --- GENERALIZED BORN PARAMETERS  -----
    */
#define BOFFSET 0.09
   if (newparm) {

      pfind(file, newparm, "RADII");
      for (i = 0; i < prm->Natom; i++) {
         if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
            fscanf(file, " %lf", &prm->Rborn[i]);
#else
            fscanf(file, " %f", &prm->Rborn[i]);
#endif
         }
      }
#if defined(MPI) || defined(SCALAPACK)
      MPI_Bcast(prm->Rborn, prm->Natom, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif
      skipeoln(file);

      pfind(file, newparm, "SCREEN");
      for (i = 0; i < prm->Natom; i++) {
         if (get_mytaskid() == 0) {
#ifdef NAB_DOUBLE_PRECISION
            fscanf(file, " %lf", &prm->Fs[i]);
#else
            fscanf(file, " %f", &prm->Fs[i]);
#endif
         }
      }
#if defined(MPI) || defined(SCALAPACK)
      MPI_Bcast(prm->Fs, prm->Natom, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif
      skipeoln(file);
      prm->Fsmax = 0.0;
      for (i = 0; i < prm->Natom; i++) {
         si_tmp = prm->Fs[i] * (prm->Rborn[i] - BOFFSET);
         prm->Fsmax = si_tmp > prm->Fsmax ? si_tmp : prm->Fsmax;
      }

   } else {

      /* use old algorithm to get GB parameters; only for now, for
       * backwards compatibility....
       */

      if (get_mytaskid() == 0) {
         fprintf(nabout,
                 "old prmtop format => using old algorithm for GB parms\n");
      }
      prm->Fsmax = 0.0;
      for (i = 0; i < prm->Natom; i++) {

         /*
          **               set the radii scale factors for the approximate pairwise method 
          **                               prm->Rborn[] values below are from the Bondi set of radii; 
          **              The prm->Fs[] are
          **              scale factors taken from Jay Ponder's tinker program.
          */

         atsymb = prm->AtomNames[4 * i];        /* first letter atom name */

         if (atsymb == 'H') {
            prm->Fs[i] = 0.85;
            /* find the type of atom this is bonded to: */
            if (i == 0) {
               atsymbp = 'O';
            } else {
               for (j = 1; j <= 3; j++) {
                  atsymbp = prm->AtomNames[4 * (i - j)];
                  if (atsymbp != 'H')
                     break;
               }
            }
            if (atsymbp == 'O')
               prm->Rborn[i] = 0.8;
            else if (atsymbp == 'N')
               prm->Rborn[i] = 1.2;
            else if (atsymbp == 'C')
               prm->Rborn[i] = 1.3;
            else
               prm->Rborn[i] = 1.2;
         }

         else if (atsymb == 'C') {
            prm->Fs[i] = 0.72;
            prm->Rborn[i] = 1.70;
         } else if (atsymb == 'N') {
            prm->Fs[i] = 0.79;
            prm->Rborn[i] = 1.55;
         } else if (atsymb == 'O') {
            prm->Fs[i] = 0.85;
            prm->Rborn[i] = 1.50;
         } else if (atsymb == 'F') {
            prm->Fs[i] = 0.88;
            prm->Rborn[i] = 1.47;
         } else if (atsymb == 'P') {
            prm->Fs[i] = 0.86;
            prm->Rborn[i] = 1.85;
         } else if (atsymb == 'S') {
            prm->Fs[i] = 0.96;
            prm->Rborn[i] = 1.80;
         } else if (atsymb == 'L') {
            prm->Fs[i] = 0.96;
            prm->Rborn[i] = 1.00;
         } else if (atsymb == 'Z') {
            prm->Fs[i] = 0.96;
            prm->Rborn[i] = 1.40;
         } else if (atsymb == 'M') {
            prm->Fs[i] = 0.96;
            prm->Rborn[i] = 1.40;
         } else {
            if (get_mytaskid() == 0) {
               fprintf(stderr, "bad atom symbol: %d, %c\n", i, atsymb);
            }
            exit(1);
         }
         si_tmp = prm->Fs[i] * (prm->Rborn[i] - BOFFSET);
         prm->Fsmax = si_tmp > prm->Fsmax ? si_tmp : prm->Fsmax;

      }
   }

   genclose(file, compressed);

   /*  AGBNP parameters   */

   /*  
    *  Get an array of parameters a-sub-i from Eq. 30 of Gallicchio & Levy;
    *  Store this in prm->Gvdw[i].
    *
    *  First, get the a-sub-i parameters indexed by atom types,
    *  using Eqs. 31-33:
    */

#define SIGMAW (3.15365)
#define EPSILONW (0.155)
#define RHOW (0.33428)
#define PI (3.141592650)

   atype = (REAL_T *) get(sizeof(REAL_T) * prm->Ntypes);
   sigmaw3 = SIGMAW * SIGMAW * SIGMAW;
   for (i = 0; i < prm->Ntypes; i++) {
      iaci = prm->Cno[prm->Ntypes * i + i] - 1;
      if (prm->Cn1[iaci] == 0.0 || prm->Cn2[iaci] == 0.0) {
         atype[i] = 0.0;
      } else {
         sigma_iw6 = sigmaw3 * sqrt(prm->Cn1[iaci] / prm->Cn2[iaci]);
         epsilon_iw =
             0.5 * sqrt(EPSILONW / prm->Cn1[iaci]) * prm->Cn2[iaci];
         atype[i] = -16. * PI * RHOW * epsilon_iw * sigma_iw6 / 3.;
#if 0
         fprintf(stderr, "%5d  %15.8f  %15.8f  %15.8f\n", iaci, epsilon_iw,
                 pow(sigma_iw6, 1. / 6.), atype[i]);
#endif
      }
   }

   /*
    *  Now, use these to fill in an array indexed by atom number:
    */

   for (i = 0; i < prm->Natom; i++) {
      prm->Gvdw[i] = atype[prm->Iac[i] - 1];
   }
   free((char *) atype);

#if 0
   fprintf(stderr, "Gvdw values:\n");
   for (i = 0; i < prm->Natom; i++) {
      fprintf(stderr, "%5d  %15.8f\n", i + 1, prm->Gvdw[i]);
   }
#endif

   /*
    * -------CONSTRUCT A 1-4 LIST -------
    */

   for (i = 0; i < prm->Natom; i++)
      prm->N14pairs[i] = 0;
   for (i = 0; i < prm->Nphih; i++) {
      iat = prm->DihHAt1[i] / 3;
      kat = prm->DihHAt3[i] / 3;
      lat = prm->DihHAt4[i] / 3;
      if (kat >= 0 && lat >= 0) {
         ismall = iat < lat ? iat : lat;
         ibig = iat > lat ? iat : lat;
         iptmp[12 * ismall + prm->N14pairs[ismall]++] = ibig;
      }
   }
   for (i = 0; i < prm->Mphia; i++) {
      iat = prm->DihAt1[i] / 3;
      kat = prm->DihAt3[i] / 3;
      lat = prm->DihAt4[i] / 3;
      if (kat >= 0 && lat >= 0) {
         ismall = iat < lat ? iat : lat;
         ibig = iat > lat ? iat : lat;
         iptmp[12 * ismall + prm->N14pairs[ismall]++] = ibig;
      }
   }
   idum = 0;
   for (i = 0; i < prm->Natom - 1; i++) {
      for (k = 0; k < prm->N14pairs[i]; k++)
         prm->N14pairlist[idum++] = iptmp[12 * i + k];
   }
#ifdef PRINT_14PAIRS
   if (get_mytaskid() == 0) {
      fprintf(nabout, "npairs:\n");
      for (k = 0; k < prm->Natom; k++) {
         fprintf(nabout, "%4d", prm->N14pairs[k]);
         if ((k + 1) % 20 == 0)
            fprintf(nabout, "\n");
      }
      fprintf(nabout, "\npairlist:\n");
      for (k = 0; k < idum; k++) {
         fprintf(nabout, "%4d", prm->N14pairlist[k]);
         if ((k + 1) % 20 == 0)
            fprintf(nabout, "\n");
      }
      fprintf(nabout, "\n");
   }
#endif
   free(iptmp);

#ifdef MORT
   return (prm);
#else
   mol->m_prm = prm;

   /*  fill in the charge and radii arrays in the molecule with the values
      we just got from the prmtop file:                                  */

   for (ai = 0, a = NULL; a = NAB_mnext(mol, a); ai++) {
      a->a_charge = prm->Charges[ai] / 18.2223;
      a->a_radius = prm->Rborn[ai];
   }

   return (0);
#endif
}

#define INTFORMAT       "%8d"
#define DBLFORMAT       "%16.8lE"
#define LBLFORMAT       "%-4s"
#define IDFORMAT       "%-8s"


/*
 *      Save the format and number of entries per line.
 */
void FortranFormat(int iPerLine, char *sFormat)
{
   SiPerLine = iPerLine;
   strcpy(SsFormat, sFormat);
   SiOnLine = 0;
   SbWroteNothing = TRUE;
}

/*
 *      Write an integer out to the file, increment the number of entries on 
 *      the line, if it is equal to SiPerLine, then move to the next line.
 */
void FortranWriteInt(int iVal)
{
   fprintf(SfFile, SsFormat, iVal);
   SiOnLine++;
   SbWroteNothing = FALSE;
   if (SiOnLine >= SiPerLine) {
      fprintf(SfFile, "\n");
      SiOnLine = 0;
   }
}

/*
 *      Write a double out to the file, increment the number of entries on 
 *      the line, if it is equal to SiPerLine, then move to the next line.
 */
void FortranWriteDouble(double dVal)
{
   fprintf(SfFile, SsFormat, dVal);
   SiOnLine++;
   SbWroteNothing = FALSE;
   if (SiOnLine >= SiPerLine) {
      fprintf(SfFile, "\n");
      SiOnLine = 0;
   }
}

/*
 *      Write a String out to the file, increment the number of entries on 
 *      the line, if it is equal to SiPerLine, then move to the next line.
 */
void FortranWriteString(char *sVal)
{
   fprintf(SfFile, SsFormat, sVal);
   SiOnLine++;
   SbWroteNothing = FALSE;
   if (SiOnLine >= SiPerLine) {
      fprintf(SfFile, "\n");
      SiOnLine = 0;
   }
}

/*
 *      If the number of objects on the line is not zero then print
 *      and end of line.
 */
void FortranEndLine()
{
   if (SbWroteNothing || SiOnLine != 0)
      fprintf(SfFile, "\n");
   SbWroteNothing = TRUE;
   SiOnLine = 0;
}

/***************************************************************************
			     WRITEPARM()
    NB: this routine is not currently used, nor tested!
****************************************************************************/

/*
 * writeparm() - write a PARMSTRUCT_T but only if task zero
 */

#ifdef MORT
int writeparm(PARMSTRUCT_T * prm, char *name)
#else
int writeparm(MOLECULE_T * mol, char *name)
#endif
{
   int i, ier;
   char sVersionHeader[81];
   char tmpchar[5];
   time_t tp;
#ifndef MORT
   PARMSTRUCT_T *prm;

   prm = mol->m_prm;
#endif
   if (prm == NULL) {
      if (get_mytaskid() == 0) {
         fprintf(stderr, "writeparm() sees a NULL parmstruct\n");
      }
      reducerror(-1);
   }

   ier = 0;
   if (get_mytaskid() == 0) {
      if ((SfFile = fopen(name, "w")) == NULL) {
         perror(name);
         ier = -1;
      }
      if (ier >= 0) {

         /* -1- Save the title of the UNIT */
         FortranFormat(1, "%-80s");
         time(&tp);
         strftime(sVersionHeader, 81,
                  "%%VERSION  VERSION_STAMP = V0001.000  DATE = %m/%d/%y  %H:%M:%S\0",
                  localtime(&tp));
         FortranWriteString(sVersionHeader);
         FortranWriteString("%FLAG TITLE");
         FortranWriteString("%FORMAT(20a4)");
         FortranWriteString(prm->ititl);
         FortranWriteString("%FLAG POINTERS");
         FortranWriteString("%FORMAT(10I8)");

         /* -2- Save control information */
         FortranFormat(10, INTFORMAT);

         FortranWriteInt(prm->Natom);
         FortranWriteInt(prm->Ntypes);
         FortranWriteInt(prm->Nbonh);
         FortranWriteInt(prm->Mbona);
         FortranWriteInt(prm->Ntheth);
         FortranWriteInt(prm->Mtheta);
         FortranWriteInt(prm->Nphih);
         FortranWriteInt(prm->Mphia);
         FortranWriteInt(prm->Nhparm);
         FortranWriteInt(prm->Nparm);
         FortranWriteInt(prm->Nnb);
         FortranWriteInt(prm->Nres);
         FortranWriteInt(prm->Nbona);
         FortranWriteInt(prm->Ntheta);
         FortranWriteInt(prm->Nphia);
         FortranWriteInt(prm->Numbnd);
         FortranWriteInt(prm->Numang);
         FortranWriteInt(prm->Nptra);
         FortranWriteInt(prm->Natyp);
         FortranWriteInt(prm->Nphb);
         FortranWriteInt(0);
         FortranWriteInt(0);
         FortranWriteInt(0);
         FortranWriteInt(0);
         FortranWriteInt(0);
         FortranWriteInt(0);
         FortranWriteInt(0);
         FortranWriteInt(prm->IfBox);
         FortranWriteInt(prm->Nmxrs);
         FortranWriteInt(prm->IfCap);
         FortranWriteInt(prm->Numextra);

         FortranEndLine();


         /* -3-  write out the names of the atoms */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG ATOM_NAME");
         FortranWriteString("%FORMAT(20a4)");
         FortranFormat(20, LBLFORMAT);
         for (i = 0; i < prm->Natom; i++) {
            strncpy(tmpchar, &prm->AtomNames[i * 4], 4);
            tmpchar[4] = '\0';
            FortranWriteString(tmpchar);
         }
         FortranEndLine();

         /* -4- write out the atomic charges */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG CHARGE");
         FortranWriteString("%FORMAT(5E16.8)");
         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Natom; i++) {
            FortranWriteDouble(prm->Charges[i]);
         }
         FortranEndLine();

         /* -5- write out the atomic masses */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG MASS");
         FortranWriteString("%FORMAT(5E16.8)");
         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Natom; i++) {
            FortranWriteDouble(prm->Masses[i]);
         }
         FortranEndLine();

         /* -6- write out the atomic types */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG ATOM_TYPE_INDEX");
         FortranWriteString("%FORMAT(10I8)");
         FortranFormat(10, INTFORMAT);
         for (i = 0; i < prm->Natom; i++) {
            FortranWriteInt(prm->Iac[i]);
         }
         FortranEndLine();

         /* -7- write out the starting index into the excluded atom list */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG NUMBER_EXCLUDED_ATOMS");
         FortranWriteString("%FORMAT(10I8)");
         FortranFormat(10, INTFORMAT);
         for (i = 0; i < prm->Natom; i++) {
            FortranWriteInt(prm->Iblo[i]);
         }
         FortranEndLine();

         /* -8- Write the index for the position of the non bond type */
         /* of each type */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG NONBONDED_PARM_INDEX");
         FortranWriteString("%FORMAT(10I8)");
         FortranFormat(10, INTFORMAT);
         for (i = 0; i < prm->Ntype2d; i++) {
            FortranWriteInt(prm->Cno[i]);
         }
         FortranEndLine();

         /* -9- Residue labels */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG RESIDUE_LABEL");
         FortranWriteString("%FORMAT(20a4)");
         FortranFormat(20, LBLFORMAT);
         for (i = 0; i < prm->Nres; i++) {
            strncpy(tmpchar, &prm->ResNames[i * 4], 3);
            tmpchar[3] = ' ';
            tmpchar[4] = '\0';
            FortranWriteString(tmpchar);
         }
         FortranEndLine();

         /* -10- Pointer list for all the residues */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG RESIDUE_POINTER");
         FortranWriteString("%FORMAT(10I8)");
         FortranFormat(10, INTFORMAT);
         for (i = 0; i < prm->Nres; i++) {
            FortranWriteInt(prm->Ipres[i]);
         }
         FortranEndLine();

         /* -11- Force constants for bonds */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG BOND_FORCE_CONSTANT");
         FortranWriteString("%FORMAT(5E16.8)");
         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Numbnd; i++) {
            FortranWriteDouble(prm->Rk[i]);
         }
         FortranEndLine();

         /* -12- Equilibrium bond lengths */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG BOND_EQUIL_VALUE");
         FortranWriteString("%FORMAT(5E16.8)");
         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Numbnd; i++) {
            FortranWriteDouble(prm->Req[i]);
         }
         FortranEndLine();

         /* -13- Force constants for angles */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG ANGLE_FORCE_CONSTANT");
         FortranWriteString("%FORMAT(5E16.8)");
         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Numang; i++) {
            FortranWriteDouble(prm->Tk[i]);
         }
         FortranEndLine();

         /* -14- Equilibrium angle values */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG ANGLE_EQUIL_VALUE");
         FortranWriteString("%FORMAT(5E16.8)");
         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Numang; i++) {
            FortranWriteDouble(prm->Teq[i]);
         }
         FortranEndLine();

         /* -15- Force constants for torsions */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG DIHEDRAL_FORCE_CONSTANT");
         FortranWriteString("%FORMAT(5E16.8)");
         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Nptra; i++) {
            FortranWriteDouble(prm->Pk[i]);
         }
         FortranEndLine();

         /* -16- Periodicity for the dihedral angles */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG DIHEDRAL_PERIODICITY");
         FortranWriteString("%FORMAT(5E16.8)");
         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Nptra; i++) {
            FortranWriteDouble(prm->Pn[i]);
         }
         FortranEndLine();

         /* -17- Phase for torsions */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG DIHEDRAL_PHASE");
         FortranWriteString("%FORMAT(5E16.8)");
         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Nptra; i++) {
            FortranWriteDouble(prm->Phase[i]);
         }
         FortranEndLine();

         /* -18- Not used, reserved for future use, uses NATYP */
         /* Corresponds to the AMBER SOLTY array */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG SOLTY");
         FortranWriteString("%FORMAT(5E16.8)");

         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Natyp; i++) {
            FortranWriteDouble(0.0);
         }
         FortranEndLine();

         /* -19- Lennard jones r**12 term for all possible interactions */
         /* CN1 array */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG LENNARD_JONES_ACOEF");
         FortranWriteString("%FORMAT(5E16.8)");

         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Nttyp; i++) {
            FortranWriteDouble(prm->Cn1[i]);
         }
         FortranEndLine();

         /* -20- Lennard jones r**6 term for all possible interactions */
         /* CN2 array */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG LENNARD_JONES_BCOEF");
         FortranWriteString("%FORMAT(5E16.8)");

         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Nttyp; i++) {
            FortranWriteDouble(prm->Cn2[i]);
         }
         FortranEndLine();

         /* -21- Write the bond interactions that include hydrogen */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG BONDS_INC_HYDROGEN");
         FortranWriteString("%FORMAT(10I8)");
         FortranFormat(10, INTFORMAT);
         for (i = 0; i < prm->Nbonh; i++) {
            FortranWriteInt(prm->BondHAt1[i]);
            FortranWriteInt(prm->BondHAt2[i]);
            FortranWriteInt(prm->BondHNum[i]);
         }
         FortranEndLine();

         /* -22- Write the bond interactions that dont include hydrogen */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG BONDS_WITHOUT_HYDROGEN");
         FortranWriteString("%FORMAT(10I8)");
         FortranFormat(10, INTFORMAT);
         for (i = 0; i < prm->Nbona; i++) {
            FortranWriteInt(prm->BondAt1[i]);
            FortranWriteInt(prm->BondAt2[i]);
            FortranWriteInt(prm->BondNum[i]);
         }
         FortranEndLine();

         /* -23- Write the angle interactions that include hydrogen */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG ANGLES_INC_HYDROGEN");
         FortranWriteString("%FORMAT(10I8)");
         FortranFormat(10, INTFORMAT);
         for (i = 0; i < prm->Ntheth; i++) {
            FortranWriteInt(prm->AngleHAt1[i]);
            FortranWriteInt(prm->AngleHAt2[i]);
            FortranWriteInt(prm->AngleHAt3[i]);
            FortranWriteInt(prm->AngleHNum[i]);
         }
         FortranEndLine();

         /* -24- Write the angle interactions that dont include hydrogen */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG ANGLES_WITHOUT_HYDROGEN");
         FortranWriteString("%FORMAT(10I8)");
         FortranFormat(10, INTFORMAT);
         for (i = 0; i < prm->Ntheta; i++) {
            FortranWriteInt(prm->AngleAt1[i]);
            FortranWriteInt(prm->AngleAt2[i]);
            FortranWriteInt(prm->AngleAt3[i]);
            FortranWriteInt(prm->AngleNum[i]);
         }
         FortranEndLine();

         /* -25- Write the torsion interactions that include hydrogen */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG DIHEDRALS_INC_HYDROGEN");
         FortranWriteString("%FORMAT(10I8)");
         FortranFormat(10, INTFORMAT);
         for (i = 0; i < prm->Nphih; i++) {
            FortranWriteInt(prm->DihHAt1[i]);
            FortranWriteInt(prm->DihHAt2[i]);
            FortranWriteInt(prm->DihHAt3[i]);
            FortranWriteInt(prm->DihHAt4[i]);
            FortranWriteInt(prm->DihHNum[i]);
         }
         FortranEndLine();

         /* -26- Write the torsion interactions that dont include hydrogen */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG DIHEDRALS_WITHOUT_HYDROGEN");
         FortranWriteString("%FORMAT(10I8)");
         FortranFormat(10, INTFORMAT);
         for (i = 0; i < prm->Nphia; i++) {
            FortranWriteInt(prm->DihAt1[i]);
            FortranWriteInt(prm->DihAt2[i]);
            FortranWriteInt(prm->DihAt3[i]);
            FortranWriteInt(prm->DihAt4[i]);
            FortranWriteInt(prm->DihNum[i]);
         }
         FortranEndLine();

         /* -27- Write the excluded atom list */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG EXCLUDED_ATOMS_LIST");
         FortranWriteString("%FORMAT(10I8)");
         FortranFormat(10, INTFORMAT);
         for (i = 0; i < prm->Nnb; i++) {
            FortranWriteInt(prm->ExclAt[i]);
         }
         FortranEndLine();

         /* -28- Write the R^12 term for the Hydrogen bond equation */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG HBOND_ACOEF");
         FortranWriteString("%FORMAT(5E16.8)");

         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Nphb; i++) {
            FortranWriteDouble(prm->HB12[i]);
         }
         FortranEndLine();

         /* -29- Write the R^10 term for the Hydrogen bond equation */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG HBOND_BCOEF");
         FortranWriteString("%FORMAT(5E16.8)");

         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Nphb; i++) {
            FortranWriteDouble(prm->HB10[i]);
         }
         FortranEndLine();

         /* -30- No longer used, but stored */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG HBCUT");
         FortranWriteString("%FORMAT(5E16.8)");

         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Nphb; i++) {
            FortranWriteDouble(0.0);
         }
         FortranEndLine();

         /* -31- List of atomic symbols */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG AMBER_ATOM_TYPE");
         FortranWriteString("%FORMAT(20a4)");

         FortranFormat(20, LBLFORMAT);
         for (i = 0; i < prm->Natom; i++) {
            strncpy(tmpchar, &prm->AtomSym[i * 4], 2);
            tmpchar[2] = tmpchar[3] = ' ';
            tmpchar[4] = '\0';
            FortranWriteString(tmpchar);
         }
         FortranEndLine();

         /* -32- List of tree symbols */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG TREE_CHAIN_CLASSIFICATION");
         FortranWriteString("%FORMAT(20a4)");

         FortranFormat(20, LBLFORMAT);
         for (i = 0; i < prm->Natom; i++) {
            strncpy(tmpchar, &prm->AtomTree[i * 4], 2);
            tmpchar[2] = tmpchar[3] = ' ';
            tmpchar[4] = '\0';
            FortranWriteString(tmpchar);
         }
         FortranEndLine();

         /* -33- Tree Joining information !!!!!!! Add support for this !!!!! */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG JOIN_ARRAY");
         FortranWriteString("%FORMAT(10I8)");

         FortranFormat(10, INTFORMAT);
         for (i = 0; i < prm->Natom; i++) {
            FortranWriteInt(prm->TreeJoin[i]);
         }
         FortranEndLine();

         /* -34- Who knows, something to do with rotating atoms */
         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG IROTAT");
         FortranWriteString("%FORMAT(10I8)");

         FortranFormat(10, INTFORMAT);
         for (i = 0; i < prm->Natom; i++) {
            /*  FortranWriteInt(prm->AtomRes[i]);  -- really the correct thing */
            FortranWriteInt(0); /* for consistency with LEaP  */
         }
         FortranEndLine();

#if 0
         /* -35A- The last residue before "solvent" */
         /* Number of molecules */
         /* Index of first molecule that is solvent */

         if (bUnitUseBox(uUnit)) {

            /* Find the index of the first solvent RESIDUE */

            for (i = 0; i < iVarArrayElementCount(uUnit->vaResidues); i++) {
               if (PVAI(uUnit->vaResidues, SAVERESIDUEt, i)->
                   sResidueType[0] == RESTYPESOLVENT)
                  break;
            }
            iTemp = i;

            /* 
             *  Find the molecules and return the number of ATOMs in each 
             *  molecule, along with the index of the first solvent molecule
             */

            vaMolecules = vaVarArrayCreate(sizeof(int));
            zUnitIOFindAndCountMolecules(uUnit, &vaMolecules,
                                         &iFirstSolvent);

            FortranFormat(1, "%-80s");
            FortranWriteString("%FLAG SOLVENT_POINTERS");
            FortranWriteString("%FORMAT(3I8)");
            FortranFormat(3, INTFORMAT);
            FortranWriteInt(iTemp);
            FortranWriteInt(iVarArrayElementCount(vaMolecules));
            FortranWriteInt(iFirstSolvent + 1); /* FORTRAN index */

            FortranEndLine();

            /* -35B- The number of ATOMs in the Ith RESIDUE */

            FortranDebug("-35B-");
            FortranFormat(1, "%-80s");
            FortranWriteString("%FLAG ATOMS_PER_MOLECULE");
            FortranWriteString("%FORMAT(10I8)");
            FortranFormat(10, INTFORMAT);
            for (i = 0; i < iVarArrayElementCount(vaMolecules); i++) {
               FortranWriteInt(*PVAI(vaMolecules, int, i));
            }
            FortranEndLine();

            /* -35C- BETA, (BOX(I), I=1,3 ) */

            FortranDebug("-35C-");
            FortranFormat(1, "%-80s");
            FortranWriteString("%FLAG BOX_DIMENSIONS");
            FortranWriteString("%FORMAT(5E16.8)");
            FortranFormat(4, DBLFORMAT);
            FortranWriteDouble(dUnitBeta(uUnit) / DEGTORAD);
            UnitGetBox(uUnit, &dX, &dY, &dZ);
            FortranWriteDouble(dX);
            FortranWriteDouble(dY);
            FortranWriteDouble(dZ);
            FortranEndLine();
         }
#endif

         /* -35D- NATCAP */

         if (prm->IfCap) {
            FortranFormat(1, "%-80s");
            FortranWriteString("%FLAG CAP_INFO");
            FortranWriteString("%FORMAT(10I8)");
            FortranFormat(1, INTFORMAT);
            FortranWriteInt(prm->Natcap);
            FortranEndLine();

            /* -35E- CUTCAP, XCAP, YCAP, ZCAP */
            FortranFormat(1, "%-80s");
            FortranWriteString("%FLAG CAP_INFO2");
            FortranWriteString("%FORMAT(5E16.8)");
            FortranFormat(4, DBLFORMAT);
            FortranWriteDouble(prm->Cutcap);
            FortranWriteDouble(prm->Xcap);
            FortranWriteDouble(prm->Ycap);
            FortranWriteDouble(prm->Zcap);
            FortranEndLine();
         }

         /* write out the GB radii */

         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG RADII");
         FortranWriteString("%FORMAT(5E16.8)");
         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Natom; i++) {
            FortranWriteDouble(prm->Rborn[i]);
         }
         FortranEndLine();

         /* write out the GB screening parameters */

         FortranFormat(1, "%-80s");
         FortranWriteString("%FLAG SCREEN");
         FortranWriteString("%FORMAT(5E16.8)");
         FortranFormat(5, DBLFORMAT);
         for (i = 0; i < prm->Natom; i++) {
            FortranWriteDouble(prm->Fs[i]);
         }
         FortranEndLine();

#if 0

         /*  Charmm-style parameters  */

         if (GDefaults.iCharmm) {
            /* -19- Lennard jones r**12 term for all 14 interactions */
            /* CN114 array */
            FortranDebug("-19-");

            FortranFormat(5, DBLFORMAT);
            for (i = 0; i < iVarArrayElementCount(vaNBParameters); i++) {
               FortranWriteDouble(PVAI(vaNBParameters, NONBONDACt, i)->
                                  dA14);
            }
            FortranEndLine();

            /* -20- Lennard jones r**6 term for all 14 interactions */
            /* CN214 array */
            FortranDebug("-20-");

            FortranFormat(5, DBLFORMAT);
            for (i = 0; i < iVarArrayElementCount(vaNBParameters); i++) {
               FortranWriteDouble(PVAI(vaNBParameters, NONBONDACt, i)->
                                  dC14);
            }
            FortranEndLine();

            /* -13- Force constants for Urey-Bradley */
            FortranDebug("-13-");

            FortranFormat(5, DBLFORMAT);
            for (i = 0; i < iParmSetTotalAngleParms(uUnit->psParameters);
                 i++) {
               ParmSetAngle(uUnit->psParameters, i, sAtom1, sAtom2, sAtom3,
                            &dKt, &dT0, &dTkub, &dRkub, sDesc);
               FortranWriteDouble(dTkub);
            }
            FortranEndLine();

            /* -14- Equilibrium distances for Urey-Bradley */
            FortranDebug("-14-");

            FortranFormat(5, DBLFORMAT);
            for (i = 0; i < iParmSetTotalAngleParms(uUnit->psParameters);
                 i++) {
               ParmSetAngle(uUnit->psParameters, i, sAtom1, sAtom2, sAtom3,
                            &dKt, &dT0, &dTkub, &dRkub, sDesc);
               FortranWriteDouble(dRkub);
            }
            FortranEndLine();

         }


      /********************************************************/
         /* Write the coordinate file                            */
      /********************************************************/

         FortranFile(fCrd);

         FortranFormat(1, "%s");
         FortranWriteString(sContainerName(uUnit));
         FortranEndLine();

         FortranFormat(1, "%6d");
         FortranWriteInt(iVarArrayElementCount(uUnit->vaAtoms));
         FortranEndLine();

         FortranFormat(6, "%12.7lf");
         if (bUnitUseBox(uUnit)) {
            double dX2, dY2, dZ2;

            UnitGetBox(uUnit, &dX, &dY, &dZ);
            dX2 = dX * 0.5;
            dY2 = dY * 0.5;
            dZ2 = dZ * 0.5;

            /*
             *  shift box to Amber spot; later, add a cmd opt or environment
             *      var to switch between 0,0,0 center (spasms) or corner
             */
            for (i = 0; i < iVarArrayElementCount(uUnit->vaAtoms); i++) {
               vPos = PVAI(uUnit->vaAtoms, SAVEATOMt, i)->vPos;
               FortranWriteDouble(dVX(&vPos) + dX2);
               FortranWriteDouble(dVY(&vPos) + dY2);
               FortranWriteDouble(dVZ(&vPos) + dZ2);
            }
            FortranEndLine();
            FortranWriteDouble(dX);
            FortranWriteDouble(dY);
            FortranWriteDouble(dZ);
            FortranWriteDouble(dUnitBeta(uUnit) / DEGTORAD);
            FortranWriteDouble(dUnitBeta(uUnit) / DEGTORAD);
            FortranWriteDouble(dUnitBeta(uUnit) / DEGTORAD);
            FortranEndLine();
         } else {
            for (i = 0; i < iVarArrayElementCount(uUnit->vaAtoms); i++) {
               vPos = PVAI(uUnit->vaAtoms, SAVEATOMt, i)->vPos;
               FortranWriteDouble(dVX(&vPos));
               FortranWriteDouble(dVY(&vPos));
               FortranWriteDouble(dVZ(&vPos));
            }
            FortranEndLine();
         }

         VarArrayDestroy(&vaNBIndexMatrix);
         VarArrayDestroy(&vaNBParameters);
         VarArrayDestroy(&vaExcludedAtoms);
         VarArrayDestroy(&vaExcludedCount);
         VarArrayDestroy(&vaNBIndex);
         VarArrayDestroy(&vaNonBonds);
#endif
         fclose(SfFile);
      }
   }
   reducerror(ier);

   return (0);
}

PARMSTRUCT_T *copyparm(PARMSTRUCT_T * prm)
{
   PARMSTRUCT_T *newprm;
   char *AtomNamesbuf, *ResNamesbuf, *AtomSymbuf, *AtomTreebuf;

   if ((newprm = (PARMSTRUCT_T *) malloc(sizeof(PARMSTRUCT_T))) == NULL) {
      sprintf(e_msg, "new PARMSTRUCT_T %s", prm->ititl);
      rt_errormsg_s(TRUE, E_NOMEM_FOR_S, e_msg);
      return (NULL);
   }

   strcpy(newprm->ititl, prm->ititl);
   newprm->IfBox = prm->IfBox;
   newprm->Nmxrs = prm->Nmxrs;
   newprm->IfCap = prm->IfCap;
   newprm->Natom = prm->Natom;
   newprm->Ntypes = prm->Ntypes;
   newprm->Nbonh = prm->Nbonh;
   newprm->Nbona = prm->Nbona;
   newprm->Ntheth = prm->Ntheth;
   newprm->Ntheta = prm->Ntheta;
   newprm->Nphih = prm->Nphih;
   newprm->Nphia = prm->Nphia;
   newprm->Numbnd = prm->Numbnd;
   newprm->Numang = prm->Numang;
   newprm->Nptra = prm->Nptra;
   newprm->Natyp = prm->Natyp;
   newprm->Nphb = prm->Nphb;
   newprm->Nat3 = prm->Nat3;
   newprm->Ntype2d = prm->Ntype2d;
   newprm->Nttyp = prm->Nttyp;
   newprm->Nspm = prm->Nspm;
   newprm->Iptres = prm->Iptres;
   newprm->Nspsol = prm->Nspsol;
   newprm->Ipatm = prm->Ipatm;
   newprm->Natcap = prm->Natcap;
   if ((AtomNamesbuf =
        (char *) malloc((strlen(prm->AtomNames) + 1) *
                        sizeof(char))) == NULL) {
      sprintf(e_msg, "copyparm AtomNames %s", prm->AtomNames);
      rt_errormsg_s(TRUE, E_NOMEM_FOR_S, e_msg);
      return (NULL);
   }
   newprm->AtomNames = AtomNamesbuf;
   newprm->Charges = prm->Charges;
   newprm->Masses = prm->Masses;
   newprm->Iac = prm->Iac;
   newprm->Iblo = prm->Iblo;
   newprm->Cno = prm->Cno;
   if ((ResNamesbuf = (char *) malloc((strlen(prm->ResNames) + 1) *
                                      sizeof(char))) == NULL) {
      sprintf(e_msg, "copyparm ResNames %s", prm->ResNames);
      rt_errormsg_s(TRUE, E_NOMEM_FOR_S, e_msg);
      return (NULL);
   }
   newprm->ResNames = ResNamesbuf;
   newprm->Ipres = prm->Ipres;
   newprm->Rk = prm->Rk;
   newprm->Req = prm->Req;
   newprm->Tk = prm->Tk;
   newprm->Teq = prm->Teq;
   newprm->Pk = prm->Pk;
   newprm->Pn = prm->Pn;
   newprm->Phase = prm->Phase;
   newprm->Solty = prm->Solty;
   newprm->Cn1 = prm->Cn1;
   newprm->Cn2 = prm->Cn2;
   newprm->Box[0] = prm->Box[0];
   newprm->Box[1] = prm->Box[1];
   newprm->Box[2] = prm->Box[2];
   newprm->Cutcap = prm->Cutcap;
   newprm->Xcap = prm->Xcap;
   newprm->Ycap = prm->Ycap;
   newprm->Zcap = prm->Zcap;
   newprm->BondHAt1 = prm->BondHAt1;
   newprm->BondHAt2 = prm->BondHAt2;
   newprm->BondHNum = prm->BondHNum;
   newprm->BondAt1 = prm->BondAt1;
   newprm->BondAt2 = prm->BondAt2;
   newprm->BondNum = prm->BondNum;
   newprm->AngleHAt1 = prm->AngleHAt1;
   newprm->AngleHAt2 = prm->AngleHAt2;
   newprm->AngleHAt3 = prm->AngleHAt3;
   newprm->AngleHNum = prm->AngleHNum;
   newprm->AngleAt1 = prm->AngleAt1;
   newprm->AngleAt2 = prm->AngleAt2;
   newprm->AngleAt3 = prm->AngleAt3;
   newprm->AngleNum = prm->AngleNum;
   newprm->DihHAt1 = prm->DihHAt1;
   newprm->DihHAt2 = prm->DihHAt2;
   newprm->DihHAt3 = prm->DihHAt3;
   newprm->DihHAt4 = prm->DihHAt4;
   newprm->DihHNum = prm->DihHNum;
   newprm->DihAt1 = prm->DihAt1;
   newprm->DihAt2 = prm->DihAt2;
   newprm->DihAt3 = prm->DihAt3;
   newprm->DihAt4 = prm->DihAt4;
   newprm->DihNum = prm->DihNum;
   newprm->Boundary = prm->Boundary;
   newprm->ExclAt = prm->ExclAt;
   newprm->HB12 = prm->HB12;
   newprm->HB10 = prm->HB10;
   if ((AtomSymbuf = (char *) malloc((strlen(prm->AtomSym) + 1) *
                                     sizeof(char))) == NULL) {
      sprintf(e_msg, "copyparm AtomSym %s", prm->AtomSym);
      rt_errormsg_s(TRUE, E_NOMEM_FOR_S, e_msg);
      return (NULL);
   }
   newprm->AtomSym = AtomSymbuf;
   if ((AtomTreebuf = (char *) malloc((strlen(prm->AtomTree) + 1) *
                                      sizeof(char))) == NULL) {
      sprintf(e_msg, "copyparm AtomTree %s", prm->AtomTree);
      rt_errormsg_s(TRUE, E_NOMEM_FOR_S, e_msg);
      return (NULL);
   }
   newprm->AtomTree = AtomTreebuf;
   newprm->TreeJoin = prm->TreeJoin;
   newprm->AtomRes = prm->AtomRes;
   newprm->N14pairs = prm->N14pairs;
   newprm->N14pairlist = prm->N14pairlist;

   return (newprm);
}

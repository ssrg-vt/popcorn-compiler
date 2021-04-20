#include	<stdio.h>
#include	<ctype.h>
#include	<string.h>
#include	<math.h>
#include	<stdlib.h>
#include	<assert.h>

#include	"nab.h"
#include	"errormsg.h"
#include	"memutil.h"
#ifdef MMCIF
#  include	"../cifparse/cifparse.h"
#endif

    /* PDB columns, 0-based */
#define		PDB_ANUM_COL	7
#define		PDB_ANAM_COL	12
#define		PDB_RNAM_COL	17
#define		PDB_CNAM_COL	21
#define		PDB_RID_COL	21
#define		PDB_RNUM_COL	22

#define		PDB_XPOS_COL	30
#define		PDB_YPOS_COL	38
#define		PDB_ZPOS_COL	46

#define		PDB_QPOS_COL	54
#define		PDB_RPOS_COL	64

#define		PDB_OPOS_COL	54
#define		PDB_BPOS_COL	60

#define		PDB_XPOS_LEN	8
#define		PDB_YPOS_LEN	8
#define		PDB_ZPOS_LEN	8

#define		PDB_QPOS_LEN	10
#define		PDB_RPOS_LEN	10

#define		PDB_OPOS_LEN	6
#define		PDB_BPOS_LEN	6

#define		PDB_ANUM_LEN	4
#define		PDB_ANAM_LEN	4
#define		PDB_RNAM_LEN	3
#define		PDB_RID_LEN	6
#define		PDB_RID_SIZE	7
#define		PDB_NPOS_COL	8

    /* useful sizes for initial allocs of a_atomname, r_resname */
#define	A_NAME_SIZE	8
#define	R_NAME_SIZE	8

#define	D2R	0.01745329251994329576

    /* stuff for craeting chain ids */

typedef struct cid_t {
    int c_next;
    int c_last;
    char *c_cids;
} CID_T;

static void freecid(CID_T *);
static CID_T *initcid(int, int, MOLECULE_T *);
static int nextcid(CID_T *, int, int, STRAND_T *);

#define	ATAB_SIZE	1000
static ATOM_T atab[ATAB_SIZE];
static int n_atab;

#define	B_THRESH	1.85
#define	BH_THRESH	1.20

static RESIDUE_T res;

MOLECULE_T *getpdb(char *, char *);
MATRIX_T *getmatrix(char *);
int putpdb(char *, MOLECULE_T *, char *);
int putcif(char[], char[], MOLECULE_T *);
int putbnd(char[], MOLECULE_T *);
int putdist(char[], MOLECULE_T *, char[], char[]);
int putmatrix(char[], MATRIX_T);
static MOLECULE_T *fgetpdb(FILE *, char *);
static int isnewres(char[], char[], int, int);
static void init_atab(void);
static void initres(void);
void NAB_initres(RESIDUE_T *, int);
void NAB_initatom(ATOM_T *, int);
static void makebonds(RESIDUE_T *);
static REAL_T dist(ATOM_T *, ATOM_T *);
static void fputpdb(FILE *, MOLECULE_T *, char *);
static void fputcif(FILE *, char *, MOLECULE_T *);
static void mk_brook_rname(char[], RESIDUE_T *);
static void mk_brook_aname(char[], char[], char[]);
static void mk_wwpdb_rname(char[], RESIDUE_T *);
static void mk_wwpdb_aname(char[], char[], char[]);

int get_mytaskid(void);
void reducerror(int);
char *ggets(char *, int, FILE *);
void upd_molnumbers(MOLECULE_T *);
MOLECULE_T *newmolecule(void);
int addstrand(MOLECULE_T *, char[]);
int addresidue(MOLECULE_T *, char[], RESIDUE_T *);
void select_atoms(MOLECULE_T *, char[]);
int rt_errormsg_s(int, char[], char[]);


/***********************************************************************
                            GETPDB()
************************************************************************/

/*
 * Get the molecular topology and geometry from a .pdb file.
 * Task 0 opens and closes the file.  All tasks execute fgetpdb
 * but the ggets function reads from the file via task 0, and
 * broadcasts the results to all other tasks.
 */

MOLECULE_T *getpdb(char *fname, char *options)
{
    FILE *fp;
    MOLECULE_T *mol;
    int ier;

    if (!fname || !*fname) {
        fp = stdin;
    } else if (!strcmp(fname, "-")) {
        fp = stdin;
    } else {
        ier = 0;
        if (get_mytaskid() == 0) {
            if ((fp = fopen(fname, "r")) == NULL) {
                fprintf(stderr, "getpdb: can't open file %s\n", fname);
                ier = -1;
            }
        }
        reducerror(ier);

        /* The file is open.  Set fp to NULL for all tasks but task 0. */

        if (get_mytaskid() != 0) {
            fp = NULL;
        }
    }

    mol = fgetpdb(fp, options);

    if (fp != stdin) {
        if (get_mytaskid() == 0) {
            fclose(fp);
        }
    }

    return (mol);
}

MATRIX_T *getmatrix(char *fname)
{
    FILE *fp = NULL;
    char line[256];
    int r, c, cnt;
    static MATRIX_T mat;
    void *ptr;
    int err = 0;

    memset(mat, 0, sizeof(mat));
    if (fname == NULL || *fname == '\0') {
        fprintf(stderr, "getmatrix: NULL or empty file name\n");
        err = 1;
        goto CLEAN_UP;
    } else if (!strcmp(fname, "-"))
        fp = stdin;
    else if ((fp = fopen(fname, "r")) == NULL) {
        fprintf(stderr, "getmatrix: can't read matrix file %s\n", fname);
        err = 1;
        goto CLEAN_UP;
    }
    for (r = 0; fgets(line, sizeof(line), fp);) {
        if (*line == '#')
            continue;
#ifdef NAB_DOUBLE_PRECISION
        cnt = sscanf(line, "%lf %lf %lf %lf",
                     &mat[r][0], &mat[r][1], &mat[r][2], &mat[r][3]);
#else
        cnt = sscanf(line, "%f %f %f %f",
                     &mat[r][0], &mat[r][1], &mat[r][2], &mat[r][3]);
#endif
        if (cnt != 4) {
            fprintf(stderr,
                    "getmatrix: bad row %d: got %d elements, needed 4\n",
                    r + 1, cnt);
            memset(mat, 0, sizeof(mat));
            err = 1;
            goto CLEAN_UP;
        }
        r++;
        if (r >= 4)
            break;
    }
    if (r != 4) {
        fprintf(stderr, "getmatrix: missing rows: got %d rows, needed 4\n",
                r);
        memset(mat, 0, sizeof(mat));
        err = 1;
        goto CLEAN_UP;
    }

  CLEAN_UP:;

    if (fp != NULL && fp != stdin)
        fclose(fp);

    ptr = mat;
    return ptr;
}

/***********************************************************************
                            PUTPDB()
************************************************************************/

/* Put the molecular topology and geometry to a .pdb file via task 0 only. */

int putpdb(char *fname, MOLECULE_T * mol, char *options)
{
    FILE *fp;
    int ier;

    if (!mol) {
        if (get_mytaskid() == 0) {
            fprintf(stderr, "putpdb: NULL molecule\n");
        }
        return (0);
    }

    ier = 0;
    if (get_mytaskid() == 0) {
        if (!strcmp(fname, "-"))
            fp = stdout;
        else if ((fp = fopen(fname, "w")) == NULL) {
            rt_errormsg_s(TRUE, E_CANT_OPEN_S, fname);
            ier = -1;
        }
        if (ier >= 0) {
            fputpdb(fp, mol, options);
            if (fp != stdout)
                fclose(fp);
        }
    }
    reducerror(ier);

    return (0);
}

int putcif(char fname[], char blockId[], MOLECULE_T * mol)
{
    FILE *fp;

    if (!mol) {
        fprintf(stderr, "putcif: NULL molecule\n");
        return (0);
    }
    if (!strcmp(fname, "-"))
        fp = stdout;
    else if ((fp = fopen(fname, "w")) == NULL) {
        rt_errormsg_s(TRUE, E_CANT_OPEN_S, fname);
        exit(1);
    }
    fputcif(fp, blockId, mol);
    if (fp != stdout)
        fclose(fp);
    return (0);
}

int putbnd(char fname[], MOLECULE_T * mol)
{
    FILE *fp;
    STRAND_T *sp;
    RESIDUE_T *res;
    int a, ta, ai, aj, r, rj, tr;
    int rval = 0;
    int *aoff = NULL;
    int b;
    EXTBOND_T *ebp;

    if (!mol) {
        fprintf(stderr, "putbnd: NULL molecule\n");
        return (0);
    }
    if ((fp = fopen(fname, "w")) == NULL) {
        rt_errormsg_s(TRUE, E_CANT_OPEN_S, fname);
        exit(1);
    }

    for (tr = 0, sp = mol->m_strands; sp; sp = sp->s_next)
        tr += sp->s_nresidues;

    if ((aoff = (int *) malloc(tr * sizeof(int))) == NULL) {
        rt_errormsg_s(TRUE, E_NOMEM_FOR_S, "aoff array in putbnd");
        rval = 1;
        goto clean_up;
    }

    for (ta = 0, tr = 0, sp = mol->m_strands; sp; sp = sp->s_next) {
        aoff[tr] = ta;
        if (sp->s_nresidues > 0) {
            ta += sp->s_residues[0]->r_natoms;
            for (r = 1; r < sp->s_nresidues; r++) {
                aoff[tr + r] = ta;
                ta += sp->s_residues[r]->r_natoms;
            }
            tr += sp->s_nresidues;
        }
    }

    for (tr = 0, sp = mol->m_strands; sp; sp = sp->s_next) {
        for (r = 0; r < sp->s_nresidues; r++) {
            res = sp->s_residues[r];
            a = aoff[r + tr];
            for (b = 0; b < res->r_nintbonds; b++) {
                fprintf(fp, "%d %d\n",
                        res->r_intbonds[b][0] + a,
                        res->r_intbonds[b][1] + a);
            }
            for (ebp = res->r_extbonds; ebp; ebp = ebp->eb_next) {
                if ((rj = ebp->eb_rnum) < r + 1)
                    continue;
                ai = aoff[tr + r];
                aj = aoff[tr + rj - 1];
                fprintf(fp, "%d %d\n",
                        ebp->eb_anum + ai, ebp->eb_ranum + aj);
            }
        }
        tr += sp->s_nresidues;
    }

  clean_up:;
    if (aoff)
        free(aoff);

    fclose(fp);

    return (rval);
}

int putdist(char fname[], MOLECULE_T * mol, char aexp1[], char aexp2[])
{
    FILE *fp;
    STRAND_T *sp1;
    RESIDUE_T *res1, *res2;
    ATOM_T *ap1, *ap2;
    int tr, r1, r2, a1, a2;
    RESIDUE_T **res;
    int cnt;

    if (!mol) {
        fprintf(stderr, "putdist: NULL molecule\n");
        return (0);
    }

    select_atoms(mol, aexp1);
    for (sp1 = mol->m_strands; sp1; sp1 = sp1->s_next) {
        sp1->s_attr &= ~AT_SELECTED;
        sp1->s_attr |= (sp1->s_attr & AT_SELECT) ? AT_SELECTED : 0;
        for (r1 = 0; r1 < sp1->s_nresidues; r1++) {
            res1 = sp1->s_residues[r1];
            res1->r_attr &= ~AT_SELECTED;
            res1->r_attr |= (res1->r_attr & AT_SELECT) ? AT_SELECTED : 0;
            for (a1 = 0, ap1 = res1->r_atoms; a1 < res1->r_natoms;
                 a1++, ap1++) {
                ap1->a_attr &= ~AT_SELECTED;
                ap1->a_attr |= (ap1->a_attr & AT_SELECT) ? AT_SELECTED : 0;
            }
        }
    }
    select_atoms(mol, aexp2);

    for (tr = 0, sp1 = mol->m_strands; sp1; sp1 = sp1->s_next)
        tr += sp1->s_nresidues;
    if ((res = (RESIDUE_T **) malloc(tr * sizeof(RESIDUE_T *)))
        == NULL) {
        fprintf(stderr, "can't allocate res\n");
        goto clean_up;
    }
    for (r2 = 0, sp1 = mol->m_strands; sp1; sp1 = sp1->s_next) {
        for (r1 = 0; r1 < sp1->s_nresidues; r1++)
            res[r2++] = sp1->s_residues[r1];
    }

    if ((fp = fopen(fname, "w")) == NULL) {
        fprintf(stderr, "can't open dist file %s\n", fname);
        goto clean_up;
    }

    for (cnt = 0, r1 = 0; r1 < tr; r1++) {
        res1 = res[r1];
        for (a1 = 0; a1 < res1->r_natoms; a1++) {
            ap1 = &res1->r_atoms[a1];
            if (AT_SELECTED & ap1->a_attr) {
                for (r2 = 0; r2 < tr; r2++) {
                    res2 = res[r2];
                    for (a2 = 0; a2 < res2->r_natoms; a2++) {
                        ap2 = &res2->r_atoms[a2];
                        if (AT_SELECT & ap2->a_attr) {
                            fprintf(fp,
                                    "%3d %-4s %-4s %3d %-4s %4s %8.3f\n",
                                    r1 + 1, res1->r_resname,
                                    ap1->a_atomname, r2 + 1,
                                    res2->r_resname, ap2->a_atomname,
                                    dist(ap1, ap2));
                            cnt++;
                        }
                    }
                }
            }
        }
    }

  clean_up:;
    if (res)
        free(res);

    return (cnt);
}

int putmatrix(char fname[], MATRIX_T mat)
{
    FILE *fp = NULL;
    int i, j;
    int err = 0;

    if (fname == NULL || *fname == '\0') {
        fprintf(stderr, "putmatrix: NULL or empty file name\n");
        err = 1;
        goto CLEAN_UP;
    } else if (!strcmp(fname, "-"))
        fp = stdout;
    else if ((fp = fopen(fname, "w")) == NULL) {
        fprintf(stderr, "putmatrix: can't write file %s\n", fname);
        err = 1;
        goto CLEAN_UP;
    }

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++)
            fprintf(fp, " %g", mat[i][j]);
        fprintf(fp, "\n");
    }

  CLEAN_UP:;

    if (fp != NULL && fp != stdout)
        fclose(fp);
    return err;
}

/***********************************************************************
                            FGETPDB()
************************************************************************/

/* Parse the molecular topology and geometry from a .pdb file via all tasks. */

static MOLECULE_T *fgetpdb(FILE * fp, char *options)
{
    MOLECULE_T *mol;
    ATOM_T *ap;
    int l_cid, cid;
    int nchains;
    char line[82];
    char sname[10];
    char aname[10];
    char rname[10], l_rname[10];
    char rid[10], l_rid[10];    /* cols 22-27 */
    char field[10];             /* temp. used to get coords, etc.  */
    char *np, *np1;
    int rnum, l_rnum;
    REAL_T x, y, z, q, r, occ, bfact;
    int i;
    char temp;
    char loptions[256];

    if (options == NULL)
        *loptions = '\0';
    else
        strcpy(loptions, options);

    initres();
    init_atab();

    mol = newmolecule();

    l_cid = '\0';
    *l_rname = '\0';
    l_rnum = 0;
    for (nchains = 0, n_atab = 0;;) {

        /*
         * The ggets function performs fgets via task 0 only,
         * and broadcasts the result to all other tasks.
         */

        if (ggets(line, sizeof(line), fp) == NULL)
            break;

        /* pad input line out to 80 columns with blanks  */
        for (i = strlen(line); i < 80; i++)
            line[i] = ' ';
        line[80] = '\0';

        if (strncmp("ATOM", line, 4) == 0 ||
            strncmp("HETATM", line, 6) == 0) {
            strncpy(aname, &line[PDB_ANAM_COL], PDB_ANAM_LEN);
            aname[PDB_ANAM_LEN] = '\0';
            /* strip blanks: */
            for (np = np1 = aname; *np; np++) {
                if (*np != ' ')
                    *np1++ = *np;
            }
            *np1 = '\0';

            /* rotate initial digit: */
            if (aname[0] == '1' || aname[0] == '2' || aname[0] == '3') {
                temp = aname[0];
                for (i = 0; i < strlen(aname) - 1; i++)
                    aname[i] = aname[i + 1];
                aname[i] = temp;
            }

            /* change Brookhaven nucleic acid atom names to iupuac */
            if (aname[2] == '*')
                aname[2] = '\'';
            if (!strcmp(aname, "C5M"))
                strcpy(aname, "C7");

            strncpy(rname, &line[PDB_RNAM_COL], PDB_RNAM_LEN);
            rname[PDB_RNAM_LEN] = '\0';
            for (np = np1 = rname; *np; np++) {
                if (*np != ' ')
                    *np1++ = *np;
            }
            *np1 = '\0';

            /* check to see if this residue is RNA, and change the
               Brookahven name H2'1 to the Amber (and IUPAC?) standard
               H2'.  If the Brookhaven file doesn't have protons, there
               is no problem; if it does, they usually follow the heavy
               atoms, and the following code will also work.  If H2'
               precedes O2', then the code below will fail, and something
               will have to be written to pre-scan the entire resiude :-(   */
            if (!strcmp(aname, "O2\'"))
                res.r_kind = RT_RNA;
            if (!strcmp(aname, "H2\'1") && res.r_kind == RT_RNA)
                strcpy(aname, "H2\'");

            /* change Brookhaven nucleic acid residue names to iupuac */
            if (!strcmp(rname, "A"))
                strcpy(rname, "ADE");
            if (!strcmp(rname, "C"))
                strcpy(rname, "CYT");
            if (!strcmp(rname, "G"))
                strcpy(rname, "GUA");
            if (!strcmp(rname, "T"))
                strcpy(rname, "THY");
            if (!strcmp(rname, "U"))
                strcpy(rname, "URA");

            /* grab the chain, r seq num & ins. # */
            strncpy(rid, &line[PDB_RID_COL], PDB_RID_LEN);
            rid[PDB_RID_LEN] = '\0';

            cid = line[PDB_CNAM_COL];
            sscanf(&line[PDB_RNUM_COL], "%d", &rnum);

            strncpy(field, &line[PDB_XPOS_COL], PDB_XPOS_LEN);
            field[PDB_XPOS_LEN] = '\0';
            x = atof(field);

            strncpy(field, &line[PDB_YPOS_COL], PDB_YPOS_LEN);
            field[PDB_YPOS_LEN] = '\0';
            y = atof(field);

            strncpy(field, &line[PDB_ZPOS_COL], PDB_ZPOS_LEN);
            field[PDB_ZPOS_LEN] = '\0';
            z = atof(field);

            if (strstr(loptions, "-pqr")) {

                /*          if we decide to make pqr files fixed format, the following
                   code may help:

                   strncpy( field, &line[PDB_QPOS_COL], PDB_QPOS_LEN );
                   field[ PDB_QPOS_LEN ] = '\0';
                   q = atof( field );

                   strncpy( field, &line[PDB_RPOS_COL], PDB_RPOS_LEN );
                   field[ PDB_RPOS_LEN ] = '\0';
                   r = atof( field );

                   for now, with free format, use the following:               */

#ifdef NAB_DOUBLE_PRECISION
                sscanf(&line[PDB_QPOS_COL], "%lf%lf", &q, &r);
#else
                sscanf(&line[PDB_QPOS_COL], "%f%f", &q, &r);
#endif

                occ = 1.0;
                bfact = 0.0;

            } else {
                /* use Bondi radii and zero charges for defaults: */
                if (!strncmp(aname, "H", 1))
                    r = 1.2;
                else if (!strncmp(aname, "C", 1))
                    r = 1.70;
                else if (!strncmp(aname, "N", 1))
                    r = 1.55;
                else if (!strncmp(aname, "O", 1))
                    r = 1.50;
                else if (!strncmp(aname, "S", 1))
                    r = 1.80;
                else if (!strncmp(aname, "P", 1))
                    r = 1.85;
                else
                    r = 1.7;    /* carbon-like radius for others */
                q = 0.0;

                /* read in occ. and bfactor */
                strncpy(field, &line[PDB_OPOS_COL], PDB_OPOS_LEN);
                field[PDB_OPOS_LEN] = '\0';
                occ = atof(field);

                strncpy(field, &line[PDB_BPOS_COL], PDB_BPOS_LEN);
                field[PDB_BPOS_LEN] = '\0';
                bfact = atof(field);
            }

            if (cid != l_cid) {
                if (n_atab > 0) {
                    strcpy(res.r_resname, l_rname);
                    strcpy(res.r_resid, l_rid);
                    res.r_num = l_rnum;
                    res.r_natoms = n_atab;
                    makebonds(&res);
                    addresidue(mol, sname, &res);
                    initres();
                    n_atab = 0;
                }

                nchains++;
                if (cid == ' ')
                    sprintf(sname, "%d", nchains);
                else {
                    sname[0] = cid;
                    sname[1] = '\0';
                }
                addstrand(mol, sname);

                {               /* temp, will do right soon!    */
                    STRAND_T *sp, *spl;

                    for (sp = NULL, spl = mol->m_strands; spl;
                         spl = spl->s_next) {
                        if (!strcmp(spl->s_strandname, sname)) {
                            sp = spl;
                            break;
                        }
                    }
                    sp->s_res_size = 10000; /* memory is cheap */
                    sp->s_residues =
                        (RESIDUE_T **) malloc(sp->s_res_size *
                                              sizeof(RESIDUE_T));
                    if (sp->s_residues == NULL) {
                        /* too big, die, shouldn't happen   */
                    }
                }

                l_cid = cid;
                strcpy(l_rname, rname);
                strcpy(l_rid, rid);
                l_rnum = rnum;

            } else if (isnewres(l_rname, rname, l_rnum, rnum)) {
                strcpy(res.r_resname, l_rname);
                strcpy(res.r_resid, l_rid);
                res.r_num = l_rnum;
                res.r_natoms = n_atab;
                makebonds(&res);
                addresidue(mol, sname, &res);
                initres();
                strcpy(l_rname, rname);
                strcpy(l_rid, rid);
                l_rnum = rnum;
                n_atab = 0;
            }
            ap = &atab[n_atab];
            NAB_initatom(ap, 0);
            strcpy(ap->a_atomname, aname);
            ap->a_attr = 0;
            ap->a_residue = NULL;
            ap->a_pos[0] = x;
            ap->a_pos[1] = y;
            ap->a_pos[2] = z;
            ap->a_charge = q;
            ap->a_radius = r;
            ap->a_occ = occ;
            ap->a_bfact = bfact;
            n_atab++;

        } else if (strncmp("TER", line, 3) == 0) {  /* new strand */
            if (n_atab > 0) {
                strcpy(res.r_resname, l_rname);
                strcpy(res.r_resid, l_rid);
                res.r_num = l_rnum;
                res.r_natoms = n_atab;
                makebonds(&res);
                addresidue(mol, sname, &res);
                initres();
                n_atab = 0;
            }
            l_cid = '\0';
            *l_rname = '\0';
            *l_rid = '\0';
            l_rnum = 0;
        } else if (strncmp("END", line, 3) == 0)
            break;
    }
    if (n_atab > 0) {
        strcpy(res.r_resname, l_rname);
        strcpy(res.r_resid, l_rid);
        res.r_num = l_rnum;
        res.r_natoms = n_atab;
        makebonds(&res);
        addresidue(mol, sname, &res);
        initres();
    }

    mol->m_nvalid = 0;

    return (mol);
}

static int isnewres(char l_rname[], char rname[], int l_rnum, int rnum)
{

    return (strcmp(l_rname, rname) || l_rnum != rnum);
}

static void initres(void)
{

    NAB_initres(&res, 0);
    if (res.r_resname == NULL) {
        res.r_resname = (char *) malloc(R_NAME_SIZE * sizeof(char));
        if (res.r_resname == NULL) {
            fprintf(stderr, "initres: can't allocate res->r_resname.\n");
            exit(1);
        }
    }
    if (res.r_resid == NULL) {
        res.r_resid = (char *) malloc(R_NAME_SIZE * sizeof(char));
        if (res.r_resid == NULL) {
            fprintf(stderr, "initres: can't allocate res->r_resid.\n");
            exit(1);
        }
    }
    res.r_natoms = n_atab;
    res.r_atoms = atab;
}

static void init_atab(void)
{
    static int init = 1;
    int a;
    ATOM_T *ap;

    if (!init)
        return;
    for (ap = atab, a = 0; a < ATAB_SIZE; a++, ap++) {
        ap->a_atomname = (char *) malloc(A_NAME_SIZE * sizeof(char));
        if (ap->a_atomname == NULL) {
            fprintf(stderr, "init_atab: can't alloc a_atomname.\n");
            exit(1);
        }
        *ap->a_atomname = '\0';
        ap->a_atomtype = NULL;
        ap->a_element = NULL;
        ap->a_fullname = NULL;
    }
    init = 0;
}

static void makebonds(RESIDUE_T * res)
{
    int a1, a2;
    ATOM_T *ap1, *ap2;
    int ih1, ih2;
    REAL_T d;

    for (a1 = 0; a1 < res->r_natoms; a1++) {
        ap1 = &res->r_atoms[a1];
        ap1->a_nconnect = 0;
    }

    for (a1 = 0; a1 < res->r_natoms - 1; a1++) {
        ap1 = &res->r_atoms[a1];
        if (isdigit(*ap1->a_atomname))
            ih1 = ap1->a_atomname[1] == 'H' || ap1->a_atomname[1] == 'h';
        else
            ih1 = *ap1->a_atomname == 'H' || *ap1->a_atomname == 'h';
        for (a2 = a1 + 1; a2 < res->r_natoms; a2++) {
            ap2 = &res->r_atoms[a2];
            if (isdigit(*ap2->a_atomname))
                ih2 = ap2->a_atomname[1] == 'H' ||
                    ap2->a_atomname[1] == 'h';
            else
                ih2 = *ap2->a_atomname == 'H' || *ap2->a_atomname == 'h';
            d = dist(ap1, ap2);
            if (ih1 || ih2) {
                if (d <= BH_THRESH) {
                    if (ap1->a_nconnect < A_CONNECT_SIZE &&
                        ap2->a_nconnect < A_CONNECT_SIZE) {
                        ap1->a_connect[ap1->a_nconnect++] = a2;
                        ap2->a_connect[ap2->a_nconnect++] = a1;
                    }
                }
            } else if (d <= B_THRESH) {
                if (ap1->a_nconnect < A_CONNECT_SIZE &&
                    ap2->a_nconnect < A_CONNECT_SIZE) {
                    ap1->a_connect[ap1->a_nconnect++] = a2;
                    ap2->a_connect[ap2->a_nconnect++] = a1;
                }
            }
        }
    }
}

static REAL_T dist(ATOM_T * ap1, ATOM_T * ap2)
{
    REAL_T dx, dy, dz;

    dx = ap1->a_pos[0] - ap2->a_pos[0];
    dy = ap1->a_pos[1] - ap2->a_pos[1];
    dz = ap1->a_pos[2] - ap2->a_pos[2];
    return (sqrt(dx * dx + dy * dy + dz * dz));
}

static void fputpdb(FILE * fp, MOLECULE_T * mol, char *options)
     /*
        Options:

        -pqr:  add charges and radii after the xyz coordinates
        -nobocc:  don't add occupancies and b-factors after the xyz coordinates
        (implied if -pqr is present)
        -brook:  use Broohaven (aka pdb version2 )atom/residue names
        -wwpdb:  use wwpdb (aka pdb version 3)  atom/residue names
        -tr:  use residue numbers that do not restart at each chain
        -nocid:  do not put put chain id's in the output

      */
{
    int r, tr, rn, a, ta;
    char cid;
    STRAND_T *sp;
    RESIDUE_T *res;
    ATOM_T *ap;
    char rname[R_NAME_SIZE];
    char aname[A_NAME_SIZE];
    char rid[PDB_RID_SIZE];
    char loptions[256];
    int opt_pqr = 0;
    int opt_nobocc = 0;
    int opt_brook = 0;
    int opt_wwpdb = 0;
    int opt_tr = 0;
    int opt_nocid = 0;
    int opt_allcid = 0;
    CID_T *cidstate = NULL;

    if (!mol) {
        fprintf(stderr, "fputpdb: NULL molecule\n");
        return;
    }
    if (!fp) {
        fprintf(stderr, "fputpdb: NULL file pointer\n");
        return;
    }

    if (options == NULL)
        *loptions = '\0';
    else {
        strncpy(loptions, options, 255);
        loptions[255] = '\0';
    }
    opt_pqr = strstr(loptions, "-pqr") != NULL;
    opt_nobocc = strstr(loptions, "-nobocc") != NULL;
    opt_brook = strstr(loptions, "-brook") != NULL;
    opt_wwpdb = strstr(loptions, "-wwpdb") != NULL;
    opt_tr = strstr(loptions, "-tr") != NULL;
    opt_nocid = strstr(loptions, "-nocid") != NULL;
    opt_allcid = strstr(loptions, "-allcid") != NULL;
    if (opt_allcid)
        opt_nocid = 0;
    cidstate = initcid(opt_nocid, opt_allcid, mol);

/*
  cid = ( mol->m_nstrands > 1 ) ? 'A' : ' ';
*/

    for (ta = 0, tr = 0, sp = mol->m_strands; sp; sp = sp->s_next) {
        /*
         **     fprintf( fp, "REMARK %s strand has %d residues\n",
         **         sp->s_name, sp->s_nresidues );
         **     for( r = 0; r < sp->s_nresidues; r++ )
         **         fprintf( fp, "REMARK   RES %3d %s %d atoms\n",
         **             r + 1, sp->s_residues[ r ]->r_resname,
         **             sp->s_residues[ r ]->r_natoms );
         */
        cid = nextcid(cidstate, opt_nocid, opt_allcid, sp);
        for (r = 0; r < sp->s_nresidues; r++, tr++) {
            res = sp->s_residues[r];
            strcpy(rid, res->r_resid ? res->r_resid : "");
            if (opt_brook)
                mk_brook_rname(rname, res);
            else if (opt_wwpdb)
                mk_wwpdb_rname(rname, res);
            else
                strcpy(rname, res->r_resname);
            for (a = 0; a < res->r_natoms; a++) {
                ta++;
                ap = &res->r_atoms[a];

                if (opt_brook)
                    mk_brook_aname(aname, ap->a_atomname, rname);
                else if (opt_wwpdb)
                    mk_wwpdb_aname(aname, ap->a_atomname, rname);
                else
                    strcpy(aname, ap->a_atomname);

                if (!strcmp(rid, "") || opt_tr) {
                    /* id string is empty or "-tr" is set: use residue
                     ** numbers & chain id on output
                     ** amber numbers are total number of residues
                     ** without resetting at chain ids.  Ordinary
                     ** rn's begin at 1 for each chain
                     */
                    rn = opt_tr ? tr + 1 : r + 1;
                    if (ta < 100000) {
                        fprintf(fp,
                                "ATOM  %5d %-4s %-3s %c%4d    %8.3f%8.3f%8.3f",
                                ta, aname, rname,
/*
		      opt_nocid ? ' ' : cid, rn,
*/
                                cid, rn,
                                ap->a_pos[0], ap->a_pos[1], ap->a_pos[2]);
                    } else {
                        fprintf(fp,
                                "ATOM  %05d %-4s %-3s %c%4d    %8.3f%8.3f%8.3f",
                                ta % 100000, aname, rname,
/*
		      opt_nocid ? ' ' : cid, rn,
*/
                                cid, rn,
                                ap->a_pos[0], ap->a_pos[1], ap->a_pos[2]);
                    }
                } else {
                    /*  use the resid field from the input pdb file  */
                    if (ta < 100000) {
                        fprintf(fp,
                                "ATOM  %5d %-4s %3s %-6s   %8.3f%8.3f%8.3f",
                                ta, aname, rname, rid,
                                ap->a_pos[0], ap->a_pos[1], ap->a_pos[2]);
                    } else {
                        fprintf(fp,
                                "ATOM  %05d %-4s %3s %-6s   %8.3f%8.3f%8.3f",
                                ta % 100000, aname, rname, rid,
                                ap->a_pos[0], ap->a_pos[1], ap->a_pos[2]);
                    }
                }
                if (opt_pqr) {
                    /* this format matches Beroza stuff:  */
                    fprintf(fp, "%10.5f%10.5f   ",
                            ap->a_charge, ap->a_radius);
                } else if (!opt_nobocc) {
                    fprintf(fp, "%6.2f%6.2f           ",
                            ap->a_occ, ap->a_bfact);
                }

                if ((opt_brook || opt_wwpdb) && !opt_pqr && !opt_nobocc) {
                    fprintf(fp, "%.1s  ", ap->a_atomname);
                }

                fprintf(fp, "\n");

            }                   /* end loop over atoms in this residue  */

        }                       /* end loop over residues in this strand  */

        fprintf(fp, "TER\n");
/*
    if( mol->m_nstrands > 1 )
      cid++;
*/

    }                           /* end loop over strands  */
    freecid(cidstate);
}

static void freecid(CID_T * cid)
{

    if (cid != NULL) {
        if (cid->c_cids != NULL)
            free(cid->c_cids);
        free(cid);
    }
}

static CID_T *initcid(int nocid, int allcid, MOLECULE_T * mol)
{
    CID_T *cid = NULL;
    int c;
    STRAND_T *sp;
    int err = 0;

    if (nocid)
        return NULL;

    cid = (CID_T *) malloc(sizeof(CID_T));
    if (cid == NULL) {
        fprintf(stderr, "initcid: can't allocate cid\n");
        err = 1;
        goto CLEAN_UP;
    }
    cid->c_cids = (char *) malloc(128 * sizeof(char));
    if (cid->c_cids == NULL) {
        fprintf(stderr, "initcid: can't allocate c_cids\n");
        err = 1;
        goto CLEAN_UP;
    }
    cid->c_next = 'A';
    cid->c_last = 'Z';
    for (c = cid->c_next; c <= cid->c_last; c++)
        cid->c_cids[c] = 0;

    if (allcid) {
        if (mol == NULL) {
            fprintf(stderr, "initcid: NULL molecule\n");
            err = 1;
            goto CLEAN_UP;
        }
        for (sp = mol->m_strands; sp; sp = sp->s_next) {
            if (strlen(sp->s_strandname) == 1) {
                c = *sp->s_strandname;
                if (c >= 'A' && c <= 'Z')
                    cid->c_cids[c] = 1;
            }
        }
        for (; cid->c_next <= cid->c_last; cid->c_next++) {
            if (!cid->c_cids[cid->c_next])
                break;
        }
    }

  CLEAN_UP:;

    if (err) {
        freecid(cid);
        cid = NULL;
    }

    return cid;
}

static int nextcid(CID_T * cid, int nocid, int allcid, STRAND_T * sp)
{
    int c;

    if (nocid)
        return ' ';

    if (cid == NULL) {
        fprintf(stderr, "nextcid: NULL cid\n");
        return ' ';
    } else if (cid->c_next > cid->c_last)
        return ' ';
    else if (allcid) {
        if (strlen(sp->s_strandname) == 1) {
            c = *sp->s_strandname;
            if (c >= 'A' && c <= 'Z')
                return c;
        }
        c = cid->c_next;
        cid->c_cids[cid->c_next] = 1;
        for (++cid->c_next; cid->c_next <= cid->c_last; cid->c_next++) {
            if (!cid->c_cids[cid->c_next])
                break;
        }
        return c;
    } else {
        c = cid->c_next;
        cid->c_next++;
        return c;
    }
}

static void mk_brook_rname(char rname[], RESIDUE_T * res)
{

    strcpy(rname, res->r_resname);

    if (!strcmp(rname, "GUA"))
        strcpy(rname, "  G");
    if (!strcmp(rname, "ADE"))
        strcpy(rname, "  A");
    if (!strcmp(rname, "THY"))
        strcpy(rname, "  T");
    if (!strcmp(rname, "CYT"))
        strcpy(rname, "  C");
    if (!strcmp(rname, "URA"))
        strcpy(rname, "  U");

    if (!strcmp(rname, "DG"))
        strcpy(rname, "  G");
    if (!strcmp(rname, "DA"))
        strcpy(rname, "  A");
    if (!strcmp(rname, "DT"))
        strcpy(rname, "  T");
    if (!strcmp(rname, "DC"))
        strcpy(rname, "  C");

    if (!strcmp(rname, "RG"))
        strcpy(rname, "  G");
    if (!strcmp(rname, "RA"))
        strcpy(rname, "  A");
    if (!strcmp(rname, "RU"))
        strcpy(rname, "  U");
    if (!strcmp(rname, "RC"))
        strcpy(rname, "  C");

    if (!strcmp(rname, "DG3"))
        strcpy(rname, "  G");
    if (!strcmp(rname, "DA3"))
        strcpy(rname, "  A");
    if (!strcmp(rname, "DT3"))
        strcpy(rname, "  T");
    if (!strcmp(rname, "DC3"))
        strcpy(rname, "  C");

    if (!strcmp(rname, "RG3"))
        strcpy(rname, "  G");
    if (!strcmp(rname, "RA3"))
        strcpy(rname, "  A");
    if (!strcmp(rname, "RU3"))
        strcpy(rname, "  U");
    if (!strcmp(rname, "RC3"))
        strcpy(rname, "  C");

    if (!strcmp(rname, "DG5"))
        strcpy(rname, "  G");
    if (!strcmp(rname, "DA5"))
        strcpy(rname, "  A");
    if (!strcmp(rname, "DT5"))
        strcpy(rname, "  T");
    if (!strcmp(rname, "DC5"))
        strcpy(rname, "  C");

    if (!strcmp(rname, "RG5"))
        strcpy(rname, "  G");
    if (!strcmp(rname, "RA5"))
        strcpy(rname, "  A");
    if (!strcmp(rname, "RU5"))
        strcpy(rname, "  U");
    if (!strcmp(rname, "RC5"))
        strcpy(rname, "  C");

    if (!strcmp(rname, "HID"))
        strcpy(rname, "HIS");
    if (!strcmp(rname, "HID"))
        strcpy(rname, "HIS");
    if (!strcmp(rname, "HIP"))
        strcpy(rname, "HIS");
    if (!strcmp(rname, "CYX"))
        strcpy(rname, "CYS");
    if (!strcmp(rname, "ASH"))
        strcpy(rname, "ASP");
    if (!strcmp(rname, "GLH"))
        strcpy(rname, "GLU");

}

static void mk_wwpdb_rname(char rname[], RESIDUE_T * res)
{

    strcpy(rname, res->r_resname);

    if (!strcmp(rname, "GUA"))
        strcpy(rname, "  G");
    if (!strcmp(rname, "ADE"))
        strcpy(rname, "  A");
    if (!strcmp(rname, "THY"))
        strcpy(rname, "  T");
    if (!strcmp(rname, "CYT"))
        strcpy(rname, "  C");
    if (!strcmp(rname, "URA"))
        strcpy(rname, "  U");

    if (!strcmp(rname, "DG"))
        strcpy(rname, " DG");
    if (!strcmp(rname, "DA"))
        strcpy(rname, " DA");
    if (!strcmp(rname, "DT"))
        strcpy(rname, " DT");
    if (!strcmp(rname, "DC"))
        strcpy(rname, " DC");

    if (!strcmp(rname, "RG"))
        strcpy(rname, " RG");
    if (!strcmp(rname, "RA"))
        strcpy(rname, " RA");
    if (!strcmp(rname, "RU"))
        strcpy(rname, " RU");
    if (!strcmp(rname, "RC"))
        strcpy(rname, " RC");

    if (!strcmp(rname, "DG3"))
        strcpy(rname, " DG");
    if (!strcmp(rname, "DA3"))
        strcpy(rname, " DA");
    if (!strcmp(rname, "DT3"))
        strcpy(rname, " DT");
    if (!strcmp(rname, "DC3"))
        strcpy(rname, " DC");

    if (!strcmp(rname, "RG3"))
        strcpy(rname, " RG");
    if (!strcmp(rname, "RA3"))
        strcpy(rname, " RA");
    if (!strcmp(rname, "RU3"))
        strcpy(rname, " RU");
    if (!strcmp(rname, "RC3"))
        strcpy(rname, " RC");

    if (!strcmp(rname, "DG5"))
        strcpy(rname, " DG");
    if (!strcmp(rname, "DA5"))
        strcpy(rname, " DA");
    if (!strcmp(rname, "DT5"))
        strcpy(rname, " DT");
    if (!strcmp(rname, "DC5"))
        strcpy(rname, " DC");

    if (!strcmp(rname, "RG5"))
        strcpy(rname, " RG");
    if (!strcmp(rname, "RA5"))
        strcpy(rname, " RA");
    if (!strcmp(rname, "RU5"))
        strcpy(rname, " RU");
    if (!strcmp(rname, "RC5"))
        strcpy(rname, " RC");

    if (!strcmp(rname, "HID"))
        strcpy(rname, "HIS");
    if (!strcmp(rname, "HID"))
        strcpy(rname, "HIS");
    if (!strcmp(rname, "HIP"))
        strcpy(rname, "HIS");
    if (!strcmp(rname, "CYX"))
        strcpy(rname, "CYS");
    if (!strcmp(rname, "ASH"))
        strcpy(rname, "ASP");
    if (!strcmp(rname, "GLH"))
        strcpy(rname, "GLU");

}

static void mk_brook_aname(char aname[], char name[], char rname[])
{

    /* Following works for single-character element names, like H, C, N, O, S, P */


    aname[0] = ' ';             /* Brookhaven names start with space ....  */
    /*  ... except when a four-character name is "folded": */
    if (strlen(name) >= 4 && name[3] != ' ')
        aname[0] = name[3];
    aname[1] = name[0];
    aname[2] = name[1];
    aname[3] = name[2];
    if (aname[3] == '\'')
        aname[3] = '*';         /* for nucleic acids */
    aname[4] = '\0';

    /*  Now fold certain numbers back to the first space:  */
    if (!strncmp(aname, " H", 2) &&
        (aname[3] == '1' || aname[3] == '2' || aname[3] == '3') &&
        (!strncmp(aname, " HB", 3) ||
         (rname != "PHE" && rname != "TYR" && rname != "TRP"
          && strncmp(rname, "HI", 2)))) {
        aname[0] = aname[3];
        aname[3] = ' ';
    }

    /*   We should now figure out and process two-character element names, but
       will not do this now.....    */

}

static void mk_wwpdb_aname(char aname[], char name[], char rname[])
{

    /*
     *      ---convert atom names to closely resemble those used by the
     *         wwPDB in its "remdiated" files
     *       
     *      ---First, assume that there are no two-character element names
     *         (like Fe or Ca or Na).  Then, according to Brookhaven rules,
     *         column 13 will be blank, and the name will be left-justified
     *         starting in column 14.  UNLESS, the name is four characters
     *         long!  In that case, don't use the first blank.
     */

    if (strlen(name) >= 4 && name[3] != ' '){
        strncpy(aname, name, 5);
    } else {
        aname[0] = ' ';
        strncpy( aname+1, name, 3 );
        aname[4] = '\0';
    }
    /*  pad to 4 characters with blanks
    if( aname[1] == '\0' ) aname[1] = ' ';
    if( aname[2] == '\0' ) aname[2] = ' ';
    if( aname[3] == '\0' ) aname[3] = ' ';

    /*
     *  --- Special fixes where Amber nucleic acid atom names differ from
     *      version 3 pdb names:
     */

    if (!strncmp(aname, "H5'1", 4)) strncpy(aname, " H5'", 5);
    if (!strncmp(aname, "H5'2", 4)) strncpy(aname, "H5''", 5);
    if (!strncmp(aname, "H2'1", 4)) strncpy(aname, " H2'", 5);
    if (!strncmp(aname, "H2'2", 4)) strncpy(aname, "H2''", 5);
    if (!strncmp(aname, " O1P", 4)) strncpy(aname, " OP1", 5);
    if (!strncmp(aname, " O2P", 4)) strncpy(aname, " OP2", 5);
    if (!strncmp(aname, " H5T", 4)) strncpy(aname, "HO5'", 5);
    if (!strncmp(aname, " H3T", 4)) strncpy(aname, "HO3'", 5);

    /*
     *  --- Now, special case out the two-character element names:
     */

    if (!strncmp(aname, " Na+", 4) || !strncmp(aname, " NA+", 4) ||
        !strncmp(aname, " Fe ", 4) || !strncmp(aname, " FE ", 4) ||
        !strncmp(aname, " Cl ", 4) || !strncmp(aname, " CL ", 4) ||
        !strncmp(aname, " Zn ", 4) || !strncmp(aname, " ZN ", 4) ||
        !strncmp(aname, " Li+", 4) || !strncmp(aname, " LI+", 4) ||
        !strncmp(aname, " Ca+", 4) || !strncmp(aname, " CA+", 4) ||
        !strncmp(aname, " Mg+", 4) || !strncmp(aname, " MG+", 4) ||
        !strncmp(aname, " Br-", 4) || !strncmp(aname, " BR-", 4)) {
        aname[0] = aname[1];
        aname[1] = aname[2];
        aname[2] = aname[3];
        aname[3] = ' ';
    }

}

/*   Write out the molecule information in an "mmCIF-like" format; this
     should come close to being correct, but probably will not be acceptable
     to all mmCIF parsers.  For now, it should just be considered as a 
     prototype.  In particular, no changes are made to atom or residue
     names:  whatever is in the molecule will be used.                   */

static void fputcif(FILE * fp, char *blockId, MOLECULE_T * mol)
{
    int r, tr, a, ta, strandnum;
    char cid;
    STRAND_T *sp;
    RESIDUE_T *res;
    ATOM_T *ap;

    assert(mol);
    assert(fp);

    cid = (mol->m_nstrands > 1) ? 'A' : '.';

    /* simple mmCIF header: */

    fprintf(fp, "data_%s\n\n", blockId);
    fprintf(fp, "###########\n## ENTRY ##\n###########\n\n");
    fprintf(fp, "_entry.id       %s\n\n", blockId);
    fprintf(fp, "###############\n## ATOM_SITE ##\n###############\n\n");
    fprintf(fp, "loop_\n_atom_site.id\n_atom_site.label_atom_id\n");
    fprintf(fp, "_atom_site.label_comp_id\n_atom_site.label_asym_id\n");
    fprintf(fp, "_atom_site.auth_seq_id\n_atom_site.cartn_x\n");
    fprintf(fp, "_atom_site.cartn_y\n_atom_site.cartn_z\n");
    fprintf(fp, "_atom_site.label_entity_id\n_atom_site.label_seq_id\n");

    strandnum = 0;
    for (ta = 0, tr = 0, sp = mol->m_strands; sp; sp = sp->s_next) {
        strandnum++;
        for (r = 0; r < sp->s_nresidues; r++, tr++) {
            res = sp->s_residues[r];
            for (a = 0; a < res->r_natoms; a++) {
                ta++;
                ap = &res->r_atoms[a];


                /* following examples from NDB:
                 ** auth_seq_id is the "absolute" residue number;
                 ** label_seq_id starts over from 1 at each new "strand"  
                 */

                fprintf(fp,
                        "%5d %-4s %3s %c %4d %8.3f %8.3f %8.3f %3d %4d\n",
                        ta, ap->a_atomname, res->r_resname, cid, tr + 1,
                        ap->a_pos[0], ap->a_pos[1], ap->a_pos[2],
                        strandnum, r + 1);
            }
        }
        if (mol->m_nstrands > 1)
            cid++;
    }
}

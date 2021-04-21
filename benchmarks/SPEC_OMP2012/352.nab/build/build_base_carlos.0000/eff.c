/* 
 * eff.c: implement energy subroutines for 3 or 4 cartesian coordinates.
 *
 * Parallelization via OpenMP, creation of pair lists using a kd tree, and
 * optional calculation in 4D were added by Russ Brown (russ.brown@sun.com)
 */

/*
 * For OpenMP execution, energy values can fluctuate across repeat
 * executions when large molecular models and large numbers of cores are
 * used.  This effect can be minimized is NOREDUCE is defined.
 */

/***********************************************************************
                            ECONS()
************************************************************************/

/* Calculate the constrained energy and first derivatives. */

static
REAL_T econs(REAL_T * x, REAL_T * f)
{
   int i, foff, threadnum, numthreads;
   REAL_T e_cons, rx, ry, rz, rw;

   e_cons = 0.0;

   /* Parallel execution for OpenMP unless NOREDUCE or NOPAR is defined. */

#if !defined(NOREDUCE) && !defined(NOPAR) && (defined(SPEC_OMP) || defined(OPENMP))
#pragma omp parallel reduction (+: e_cons) private (i, rx, ry, rz, rw)
#endif
   {
     /*
      * For execution under OpenMP when NOREDUCE is not defined,
      * multi-threaded execution is possible because each thread
      * accesses its own region of the f array.  Hence, compute
      * an offset into the f array, and use the OpenMP thread number
      * and number of threads information.
      *
      * For execution under OpenMP when NOREDUCE is defined,
      * multi-threaded execution is not possible because all threads
      * share the same region of the f array and it is not possible
      * to parallelize and avoid race conditions.  Hence, do not
      * compute an offset into the f array, and get the thread number
      * and number of threads from the mytaskid and numtasks variables
      * that have been set to 0 and 1, respectively in the sff.c file.
      * Note that this case is identical to single-threaded execution.
      *
      * For execution under ScaLAPACK or MPI, each process has its
      * own copy of the f array.  Hence, do not compute an offset into
      * the f array, and get the thread number and number of threads
      * that have been stored in the mytaskid and numtasks variables
      * that have been set by the mpiinit() function of the sff.c file.
      *
      * The thread number and number of threads are used to determine
      * the specific iterations of the below for loop that will be
      * executed by specific OpenMP threads or MPI processes.
      */

#if (defined(SPEC_OMP) || defined(OPENMP)) && !defined(NOREDUCE)
     threadnum = omp_get_thread_num();
     numthreads = omp_get_num_threads();
     foff = dim * prm->Natom * threadnum;
#else
     threadnum = mytaskid;
     numthreads = numtasks;
     foff = 0;
#endif

     /*
      * Loop over all atoms.  Map loop indices onto OpenMP threads
      * or MPI tasks using (static, 1) scheduling.
      */

      for (i = threadnum; i < prm->Natom; i += numthreads) {
         if (constrained[i]) {
            rx = x[dim * i] - x0[dim * i];
            ry = x[dim * i + 1] - x0[dim * i + 1];
            rz = x[dim * i + 2] - x0[dim * i + 2];

            e_cons += wcons * (rx * rx + ry * ry + rz * rz);

            f[foff + dim * i] += 2. * wcons * rx;
            f[foff + dim * i + 1] += 2. * wcons * ry;
            f[foff + dim * i + 2] += 2. * wcons * rz;

            if (dim == 4) {
               rw = x[dim * i + 3] - x0[dim * i + 3];
               e_cons += wcons * rw * rw;
               f[foff + dim * i + 3] += 2. * wcons * rw;
            }
         }
      }
   }

   return (e_cons);
}

/***********************************************************************
                            EBOND()
************************************************************************/

/* Calculate the bond stretching energy and first derivatives.*/

static
REAL_T ebond(int nbond, int *a1, int *a2, int *atype,
             REAL_T * Rk, REAL_T * Req, REAL_T * x, REAL_T * f)
{
   int i, at1, at2, atyp, foff, threadnum, numthreads;
   REAL_T e_bond, r, rx, ry, rz, rw, r2, s, db, df, e;

   e_bond = 0.0;

   /* Parallel execution for OpenMP unless NOREDUCE or NOPAR is defined. */

#if !defined(NOREDUCE) && !defined(NOPAR) && (defined(SPEC_OMP) || defined(OPENMP))
#pragma omp parallel reduction (+: e_bond) \
  private (i, foff, at1, at2, atyp, threadnum, numthreads, \
           rx, ry, rz, rw, r2, s, r, db, df, e )
#endif
   {
     /*
      * For execution under OpenMP when NOREDUCE is not defined,
      * multi-threaded execution is possible because each thread
      * accesses its own region of the f array.  Hence, compute
      * an offset into the f array, and use the OpenMP thread number
      * and number of threads information.
      *
      * For execution under OpenMP when NOREDUCE is defined,
      * multi-threaded execution is not possible because all threads
      * share the same region of the f array and it is not possible
      * to parallelize and avoid race conditions.  Hence, do not
      * compute an offset into the f array, and get the thread number
      * and number of threads from the mytaskid and numtasks variables
      * that have been set to 0 and 1, respectively in the sff.c file.
      * Note that this case is identical to single-threaded execution.
      *
      * For execution under ScaLAPACK or MPI, each process has its
      * own copy of the f array.  Hence, do not compute an offset into
      * the f array, and get the thread number and number of threads
      * that have been stored in the mytaskid and numtasks variables
      * that have been set by the mpiinit() function of the sff.c file.
      *
      * The thread number and number of threads are used to determine
      * the specific iterations of the below for loop that will be
      * executed by specific OpenMP threads or MPI processes.
      */

#if (defined(SPEC_OMP) || defined(OPENMP)) && !defined(NOREDUCE)
     threadnum = omp_get_thread_num();
     numthreads = omp_get_num_threads();
     foff = dim * prm->Natom * threadnum;
#else
     threadnum = mytaskid;
     numthreads = numtasks;
     foff = 0;
#endif

      /*
       * Loop over all 1-2 bonds.  Map loop indices onto OpenMP threads
       * or MPI tasks using (static, 1) scheduling.
       */

      for (i = threadnum; i < nbond; i += numthreads) {

         at1 = dim * a1[i] / 3;
         at2 = dim * a2[i] / 3;
         atyp = atype[i] - 1;

         rx = x[at1] - x[at2];
         ry = x[at1 + 1] - x[at2 + 1];
         rz = x[at1 + 2] - x[at2 + 2];
         r2 = rx * rx + ry * ry + rz * rz;

         if (dim == 4) {
            rw = x[at1 + 3] - x[at2 + 3];
            r2 += rw * rw;
         }

         s = sqrt(r2);
         r = 2.0 / s;
         db = s - Req[atyp];
         df = Rk[atyp] * db;
         e = df * db;
         e_bond += e;
         df *= r;

         f[foff + at1 + 0] += rx * df;
         f[foff + at1 + 1] += ry * df;
         f[foff + at1 + 2] += rz * df;

         f[foff + at2 + 0] -= rx * df;
         f[foff + at2 + 1] -= ry * df;
         f[foff + at2 + 2] -= rz * df;

         if (dim == 4) {
            f[foff + at1 + 3] += rw * df;
            f[foff + at2 + 3] -= rw * df;
         }
      }
   }

   return (e_bond);
}

/***********************************************************************
                            EANGL()
************************************************************************/

/* Calculate the bond bending energy and first derivatives. */

static
REAL_T eangl(int nang, int *a1, int *a2, int *a3, int *atype,
             REAL_T * Tk, REAL_T * Teq, REAL_T * x, REAL_T * f)
{
   int i, atyp, at1, at2, at3, foff, threadnum, numthreads;
   REAL_T dxi, dyi, dzi, dwi, dxj, dyj, dzj, dwj, ri2, rj2, ri, rj, rir,
       rjr;
   REAL_T dxir, dyir, dzir, dwir, dxjr, dyjr, dzjr, dwjr, cst, at, da, df,
       e, e_theta;
   REAL_T xtmp, dxtmp, ytmp, wtmp, dytmp, ztmp, dztmp, dwtmp;

   e_theta = 0.0;

   /* Parallel execution for OpenMP unless NOREDUCE or NOPAR is defined. */

#if !defined(NOREDUCE) && !defined(NOPAR) && (defined(SPEC_OMP) || defined(OPENMP))
#pragma omp parallel reduction (+: e_theta) \
  private (i, foff, at1, at2, at3, atyp, threadnum, numthreads, \
           dxi, dyi, dzi, dwi, dxj, dyj, dzj, dwj, ri2, rj2, ri, rj, rir, rjr, \
           dxir, dyir, dzir, dwir, dxjr, dyjr, dzjr, dwjr, cst, at, da, df, e, \
           xtmp, dxtmp, ytmp, dytmp, ztmp, dztmp, wtmp, dwtmp)
#endif
   {
     /*
      * For execution under OpenMP when NOREDUCE is not defined,
      * multi-threaded execution is possible because each thread
      * accesses its own region of the f array.  Hence, compute
      * an offset into the f array, and use the OpenMP thread number
      * and number of threads information.
      *
      * For execution under OpenMP when NOREDUCE is defined,
      * multi-threaded execution is not possible because all threads
      * share the same region of the f array and it is not possible
      * to parallelize and avoid race conditions.  Hence, do not
      * compute an offset into the f array, and get the thread number
      * and number of threads from the mytaskid and numtasks variables
      * that have been set to 0 and 1, respectively in the sff.c file.
      * Note that this case is identical to single-threaded execution.
      *
      * For execution under ScaLAPACK or MPI, each process has its
      * own copy of the f array.  Hence, do not compute an offset into
      * the f array, and get the thread number and number of threads
      * that have been stored in the mytaskid and numtasks variables
      * that have been set by the mpiinit() function of the sff.c file.
      *
      * The thread number and number of threads are used to determine
      * the specific iterations of the below for loop that will be
      * executed by specific OpenMP threads or MPI processes.
      */

#if (defined(SPEC_OMP) || defined(OPENMP)) && !defined(NOREDUCE)
     threadnum = omp_get_thread_num();
     numthreads = omp_get_num_threads();
     foff = dim * prm->Natom * threadnum;
#else
     threadnum = mytaskid;
     numthreads = numtasks;
     foff = 0;
#endif

      /*
       * Loop over all 1-3 bonds.  Map loop indices onto OpenMP threads
       * or MPI tasks using (static, 1) scheduling.
       */

      for (i = threadnum; i < nang; i += numthreads) {

         at1 = dim * a1[i] / 3;
         at2 = dim * a2[i] / 3;
         at3 = dim * a3[i] / 3;
         atyp = atype[i] - 1;

         dxi = x[at1] - x[at2];
         dyi = x[at1 + 1] - x[at2 + 1];
         dzi = x[at1 + 2] - x[at2 + 2];

         dxj = x[at3] - x[at2];
         dyj = x[at3 + 1] - x[at2 + 1];
         dzj = x[at3 + 2] - x[at2 + 2];

         ri2 = dxi * dxi + dyi * dyi + dzi * dzi;
         rj2 = dxj * dxj + dyj * dyj + dzj * dzj;

         if (dim == 4) {
            dwi = x[at1 + 3] - x[at2 + 3];
            dwj = x[at3 + 3] - x[at2 + 3];
            ri2 += dwi * dwi;
            rj2 += dwj * dwj;
         }

         ri = sqrt(ri2);
         rj = sqrt(rj2);
         rir = 1. / ri;
         rjr = 1. / rj;

         dxir = dxi * rir;
         dyir = dyi * rir;
         dzir = dzi * rir;

         dxjr = dxj * rjr;
         dyjr = dyj * rjr;
         dzjr = dzj * rjr;

         cst = dxir * dxjr + dyir * dyjr + dzir * dzjr;

         if (dim == 4) {
            dwir = dwi * rir;
            dwjr = dwj * rjr;
            cst += dwir * dwjr;
         }

         if (cst > 1.0)
            cst = 1.0;
         if (cst < -1.0)
            cst = -1.0;

         at = acos(cst);
         da = at - Teq[atyp];
         df = da * Tk[atyp];
         e = df * da;
         e_theta = e_theta + e;
         df = df + df;
         at = sin(at);
         if (at > 0 && at < 1.e-3)
            at = 1.e-3;
         else if (at < 0 && at > -1.e-3)
            at = -1.e-3;
         df = -df / at;

         xtmp = df * rir * (dxjr - cst * dxir);
         dxtmp = df * rjr * (dxir - cst * dxjr);

         ytmp = df * rir * (dyjr - cst * dyir);
         dytmp = df * rjr * (dyir - cst * dyjr);

         ztmp = df * rir * (dzjr - cst * dzir);
         dztmp = df * rjr * (dzir - cst * dzjr);

         f[foff + at1 + 0] += xtmp;
         f[foff + at3 + 0] += dxtmp;
         f[foff + at2 + 0] -= xtmp + dxtmp;

         f[foff + at1 + 1] += ytmp;
         f[foff + at3 + 1] += dytmp;
         f[foff + at2 + 1] -= ytmp + dytmp;

         f[foff + at1 + 2] += ztmp;
         f[foff + at3 + 2] += dztmp;
         f[foff + at2 + 2] -= ztmp + dztmp;

         if (dim == 4) {
            wtmp = df * rir * (dwjr - cst * dwir);
            dwtmp = df * rjr * (dwir - cst * dwjr);
            f[foff + at1 + 3] += wtmp;
            f[foff + at3 + 3] += dwtmp;
            f[foff + at2 + 3] -= wtmp + dwtmp;
         }
      }
   }

   return (e_theta);
}

/***********************************************************************
                            EPHI()
************************************************************************/

/* Calculate the dihedral torsion energy and first derivatives. */

static
REAL_T ephi(int nphi, int *a1, int *a2, int *a3, int *a4, int *atype,
            REAL_T * Pk, REAL_T * Pn, REAL_T * Phase, REAL_T * x,
            REAL_T * f)
{
   REAL_T e, co, den, co1, uu, vv, uv, ax, bx, cx, ay, by, cy, az, bz, cz,
       aw, bw, cw;
   REAL_T a0x, b0x, c0x, a0y, b0y, c0y, a0z, b0z, c0z, a0w, b0w, c0w, a1x,
       b1x;
   REAL_T a1y, b1y, a1z, b1z, a1w, b1w, a2x, b2x, a2y, b2y, a2z, b2z, a2w,
       b2w;
   REAL_T dd1x, dd2x, dd3x, dd4x, dd1y, dd2y, dd3y, dd4y, dd1z, dd2z,
       dd3z, dd4z;
   REAL_T dd1w, dd2w, dd3w, dd4w;
   REAL_T df, aa, bb, cc, ab, bc, ac, cosq;
   REAL_T ktors, phase, e_tors;
   int i, at1, at2, at3, at4, atyp, foff, threadnum, numthreads;
   REAL_T ux, uy, uz, vx, vy, vz, delta, phi, dx1, dy1, dz1, yy, pi;
   pi = 3.1415927;

   e_tors = 0.0;

   /* Parallel execution for OpenMP unless NOREDUCE or NOPAR is defined. */

#if !defined(NOREDUCE) && !defined(NOPAR) && (defined(SPEC_OMP) || defined(OPENMP))
#pragma omp parallel reduction (+: e_tors) \
  private (i, at1, at2, at3, at4, atyp, ax, ay, az, aw, bx, by, bz, bw, \
           cx, cy, cz, cw, ab, bc, ac, aa, bb, cc, uu, vv, uv, den, co, co1, \
           a0x, a0y, a0z, a0w, b0x, b0y, b0z, b0w, c0x, c0y, c0z, c0w, \
           a1x, a1y, a1z, a1w, b1x, b1y, b1z, b1w, a2x, a2y, a2z, a2w, \
           b2x, b2y, b2z, b2w, dd1x, dd1y, dd1z, dd1w, dd2x, dd2y, dd2z, dd2w, \
           dd3x, dd3y, dd3z, dd3w, dd4x, dd4y, dd4z, dd4w, phi, \
           ux, uy, uz, vx, vy, vz, dx1, dy1, dz1, delta, df, e, yy, phase, \
           ktors, cosq, threadnum, numthreads, foff)
#endif
   {
     /*
      * For execution under OpenMP when NOREDUCE is not defined,
      * multi-threaded execution is possible because each thread
      * accesses its own region of the f array.  Hence, compute
      * an offset into the f array, and use the OpenMP thread number
      * and number of threads information.
      *
      * For execution under OpenMP when NOREDUCE is defined,
      * multi-threaded execution is not possible because all threads
      * share the same region of the f array and it is not possible
      * to parallelize and avoid race conditions.  Hence, do not
      * compute an offset into the f array, and get the thread number
      * and number of threads from the mytaskid and numtasks variables
      * that have been set to 0 and 1, respectively in the sff.c file.
      * Note that this case is identical to single-threaded execution.
      *
      * For execution under ScaLAPACK or MPI, each process has its
      * own copy of the f array.  Hence, do not compute an offset into
      * the f array, and get the thread number and number of threads
      * that have been stored in the mytaskid and numtasks variables
      * that have been set by the mpiinit() function of the sff.c file.
      *
      * The thread number and number of threads are used to determine
      * the specific iterations of the below for loop that will be
      * executed by specific OpenMP threads or MPI processes.
      */

#if (defined(SPEC_OMP) || defined(OPENMP)) && !defined(NOREDUCE)
     threadnum = omp_get_thread_num();
     numthreads = omp_get_num_threads();
     foff = dim * prm->Natom * threadnum;
#else
     threadnum = mytaskid;
     numthreads = numtasks;
     foff = 0;
#endif

      /*
       * Loop over all 1-4 bonds.  Map loop indices onto OpenMP threads
       * or MPI tasks using (static, 1) scheduling.
       */

      for (i = threadnum; i < nphi; i += numthreads) {

         at1 = dim * a1[i] / 3;
         at2 = dim * a2[i] / 3;
         at3 = dim * abs(a3[i]) / 3;
         at4 = dim * abs(a4[i]) / 3;
         atyp = atype[i] - 1;

         ax = x[at2 + 0] - x[at1 + 0];
         ay = x[at2 + 1] - x[at1 + 1];
         az = x[at2 + 2] - x[at1 + 2];

         bx = x[at3 + 0] - x[at2 + 0];
         by = x[at3 + 1] - x[at2 + 1];
         bz = x[at3 + 2] - x[at2 + 2];

         cx = x[at4 + 0] - x[at3 + 0];
         cy = x[at4 + 1] - x[at3 + 1];
         cz = x[at4 + 2] - x[at3 + 2];

         if (dim == 4) {
            aw = x[at2 + 3] - x[at1 + 3];
            bw = x[at3 + 3] - x[at2 + 3];
            cw = x[at4 + 3] - x[at3 + 3];

#     define DOT4(a,b,c,d,e,f,g,h) a*e + b*f + c*g + d*h

            ab = DOT4(ax, ay, az, aw, bx, by, bz, bw);
            bc = DOT4(bx, by, bz, bw, cx, cy, cz, cw);
            ac = DOT4(ax, ay, az, aw, cx, cy, cz, cw);
            aa = DOT4(ax, ay, az, aw, ax, ay, az, aw);
            bb = DOT4(bx, by, bz, bw, bx, by, bz, bw);
            cc = DOT4(cx, cy, cz, cw, cx, cy, cz, cw);
         } else {

#     define DOT3(a,b,c,d,e,f) a*d + b*e + c*f

            ab = DOT3(ax, ay, az, bx, by, bz);
            bc = DOT3(bx, by, bz, cx, cy, cz);
            ac = DOT3(ax, ay, az, cx, cy, cz);
            aa = DOT3(ax, ay, az, ax, ay, az);
            bb = DOT3(bx, by, bz, bx, by, bz);
            cc = DOT3(cx, cy, cz, cx, cy, cz);
         }

         uu = (aa * bb) - (ab * ab);
         vv = (bb * cc) - (bc * bc);
         uv = (ab * bc) - (ac * bb);
         den = 1.0 / sqrt(uu * vv);
         co = uv * den;
         co1 = 0.5 * co * den;

         a0x = -bc * bx + bb * cx;
         a0y = -bc * by + bb * cy;
         a0z = -bc * bz + bb * cz;

         b0x = ab * cx + bc * ax - 2. * ac * bx;
         b0y = ab * cy + bc * ay - 2. * ac * by;
         b0z = ab * cz + bc * az - 2. * ac * bz;

         c0x = ab * bx - bb * ax;
         c0y = ab * by - bb * ay;
         c0z = ab * bz - bb * az;

         a1x = 2. * uu * (-cc * bx + bc * cx);
         a1y = 2. * uu * (-cc * by + bc * cy);
         a1z = 2. * uu * (-cc * bz + bc * cz);

         b1x = 2. * uu * (bb * cx - bc * bx);
         b1y = 2. * uu * (bb * cy - bc * by);
         b1z = 2. * uu * (bb * cz - bc * bz);

         a2x = -2. * vv * (bb * ax - ab * bx);
         a2y = -2. * vv * (bb * ay - ab * by);
         a2z = -2. * vv * (bb * az - ab * bz);

         b2x = 2. * vv * (aa * bx - ab * ax);
         b2y = 2. * vv * (aa * by - ab * ay);
         b2z = 2. * vv * (aa * bz - ab * az);

         dd1x = (a0x - a2x * co1) * den;
         dd1y = (a0y - a2y * co1) * den;
         dd1z = (a0z - a2z * co1) * den;

         dd2x = (-a0x - b0x - (a1x - a2x - b2x) * co1) * den;
         dd2y = (-a0y - b0y - (a1y - a2y - b2y) * co1) * den;
         dd2z = (-a0z - b0z - (a1z - a2z - b2z) * co1) * den;

         dd3x = (b0x - c0x - (-a1x - b1x + b2x) * co1) * den;
         dd3y = (b0y - c0y - (-a1y - b1y + b2y) * co1) * den;
         dd3z = (b0z - c0z - (-a1z - b1z + b2z) * co1) * den;

         dd4x = (c0x - b1x * co1) * den;
         dd4y = (c0y - b1y * co1) * den;
         dd4z = (c0z - b1z * co1) * den;

         if (dim == 4) {
            a0w = -bc * bw + bb * cw;
            b0w = ab * cw + bc * aw - 2. * ac * bw;
            c0w = ab * bw - bb * aw;
            a1w = 2. * uu * (-cc * bw + bc * cw);
            b1w = 2. * uu * (bb * cw - bc * bw);
            a2w = -2. * vv * (bb * aw - ab * bw);
            b2w = 2. * vv * (aa * bw - ab * aw);
            dd1w = (a0w - a2w * co1) * den;
            dd2w = (-a0w - b0w - (a1w - a2w - b2w) * co1) * den;
            dd3w = (b0w - c0w - (-a1w - b1w + b2w) * co1) * den;
            dd4w = (c0w - b1w * co1) * den;
         }

         if (prm->Nhparm && a3[i] < 0) {

            /*   here we will use a quadratic form for the improper torsion  */
            /*     we are using the NHPARM variable in prmtop to trigger this   */
            /*
               WARNING: phi itself is here calculated from the first three coords--
               --- may fail!
             */

            /* Note: The following improper torsion code does not support 4D! */

            co = co > 1.0 ? 1.0 : co;
            co = co < -1.0 ? -1.0 : co;

            phi = acos(co);

            /*
               now calculate sin(phi) because cos(phi) is symmetric, so
               we can decide between +-phi.
             */

            ux = ay * bz - az * by;
            uy = az * bx - ax * bz;
            uz = ax * by - ay * bx;

            vx = by * cz - bz * cy;
            vy = bz * cx - bx * cz;
            vz = bx * cy - by * cx;

            dx1 = uy * vz - uz * vy;
            dy1 = uz * vx - ux * vz;
            dz1 = ux * vy - uy * vx;

            dx1 = DOT3(dx1, dy1, dz1, bx, by, bz);
            if (dx1 < 0.0)
               phi = -phi;

            delta = phi - Phase[atyp];
            delta = delta > pi ? pi : delta;
            delta = delta < -pi ? -pi : delta;

            df = Pk[atyp] * delta;
            e = df * delta;
            e_tors += e;
            yy = sin(phi);

            /*
               Decide what expansion to use
               Check first for the "normal" expression, since it will be
               the most used

               the 0.001 value could be lowered for increased precision.
               This insures ~1e-05% error for sin(phi)=0.001
             */

            if (fabs(yy) > 0.001) {
               df = -2.0 * df / yy;
            } else {
               if (fabs(delta) < 0.10) {
                  if (Phase[atyp] == 0.0) {
                     df = -2.0 * Pk[atyp] * (1 + phi * phi / 6.0);
                  } else {
                     if (fabs(Phase[atyp]) == pi) {
                        df = 2.0 * Pk[atyp] * (1 + delta * delta / 6.0);
                     }
                  }
               } else {
                  if ((phi > 0.0 && phi < (pi / 2.0)) ||
                      (phi < 0.0 && phi > -pi / 2.0))
                     df = df * 1000.;
                  else
                     df = -df * 1000.;
               }
            }
         } else {
          multi_term:

            if (fabs(Phase[atyp] - 3.142) < 0.01)
               phase = -1.0;
            else
               phase = 1.0;

            ktors = Pk[atyp];
            switch ((int) fabs(Pn[atyp])) {

            case 1:
               e = ktors * (1.0 + phase * co);
               df = phase * ktors;
               break;
            case 2:
               e = ktors * (1.0 + phase * (2. * co * co - 1.));
               df = phase * ktors * 4. * co;
               break;
            case 3:
               cosq = co * co;
               e = ktors * (1.0 + phase * co * (4. * cosq - 3.));
               df = phase * ktors * (12. * cosq - 3.);
               break;
            case 4:
               cosq = co * co;
               e = ktors * (1.0 + phase * (8. * cosq * (cosq - 1.) + 1.));
               df = phase * ktors * co * (32. * cosq - 16.);
               break;
            case 6:
               cosq = co * co;
               e = ktors * (1.0 + phase * (32. * cosq * cosq * cosq -
                                           48. * cosq * cosq +
                                           18. * cosq - 1.));
               df = phase * ktors * co * (192. * cosq * cosq -
                                          192. * cosq + 36.);
               break;
            default:
               fprintf(stderr,
                       "bad value for Pn: %d %d %d %d %8.3f\n", at1,
                       at2, at3, at4, Pn[atyp]);
               exit(1);
            }
            e_tors += e;

         }

         f[foff + at1 + 0] += df * dd1x;
         f[foff + at1 + 1] += df * dd1y;
         f[foff + at1 + 2] += df * dd1z;

         f[foff + at2 + 0] += df * dd2x;
         f[foff + at2 + 1] += df * dd2y;
         f[foff + at2 + 2] += df * dd2z;

         f[foff + at3 + 0] += df * dd3x;
         f[foff + at3 + 1] += df * dd3y;
         f[foff + at3 + 2] += df * dd3z;

         f[foff + at4 + 0] += df * dd4x;
         f[foff + at4 + 1] += df * dd4y;
         f[foff + at4 + 2] += df * dd4z;

         if (dim == 4) {
            f[foff + at1 + 3] += df * dd1w;
            f[foff + at2 + 3] += df * dd2w;
            f[foff + at3 + 3] += df * dd3w;
            f[foff + at4 + 3] += df * dd4w;
         }
#ifdef PRINT_EPHI
         fprintf(nabout, "%4d%4d%4d%4d%4d%8.3f\n", i + 1, at1, at2, at3,
                 at4, e);
         fprintf(nabout,
                 "%10.5f%10.5f%10.5f%10.5f%10.5f%10.5f%10.5f%10.5f\n",
                 -df * dd1x, -df * dd1y, -df * dd1z, -df * dd2x,
                 -df * dd2y, -df * dd2z, -df * dd3x, -df * dd3y);
         fprintf(nabout, "%10.5f%10.5f%10.5f%10.5f\n", -df * dd3z,
                 -df * dd4x, -df * dd4y, -df * dd4z);
#endif
         if (Pn[atyp] < 0.0) {
            atyp++;
            goto multi_term;
         }
      }
   }

   return (e_tors);
}

/***********************************************************************
                            NBOND()
************************************************************************/

/* 
 * Calculate the non-bonded energy and first derivatives.
 * This function is complicated by the fact that it must
 * process two forms of pair lists: the 1-4 pair list and
 * the non-bonded pair list.  The non-bonded pair list
 * must be modified by the excluded atom list whereas the
 * 1-4 pair list is used unmodified.  Also, the non-bonded
 * pair list comprises lower and upper triangles whereas
 * the 1-4 pair list comprises an upper triangle only.
 *
 * Calling parameters are as follows:
 *
 * lpears - the number of pairs on the lower triangle pair list
 * upears - the number of pairs on the upper trianble pair list
 * pearlist - either the 1-4 pair list or the non-bonded pair list
 * N14 - set to 0 for the non-bonded pair list, 1 for the 1-4 pair list
 * x - the atomic coordinate array
 * f - the gradient vector
 * enb - Van der Waals energy return value, passed by reference
 * eel - Coulombic energy return value, passed by reference
 * enbfac - scale factor for Van der Waals energy
 * eelfac - scale factor for Coulombic energy
 */

static
int nbond(int *lpears, int *upears, int **pearlist, int N14,
          REAL_T * x, REAL_T * f, REAL_T * enb, REAL_T * eel,
          REAL_T enbfac, REAL_T eelfac)
{
   int i, j, i34, j34, k, ic, npr, lpair, iaci, foff, threadnum, numthreads;
   int *iexw;
   REAL_T dumx, dumy, dumz, dumw, cgi, r2inv, df2, r6, r10, f1, f2;
   REAL_T dedx, dedy, dedz, dedw, df, enbfaci, eelfaci, evdw, elec;
   REAL_T xi, yi, zi, wi, xj, yj, zj, wj, xij, yij, zij, wij, r, r2;
   REAL_T dis, kij, d0, diff, rinv, rs, rssq, eps1, epsi, cgijr, pow;
   int ibig, isml;

#define SIG 0.3
#define DIW 78.0
#define C1 38.5

   evdw = 0.;
   elec = 0.;
   enbfaci = 1. / enbfac;
   eelfaci = 1. / eelfac;

   /*
    * If NOREDUCE or NOPAR is defined, do not execute in parallel
    * under OpenMP for the 1-4 nonbonded list.  When NOREDUCE is
    * defined, all OpenMP tasks share one copy of the f array,
    * and because the 1-4 pair list is in upper triangular form
    * only, it is not possible to avoid race conditions when
    * updating the f array, so execution is single-threaded.
    * When NOPAR is defined, execution is single-threaded as well.
    */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
#if defined(NOREDUCE) || defined(NOPAR)
#pragma omp parallel if (N14 == 0) \
  reduction (+: evdw, elec) \
  private (i, j, iexw, npr, iaci, \
           xi, yi, zi, wi, xij, yij, zij, wij, dumx, dumy, dumz, dumw, \
           cgi, k, r2, r2inv, r, rinv, rs, rssq, pow, \
           eps1, epsi, cgijr, df2, ic, r6, f2, f1, df, dis, d0, kij, \
           diff, ibig, isml, dedx, dedy, dedz, dedw, r10, \
           threadnum, numthreads, foff, lpair, i34, j34, xj, yj, zj, wj)
#else
#pragma omp parallel \
  reduction (+: evdw, elec) \
  private (i, j, iexw, npr, iaci, \
           xi, yi, zi, wi, xij, yij, zij, wij, dumx, dumy, dumz, dumw, \
           cgi, k, r2, r2inv, r, rinv, rs, rssq, pow, \
           eps1, epsi, cgijr, df2, ic, r6, f2, f1, df, dis, d0, kij, \
           diff, ibig, isml, dedx, dedy, dedz, dedw, r10, \
           threadnum, numthreads, foff, lpair, i34, j34, xj, yj, zj, wj)
#endif
#endif /* SPEC_OMP || (!SPEC && OPENMP) */
   {
     /*
      * For execution under OpenMP when NOREDUCE is not defined,
      * multi-threaded execution is possible because each thread
      * accesses its own region of the f array.  Hence, compute
      * an offset into the f array, and use the OpenMP thread number
      * and number of threads information.
      *
      * For execution under OpenMP when NOREDUCE is defined,
      * multi-threaded execution is not possible because all threads
      * share the same region of the f array and it is not possible
      * to parallelize and avoid race conditions.  Hence, do not
      * compute an offset into the f array, and get the thread number
      * and number of threads from the mytaskid and numtasks variables
      * that have been set to 0 and 1, respectively in the sff.c file.
      * Note that this case is identical to single-threaded execution.
      *
      * For execution under ScaLAPACK or MPI, each process has its
      * own copy of the f array.  Hence, do not compute an offset into
      * the f array, and get the thread number and number of threads
      * that have been stored in the mytaskid and numtasks variables
      * that have been set by the mpiinit() function of the sff.c file.
      *
      * The thread number and number of threads are used to determine
      * the specific iterations of the below for loop that will be
      * executed by specific OpenMP threads or MPI processes.
      */

#if (defined(SPEC_OMP) || defined(OPENMP)) && !defined(NOREDUCE)
     threadnum = omp_get_thread_num();
     numthreads = omp_get_num_threads();
     foff = dim * prm->Natom * threadnum;
#else
     threadnum = mytaskid;
     numthreads = numtasks;
     foff = 0;
#endif

      /*
       * Allocate and initialize the iexw array used for skipping excluded
       * atoms.  Note that because of the manner in which iexw is used, it
       * is necessary to initialize it before only the first iteration of
       * the following loop.
       */

      iexw = ivector(-1, prm->Natom);
      for (i = -1; i < prm->Natom; i++) {
         iexw[i] = -1;
      }

      /*
       * Loop over all atoms i except for the final atom.
       *
       * If OPENMP and NOREDUCE are defined, this (i,j) loop nest will
       * update f[i34 + 0..3] only, except when L14 != 0.
       *
       * For MPI or ScaLAPACK, explicitly assign tasks to loop indices
       * for the following loop in a manner equivalent to (static, N)
       * scheduling for OpenMP.  For OpenMP use (dynamic, N) scheduling.
       *
       * Synchronization of OpenMP threads will occur following this loop
       * because the parallel region ends after this loop.  Following
       * synchronization, a reduction of the sumdeijda array will be
       * performed.
       *
       * Synchronization of MPI tasks will occur via the MPI_Allreduce
       * function that is called from within mme34.
       */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
#pragma omp for schedule(dynamic, blocksize)
#endif
      for (i = 0; i < prm->Natom - 1; i++) {

#if defined(MPI) || defined(SCALAPACK)
         if (!myroc(i, blocksize, numthreads, threadnum))
            continue;
#endif

         /* Check whether there are any atoms j on the pair list of atom i. */

         npr = upears[i];
         if (npr <= 0)
            continue;

         iaci = prm->Ntypes * (prm->Iac[i] - 1);
         cgi = eelfaci * prm->Charges[i];

         dumx = dumy = dumz = 0.0;

	 i34 = dim * i;

         xi = x[i34 + 0];
         yi = x[i34 + 1];
         zi = x[i34 + 2];

         if (dim == 4) {
            dumw = 0.0;
            wi = x[i34 + 3];
         }

         /*
          * Expand the excluded list into the iexw array by storing i
          * at array address j.
          */

         for (j = 0; j < prm->Iblo[i]; j++) {
            iexw[IexclAt[i][j] - 1] = i;
         }

         /*
          * If the 'N14' calling parameter is clear, use the beginning
          * address of the upper triangle pair list, which happens
          * to be the number of atoms on the lower triangle pair list.
          * If the 'N14' calling parameter is set, the beginning
          * address is zero because no lower triangle pair list is
          * used for the N14 interactions.
          */

         if (N14 == 0) {
            lpair = lpears[i];
         } else {
            lpair = 0;
         }

         /* Select atom j from the pair list.  Non-graceful error handling. */

         for (k = 0; k < npr; k++) {

            if (pearlist[i] == NULL) {
               fprintf(nabout,
                       "NULL pair list entry in nbond loop 1, taskid = %d\n",
                       mytaskid);
               fflush(nabout);
            }
            j = pearlist[i][lpair + k];

	    j34 = dim * j;

            /*
             * If the 'N14' calling parameter is clear, check whether
             * this i,j pair is exempted by the excluded atom list.
             */

            if (N14 != 0 || iexw[j] != i) {
               xij = xi - x[j34 + 0];
               yij = yi - x[j34 + 1];
               zij = zi - x[j34 + 2];
               r2 = xij * xij + yij * yij + zij * zij;

               if (dim == 4) {
                  wij = wi - x[j34 + 3];
                  r2 += wij * wij;
               }

               r2inv = 1.0 / r2;
               r = sqrt(r2);
               rinv = r * r2inv;

               /* Calculate the energy and derivatives according to dield. */

               if (dield == -3) {

                  /* special code Ramstein & Lavery dielectric, 94 force field */

                  rs = SIG * r;
                  rssq = rs * rs;
                  pow = exp(-rs);
                  eps1 = rssq + rs + rs + 2.0;
                  epsi = 1.0 / (DIW - C1 * pow * eps1);
                  cgijr = cgi * prm->Charges[j] * rinv * epsi;
                  elec += cgijr;
                  df2 = -cgijr * (1.0 + C1 * pow * rs * rssq * epsi);
                  ic = prm->Cno[iaci + prm->Iac[j] - 1] - 1;
                  if (ic >= 0) {
                     r6 = r2inv * r2inv * r2inv;
                     f2 = prm->Cn2[ic] * r6;
                     f1 = prm->Cn1[ic] * r6 * r6;
                     evdw += (f1 - f2) * enbfaci;
                     df = (df2 + (6.0 * f2 - 12.0 * f1) * enbfaci) * rinv;
                  } else {
                     df = df2 * rinv;
                  }

               } else if (dield == -4) {

                  /* distance-dependent dielectric code, 94 ff */
                  /* epsilon = r  */

                  rs = cgi * prm->Charges[j] * r2inv;
                  df2 = -2.0 * rs;
                  elec += rs;
                  ic = prm->Cno[iaci + prm->Iac[j] - 1] - 1;
                  if (ic >= 0) {
                     r6 = r2inv * r2inv * r2inv;
                     f2 = prm->Cn2[ic] * r6;
                     f1 = prm->Cn1[ic] * r6 * r6;
                     evdw += (f1 - f2) * enbfaci;
                     df = (df2 + (6.0 * f2 - 12.0 * f1) * enbfaci) * rinv;
                  } else {
                     df = df2 * rinv;
                  }

               } else if (dield == -5) {

                  /* non-bonded term from yammp  */

                  dis = r;
                  ic = prm->Cno[iaci + prm->Iac[j] - 1] - 1;
                  d0 = prm->Cn2[ic];
                  if (dis < d0) {
                     kij = prm->Cn1[ic];
                     diff = dis - d0;
                     evdw += kij * diff * diff;
                     df = 2.0 * kij * diff;
                  } else {
                     df = 0.0;
                  }
               } else {

                  /*
                   * Code for various dielectric models.
                   * The df2 variable should hold r(dV/dr).
                   */

                  if (dield == 0) {

                     /* epsilon = r  */

                     rs = cgi * prm->Charges[j] * r2inv;
                     df2 = -2.0 * rs;
                     elec += rs;

                  } else if (dield == 1) {

                     /* epsilon = 1  */

                     rs = cgi * prm->Charges[j] * rinv;
                     df2 = -rs;
                     elec += rs;

                  } else if (dield == -2) {

                     /* Ramstein & Lavery dielectric, PNAS 85, 7231 (1988). */

                     rs = SIG * r;
                     rssq = rs * rs;
                     pow = exp(-rs);
                     eps1 = rssq + rs + rs + 2.0;
                     epsi = 1.0 / (DIW - C1 * pow * eps1);
                     cgijr = cgi * prm->Charges[j] * rinv * epsi;
                     elec += cgijr;
                     df2 = -cgijr * (1.0 + C1 * pow * rs * rssq * epsi);
                  }

                  /* Calculate either Van der Waals or hydrogen bonded term. */

                  ic = prm->Cno[iaci + prm->Iac[j] - 1];
                  if (ic > 0 || enbfac != 1.0) {
                     if (ic > 0) {
                        ic--;
                     } else {
                        ibig = prm->Iac[i] > prm->Iac[j] ?
                            prm->Iac[i] : prm->Iac[j];
                        isml = prm->Iac[i] > prm->Iac[j] ?
                            prm->Iac[j] : prm->Iac[i];
                        ic = ibig * (ibig - 1) / 2 + isml - 1;
                     }
                     r6 = r2inv * r2inv * r2inv;
                     f2 = prm->Cn2[ic] * r6;
                     f1 = prm->Cn1[ic] * r6 * r6;
                     evdw += (f1 - f2) * enbfaci;
                     df = (df2 + (6.0 * f2 - 12.0 * f1) * enbfaci) * rinv;
#if 0
                     if (enbfac != 1.0)
                        nb14 += (f1 - f2) * enbfaci;
#endif
                  } else {
                     ic = -ic - 1;
                     r10 = r2inv * r2inv * r2inv * r2inv * r2inv;
                     f2 = prm->HB10[ic] * r10;
                     f1 = prm->HB12[ic] * r10 * r2inv;
                     evdw += (f1 - f2) * enbfaci;
                     df = (df2 + (10.0 * f2 - 12.0 * f1) * enbfaci) * rinv;
#if 0
                     hbener += (f1 - f2) * enbfaci;
#endif
                  }
               }

               /*
                * The df term contains one more factor of Dij in the denominator
                * so that terms such as dedx do not need to include 1/Dij. 
                *
                * Update the gradient for atom j.
                */

               df *= rinv;

               dedx = df * xij;
               dedy = df * yij;
               dedz = df * zij;

               dumx += dedx;
               dumy += dedy;
               dumz += dedz;

	       /*
		* Update the gradient for the 1-4 pair list or
		* if either OPENMP or NOREDUCE is not defined.
		*/

	       if (N14 != 0) {
		 f[foff + j34 + 0] -= dedx;
		 f[foff + j34 + 1] -= dedy;
		 f[foff + j34 + 2] -= dedz;
	       } else {
#if !(defined(SPEC_OMP) || defined(OPENMP)) || !defined(NOREDUCE)
		 f[foff + j34 + 0] -= dedx;
		 f[foff + j34 + 1] -= dedy;
		 f[foff + j34 + 2] -= dedz;
#endif
	       }

               if (dim == 4) {
                  dedw = df * wij;
                  dumw += dedw;
		  if (N14 != 0) {
		    f[foff + j34 + 3] -= dedw;
		  } else {
#if !(defined(SPEC_OMP) || defined(OPENMP)) || !defined(NOREDUCE)
		    f[foff + j34 + 3] -= dedw;
#endif
		  }
               }
            }
         }

         /* For atom i, the gradient is updated in the i-loop only. */

         f[foff + i34 + 0] += dumx;
         f[foff + i34 + 1] += dumy;
         f[foff + i34 + 2] += dumz;

         if (dim == 4) {
            f[foff + i34 + 3] += dumw;
         }
      }

      /*
       * If OPENMP and NOREDUCE are defined and N14 == 0, execute
       * a (j,i) loop nest to update f[j34 + 0..3].
       *
       * Because the (j,i) loop nest uses the same thread-to-index
       * mapping in the outer loop as does the prior (i,j) loop nest,
       * no thread synchronization is required between the two nests.
       */

#if (defined(SPEC_OMP) || defined(OPENMP)) && defined(NOREDUCE)

      if (N14 == 0) {

	/*
	 * Initialize the iexw array used for skipping excluded atoms.
	 * Note that because of the manner in which iexw is used, it is
	 * necessary to initialize it before only the first iteration of
	 * the following loop.
	 */

	for (i = -1; i < prm->Natom; i++) {
	  iexw[i] = -1;
	}

	/*
	 * Loop over all atoms j except for the first atom.
	 *
	 * Because OPENMP and NOREDUCE are defined, this (j,i) loop nest will
	 * update f[j34 + 0..3].
	 *
	 * For MPI or ScaLAPACK, explicitly assign tasks to loop indices
	 * for the following loop in a manner equivalent to (static, N)
	 * scheduling for OpenMP.  For OpenMP use (dynamic, N) scheduling.
	 *
	 * Synchronization of OpenMP threads will occur following this loop
	 * because the parallel region ends after this loop.  Following
	 * synchronization, a reduction of the sumdeijda array will be
	 * performed.
	 *
	 * Synchronization of MPI tasks will occur via the MPI_Allreduce
	 * function that is called from within mme34.
	 */

#if defined(SPEC_OMP) || (!defined(SPEC) || defined(OPENMP))
#pragma omp for schedule(dynamic, blocksize)
#endif
	for (j = 1; j < prm->Natom; j++) {

	  /* Check whether there are any atoms i on the pair list of atom j. */

	  npr = lpears[j];
	  if (npr <= 0)
            continue;

	  dumx = dumy = dumz = 0.0;

	  j34 = dim * j;

	  xj = x[j34 + 0];
	  yj = x[j34 + 1];
	  zj = x[j34 + 2];

	  if (dim == 4) {
            dumw = 0.0;
            wj = x[j34 + 3];
	  }

	  /*
	   * Expand the excluded list into the iexw array by storing j
	   * at array address i.
	   */

	  for (i = 0; i < Jblo[j]; i++) {
            iexw[JexclAt[j][i] - 1] = j;
	  }

	  /* Select atom i from the pair list.  Non-graceful error handling. */

	  for (k = 0; k < npr; k++) {

            if (pearlist[j] == NULL) {
	      fprintf(nabout,
		      "NULL pair list entry in nbond loop 2, taskid = %d\n",
		      mytaskid);
	      fflush(nabout);
            }
            i = pearlist[j][k];

	    i34 = dim * i;

	    iaci = prm->Ntypes * (prm->Iac[i] - 1);
	    cgi = eelfaci * prm->Charges[i];

            /*
	     * Check whether this i,j pair is exempted
	     * by the excluded atom list.
             */

            if (iexw[i] != j) {
	      xij = x[i34 + 0] - xj;
	      yij = x[i34 + 1] - yj;
	      zij = x[i34 + 2] - zj;
	      r2 = xij * xij + yij * yij + zij * zij;

	      if (dim == 4) {
		wij = x[i34 + 3] - wj;
		r2 += wij * wij;
	      }

	      r2inv = 1.0 / r2;
	      r = sqrt(r2);
	      rinv = r * r2inv;

	      /* Calculate the derivatives according to dield. */

	      if (dield == -3) {

		/* special code Ramstein & Lavery dielectric, 94 force field */

		rs = SIG * r;
		rssq = rs * rs;
		pow = exp(-rs);
		eps1 = rssq + rs + rs + 2.0;
		epsi = 1.0 / (DIW - C1 * pow * eps1);
		cgijr = cgi * prm->Charges[j] * rinv * epsi;
		df2 = -cgijr * (1.0 + C1 * pow * rs * rssq * epsi);
		ic = prm->Cno[iaci + prm->Iac[j] - 1] - 1;
		if (ic >= 0) {
		  r6 = r2inv * r2inv * r2inv;
		  f2 = prm->Cn2[ic] * r6;
		  f1 = prm->Cn1[ic] * r6 * r6;
		  df = (df2 + (6.0 * f2 - 12.0 * f1) * enbfaci) * rinv;
		} else {
		  df = df2 * rinv;
		}

	      } else if (dield == -4) {

		/* distance-dependent dielectric code, 94 ff */
		/* epsilon = r  */

		rs = cgi * prm->Charges[j] * r2inv;
		df2 = -2.0 * rs;
		ic = prm->Cno[iaci + prm->Iac[j] - 1] - 1;
		if (ic >= 0) {
		  r6 = r2inv * r2inv * r2inv;
		  f2 = prm->Cn2[ic] * r6;
		  f1 = prm->Cn1[ic] * r6 * r6;
		  df = (df2 + (6.0 * f2 - 12.0 * f1) * enbfaci) * rinv;
		} else {
		  df = df2 * rinv;
		}

	      } else if (dield == -5) {

		/* non-bonded term from yammp  */

		dis = r;
		ic = prm->Cno[iaci + prm->Iac[j] - 1] - 1;
		d0 = prm->Cn2[ic];
		if (dis < d0) {
		  kij = prm->Cn1[ic];
		  diff = dis - d0;
		  df = 2.0 * kij * diff;
		} else {
		  df = 0.0;
		}
	      } else {

		/*
		 * Code for various dielectric models.
		 * The df2 variable should hold r(dV/dr).
		 */

		if (dield == 0) {

		  /* epsilon = r  */

		  rs = cgi * prm->Charges[j] * r2inv;
		  df2 = -2.0 * rs;

		} else if (dield == 1) {

		  /* epsilon = 1  */

		  rs = cgi * prm->Charges[j] * rinv;
		  df2 = -rs;

		} else if (dield == -2) {

		  /* Ramstein & Lavery dielectric, PNAS 85, 7231 (1988). */

		  rs = SIG * r;
		  rssq = rs * rs;
		  pow = exp(-rs);
		  eps1 = rssq + rs + rs + 2.0;
		  epsi = 1.0 / (DIW - C1 * pow * eps1);
		  cgijr = cgi * prm->Charges[j] * rinv * epsi;
		  df2 = -cgijr * (1.0 + C1 * pow * rs * rssq * epsi);
		}

		/* Calculate either Van der Waals or hydrogen bonded term. */

		ic = prm->Cno[iaci + prm->Iac[j] - 1];
		if (ic > 0 || enbfac != 1.0) {
		  if (ic > 0) {
		    ic--;
		  } else {
		    ibig = prm->Iac[i] > prm->Iac[j] ?
		      prm->Iac[i] : prm->Iac[j];
		    isml = prm->Iac[i] > prm->Iac[j] ?
		      prm->Iac[j] : prm->Iac[i];
		    ic = ibig * (ibig - 1) / 2 + isml - 1;
		  }
		  r6 = r2inv * r2inv * r2inv;
		  f2 = prm->Cn2[ic] * r6;
		  f1 = prm->Cn1[ic] * r6 * r6;
		  df = (df2 + (6.0 * f2 - 12.0 * f1) * enbfaci) * rinv;
#if 0
		  if (enbfac != 1.0)
		    nb14 += (f1 - f2) * enbfaci;
#endif
		} else {
		  ic = -ic - 1;
		  r10 = r2inv * r2inv * r2inv * r2inv * r2inv;
		  f2 = prm->HB10[ic] * r10;
		  f1 = prm->HB12[ic] * r10 * r2inv;
		  df = (df2 + (10.0 * f2 - 12.0 * f1) * enbfaci) * rinv;
#if 0
		  hbener += (f1 - f2) * enbfaci;
#endif
		}
	      }

	      /*
	       * The df term contains one more factor of Dij in the denominator
	       * so that terms such as dedx do not need to include 1/Dij. 
	       *
	       * Update the derivative accumulators for atom j.
	       */

	      df *= rinv;

	      dedx = df * xij;
	      dedy = df * yij;
	      dedz = df * zij;

	      dumx += dedx;
	      dumy += dedy;
	      dumz += dedz;

	      if (dim == 4) {
		dedw = df * wij;
		dumw += dedw;
	      }
            }
	  }

	  /* For atom j, the gradient is updated in the j-loop only. */

	  f[j34 + 0] -= dumx;
	  f[j34 + 1] -= dumy;
	  f[j34 + 2] -= dumz;

	  if (dim == 4) {
            f[j34 + 3] -= dumw;
	  }
	}
      }

#endif

      /* Deallocate the iexw array within this potentially parallel region. */

      free_ivector(iexw, -1, prm->Natom);
   }

   /* Return evdw and elec through by-reference calling parameters. */

   *enb = evdw;
   *eel = elec;

   return (0);
}

/***********************************************************************
                            EGB()
************************************************************************/

/*
 * Calculate the generalized Born energy and first derivatives.
 *
 * Calling parameters are as follows:
 *
 * lpears - number of pairs on the non-bonded lower triangle pair list
 * upears - number of pairs on the non-bonded upper trianble pair list
 * pearlist - non-bonded pair list, contiguous for upper & lower triangles
 * lpearsnp - number of pairs on the non-polar lower triangle pair list
 * upearsnp - number of pairs on the non-polar upper trianble pair list
 * pearlistnp - non-polar pair list, contiguous for upper & lower triangles
 * x - input: the atomic (x,y,z) coordinates
 * f - updated: the gradient vector
 * fs - input: overlap parameters
 * rborn - input: atomic radii
 * q - input: atomic charges
 * kappa - input: inverse of the Debye-Huckel length
 * diel_ext - input: solvent dielectric constant
 * enb - updated: Lennard-Jones energy
 * eelt - updated: gas-phase electrostatic energy
 * esurf - updated: nonpolar surface area solvation free energy
 * enp - updated: nonpolar van der Waals solvation free energy
 * freevectors - if !=0 free the static vectors and return
 */

static
REAL_T egb(INT_T * lpears, INT_T * upears, INT_T ** pearlist,
           INT_T * lpearsnp, INT_T * upearsnp, INT_T ** pearlistnp,
           REAL_T * x, REAL_T * f, REAL_T * fs, REAL_T * rborn, REAL_T * q,
           REAL_T * kappa, REAL_T * diel_ext, REAL_T * enb, REAL_T * eelt,
           REAL_T * esurf, REAL_T * enp, INT_T freevectors)
#define BOFFSET (0.09)
#define KSCALE (0.73)
{
#if defined(MPI) || defined(SCALAPACK)
   int ierror;
   static REAL_T *reductarr = NULL;
#endif

   static REAL_T *reff = NULL, *sumdeijda = NULL, *psi = NULL;
   static int *reqack = NULL, *iexw = NULL;

   char atsymb;
   int i, i34, j, j34, k, threadnum, numthreads, maxthreads, eoff, foff, soff;
   int npairs, ic, iaci, iteration, mask, consumer, producer, numcopies;
   size_t natom;
   REAL_T epol, dielfac, qi, qj, qiqj, fgbi, fgbk, rb2, expmkf;
   REAL_T elec, evdw, sumda, daix, daiy, daiz, daiw;
   REAL_T xi, yi, zi, wi, xj, yj, zj, wj, xij, yij, zij, wij;
   REAL_T dedx, dedy, dedz, dedw, de;
   REAL_T dij1i, dij3i, temp1;
   REAL_T qi2h, qid2h, datmp;
   REAL_T theta, ri1i, dij2i;

   REAL_T dij, sumi, t1, t2;
   REAL_T eel, f6, f12, rinv, r2inv, r6inv;
   REAL_T r2, ri, rj, sj, sj2, thi;
   REAL_T uij, efac, temp4, temp5, temp6;
   REAL_T dumbo, tmpsd;
   REAL_T rgbmax1i, rgbmax2i, rgbmaxpsmax2;

   /* LCPO stuff follows */
   int count, count2, icount;
   REAL_T si, sumAij, sumAjk, sumAijAjk, sumdAijddijdxi;
   REAL_T sumdAijddijdyi, sumdAijddijdzi, sumdAijddijdxiAjk;
   REAL_T sumdAijddijdyiAjk, sumdAijddijdziAjk, rij, tmpaij, Aij, dAijddij;
   REAL_T dAijddijdxj, dAijddijdyj, dAijddijdzj;
   REAL_T sumdAjkddjkdxj, sumdAjkddjkdyj, sumdAjkddjkdzj, p3p4Aij;
   REAL_T xk, yk, zk, rjk2, djk1i, rjk, vdw2dif, tmpajk, Ajk, sumAjk2,
       dAjkddjk;
   REAL_T dAjkddjkdxj, dAjkddjkdyj, dAjkddjkdzj, lastxj, lastyj, lastzj;
   REAL_T dAidxj, dAidyj, dAidzj, Ai, dAidxi, dAidyi, dAidzi;
   REAL_T totsasa;

   /* AGBNP stuff follows */
   int maxdepth, maxmaxdepth, fpair, sender, minusone = -1;
   int *setarray;
   REAL_T totalvolume, totvolume, surfacearea, surfarea, radius;
   REAL_T evdwnp, vdwdenom, vdwterm;

#if defined(MPI) || defined(SCALAPACK)
   MPI_Status status;
#endif

   /*FGB taylor coefficients follow */
   /* from A to H :                 */
   /* 1/3 , 2/5 , 3/7 , 4/9 , 5/11  */
   /* 4/3 , 12/5 , 24/7 , 40/9 , 60/11 */

#define TA 0.33333333333333333333
#define TB 0.4
#define TC 0.42857142857142857143
#define TD 0.44444444444444444444
#define TDD 0.45454545454545454545

#define TE 1.33333333333333333333
#define TF 2.4
#define TG 3.42857142857142857143
#define TH 4.44444444444444444444
#define THH 5.45454545454545454545

   /*
    * Determine the size of the iexw array.  If OPENMP is
    * defined, a copy of this array must be allocated for
    * each thread; otherwise, only one copy is allocated.
    */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
   maxthreads = omp_get_max_threads();
#else
   maxthreads = 1;
#endif

   /*
    * Determine the size of the sumdeijda array.  If OPENMP is
    * defined and NOREDUCE is not defined, a copy of this array
    * must be allocated for each thread; otherwise, only one copy
    * is allocated.
    */

#ifndef NOREDUCE
   numcopies = maxthreads;
#else
   numcopies = 1;
#endif

   natom = (size_t) prm->Natom;

   /*
    * If freevectors != 0, deallocate the static arrays that have been
    * previously allocated and return.
    */

   if (freevectors != 0) {
     if (reff != NULL) {
       free_vector(reff, 0, natom);
       reff = NULL;
     }
     if (iexw != NULL) {
       free_ivector(iexw, -1, maxthreads*(natom+1));
       iexw = NULL;
     }
     if (sumdeijda != NULL) {
       free_vector(sumdeijda, 0, numcopies*natom);
       sumdeijda = NULL;
     }
     if (psi != NULL) {
       free_vector(psi, 0, natom);
       psi = NULL;
     }
     if (reqack != NULL) {
       free_ivector(reqack, 0, maxthreads);
       reqack = NULL;
     }
#if defined(MPI) || defined(SCALAPACK)
     if (reductarr != NULL) {
       free_vector(reductarr, 0, natom);
       reductarr = NULL;
     }
#endif
      return (0.0);
   }

   /*
    * Smooth "cut-off" in calculating GB effective radii.
    * Implementd by Andreas Svrcek-Seiler and Alexey Onufriev.
    * The integration over solute is performed up to rgbmax and includes
    * parts of spheres; that is an atom is not just "in" or "out", as
    * with standard non-bonded cut.  As a result, calclated effective
    * radii are less than rgbmax. This saves time, and there is no
    * discontinuity in dReff/drij.
    *
    * Only the case rgbmax > 5*max(sij) = 5*fsmax ~ 9A is handled; this is
    * enforced in mdread().  Smaller values would not make much physical
    * sense anyway.
    *
    * Note: rgbmax must be less than or equal to cut so that the pairlist
    * generated from cut may be applied to calculation of the effective
    * radius and its derivatives.
    */

   if (rgbmax > cut) {
      fprintf(nabout,
              "Error in egb: rgbmax = %f is greater than cutoff = %f\n",
              rgbmax, cut);
      exit(1);
   }

   rgbmax1i = 1.0 / rgbmax;
   rgbmax2i = rgbmax1i * rgbmax1i;
   rgbmaxpsmax2 = (rgbmax + prm->Fsmax) * (rgbmax + prm->Fsmax);

   /* Allocate some static arrays if they have not been allocated already. */

   if (reff == NULL) {
     reff = vector(0, natom);
   }
   if (iexw == NULL) {
     iexw = ivector(-1, maxthreads*(natom+1));
   }
   if (sumdeijda == NULL) {
     sumdeijda = vector(0, numcopies*natom);
   }
   if ( (psi == NULL) && (gb==2 || gb==5) ) {
     psi = vector(0, natom);
   }
   if (reqack == NULL) {
     reqack = ivector(0, numcopies);
   }
#if defined(MPI) || defined(SCALAPACK)
   if (reductarr == NULL) {
     reductarr = vector(0, natom);
   }
#endif

   if (gb_debug)
      fprintf(nabout, "Effective Born radii:\n");

   /* 
    * Get the "effective" Born radii via the approximate pairwise method.
    * Use Eqs 9-11 of Hawkins, Cramer, Truhlar, J. Phys. Chem. 100:19824
    * (1996).
    *
    * For MPI or ScaLAPACK, initialize all elements of the reff array.
    * Although each task will calculate only a subset of the elements,
    * a reduction is used to combine the results from all tasks.
    * If a gather were used instead of a reduction, no initialization
    * would be necessary.
    */

#if defined(MPI) || defined(SCALAPACK)
   for (i = 0; i < prm->Natom; i++) {
      reff[i] = 0.0;
   }
#endif

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
#pragma omp parallel \
  private (i, xi, yi, zi, wi, ri, ri1i, sumi, j, k, xij, yij, zij, wij, \
           r2, dij1i, dij, sj, sj2, uij, dij2i, tmpsd, dumbo, theta, \
           threadnum, numthreads)
#endif
   {

      /*
       * Get the thread number and the number of threads for multi-threaded
       * execution under OpenMP.  For all other cases, including ScaLAPACK,
       * MPI and single-threaded execution, use the values that have been
       * stored in mytaskid and numtasks, respectively.
       */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
      threadnum = omp_get_thread_num();
      numthreads = omp_get_num_threads();
#else
      threadnum = mytaskid;
      numthreads = numtasks;
#endif

      /*
       * Loop over all atoms i.
       *
       * For MPI or ScaLAPACK, explicitly assign tasks to loop indices
       * for the following loop in a manner equivalent to (static, N)
       * scheduling for OpenMP.  For OpenMP use (dynamic, N) scheduling.
       *
       * The reff array is written in the following loops.  It is necessary to
       * synchronize the OpenMP threads or MPI tasks that execute these loops
       * following loop execution so that a race condition does not exist for
       * reading the reff array before it is written.  Even if all subsequent
       * loops use loop index to thread or task mapping that is identical to
       * that of the following loop, elements of the reff array are indexed by
       * other loop indices, so synchronization is necessary.
       *
       * OpenMP synchronization is accomplished by the implied barrier
       * at the end of this 'pragma omp for'.  MPI synchronization is
       * accomplished by MPI_Allreduce.
       */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
#pragma omp for schedule(dynamic, blocksize)
#endif
      for (i = 0; i < prm->Natom; i++) {

#if defined(MPI) || defined(SCALAPACK)
         if (!myroc(i, blocksize, numthreads, threadnum))
            continue;
#endif

         xi = x[dim * i];
         yi = x[dim * i + 1];
         zi = x[dim * i + 2];

         if (dim == 4) {
            wi = x[dim * i + 3];
         }

         ri = rborn[i] - BOFFSET;
         ri1i = 1. / ri;
         sumi = 0.0;

         /* Select atom j from the pair list.  Non-graceful error handling. */

         for (k = 0; k < lpears[i] + upears[i]; k++) {

            if (pearlist[i] == NULL) {
               fprintf(nabout,
                       "NULL pair list entry in egb loop 1, taskid = %d\n",
                       mytaskid);
               fflush(nabout);
            }
            j = pearlist[i][k];

            xij = xi - x[dim * j];
            yij = yi - x[dim * j + 1];
            zij = zi - x[dim * j + 2];
            r2 = xij * xij + yij * yij + zij * zij;

            if (dim == 4) {
               wij = wi - x[dim * j + 3];
               r2 += wij * wij;
            }

            if (r2 > rgbmaxpsmax2)
               continue;
            dij1i = 1.0 / sqrt(r2);
            dij = r2 * dij1i;
            sj = fs[j] * (rborn[j] - BOFFSET);
            sj2 = sj * sj;

            /*
             * ---following are from the Appendix of Schaefer and Froemmel,
             * JMB 216:1045-1066, 1990;  Taylor series expansion for d>>s
             * is by Andreas Svrcek-Seiler; smooth rgbmax idea is from
             * Andreas Svrcek-Seiler and Alexey Onufriev.
             */

            if (dij > rgbmax + sj)
               continue;

            if ((dij > rgbmax - sj)) {
               uij = 1. / (dij - sj);
               sumi -= 0.125 * dij1i * (1.0 + 2.0 * dij * uij +
                                        rgbmax2i * (r2 -
                                                    4.0 * rgbmax *
                                                    dij - sj2) +
                                        2.0 * log((dij - sj) * rgbmax1i));

            } else if (dij > 4.0 * sj) {
               dij2i = dij1i * dij1i;
               tmpsd = sj2 * dij2i;
               dumbo =
                   TA + tmpsd * (TB +
                                 tmpsd * (TC +
                                          tmpsd * (TD + tmpsd * TDD)));
               sumi -= sj * tmpsd * dij2i * dumbo;

            } else if (dij > ri + sj) {
               sumi -= 0.5 * (sj / (r2 - sj2) +
                              0.5 * dij1i * log((dij - sj) / (dij + sj)));

            } else if (dij > fabs(ri - sj)) {
               theta = 0.5 * ri1i * dij1i * (r2 + ri * ri - sj2);
               uij = 1. / (dij + sj);
               sumi -= 0.25 * (ri1i * (2. - theta) - uij +
                               dij1i * log(ri * uij));

            } else if (ri < sj) {
               sumi -= 0.5 * (sj / (r2 - sj2) + 2. * ri1i +
                              0.5 * dij1i * log((sj - dij) / (sj + dij)));

            }

         }

         if (gb == 1) {

            /* "standard" (HCT) effective radii:  */
            reff[i] = 1.0 / (ri1i + sumi);
            if (reff[i] < 0.0)
               reff[i] = 30.0;

         } else {

            /* "gbao" formulas:  */

            psi[i] = -ri * sumi;
            reff[i] = 1.0 / (ri1i - tanh((gbalpha - gbbeta * psi[i] +
                                          gbgamma * psi[i] * psi[i]) *
                                         psi[i]) / rborn[i]);
         }

         if (gb_debug)
            fprintf(nabout, "%d\t%15.7f\t%15.7f\n", i + 1, rborn[i],
                    reff[i]);
      }
   }

   /* The MPI synchronization is accomplished via reduction of the reff array. */

#if defined(MPI) || defined(SCALAPACK)

   t1 = seconds();

   ierror = MPI_Allreduce(reff, reductarr, prm->Natom,
                          MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
   if (ierror != MPI_SUCCESS) {
      fprintf(nabout,
              "Error in egb reff reduction, error = %d  mytaskid = %d\n",
              ierror, mytaskid);
   }
   for (i = 0; i < prm->Natom; i++) {
      reff[i] = reductarr[i];
   }

   /* Update the reduction time. */

   t2 = seconds();
   treduce += t2 - t1;
   t1 = t2;

#endif

   /* Do not compute non-polar contributions for this benchmark code. */

   *esurf = 0.0;

   /* Compute the GB, Coulomb and Lennard-Jones energies and derivatives. */

   epol = elec = evdw = evdwnp = 0.0;

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
#pragma omp parallel reduction (+: epol, elec, evdw, evdwnp) \
  private (i, i34, ri, qi, qj, expmkf, dielfac, qi2h, qid2h, iaci, \
           xi, yi, zi, wi, k, j, j34, xij, yij, zij, wij, r2, qiqj, \
           rj, rb2, efac, fgbi, fgbk, temp4, temp6, eel, de, temp5, \
           rinv, r2inv, ic, r6inv, f6, f12, dedx, dedy, dedz, dedw, \
           threadnum, numthreads, eoff, foff, soff, vdwdenom, vdwterm, \
           sumda, thi, ri1i, dij1i, datmp, daix, daiy, daiz, daiw, \
           dij2i, dij, sj, sj2, temp1, dij3i, tmpsd, dumbo, npairs, \
           iteration, mask, consumer, producer, xj, yj, zj, wj)
#endif
   {

      /*
       * Get the thread number and the number of threads for multi-threaded
       * execution under OpenMP.  For all other cases, including ScaLAPACK,
       * MPI and single-threaded execution, use the values that have been
       * stored in mytaskid and numtasks, respectively.
       */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
      threadnum = omp_get_thread_num();
      numthreads = omp_get_num_threads();
#else
      threadnum = mytaskid;
      numthreads = numtasks;
#endif

      /*
       * Compute offset into the iexw array for this thread, but only
       * if OPENMP is defined.  Otherwise, the offset is zero.
       */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
      eoff = (prm->Natom + 1) * threadnum;
#else
      eoff = 0;
#endif

      /*
       * Compute offsets into the gradient and sumdeijda arrays for this
       * thread, but only if OPENMP is defined and NOREDUCE is not defined.
       * Otherwise, the offsets are zero.
       */

#if (defined(SPEC_OMP) || defined(OPENMP)) && !defined(NOREDUCE)
      soff = prm->Natom * threadnum;
      foff = dim * soff;
#else
      soff = 0;
      foff = 0;
#endif

      /*
       * Initialize the sumdeijda array inside of the parallel region.
       *
       * For MPI and ScaLAPACK, each process has its own copy of the
       * array which must be initialized in its entirety because a
       * call to MPI_Allreduce will be used to reduce the array.
       * The MPI reduction will synchronize the processes.
       *
       * For OpenMP, when NOREDUCE is not defined each thread has its
       * own copy of the array as well.  But when NOREDUCE is not
       * defined there is only one copy of the array, and threads must
       * be synchronized to ensure that this copy is initialized
       * prior to use.
       */

/* SPEC, the logic of this is suspect */
#if !(defined(SPEC_OMP) || defined(OPENMP)) || !defined(NOREDUCE)
      for (i = 0; i < prm->Natom; i++) {
	sumdeijda[soff + i] = 0.0;
      }
#elif (defined(SPEC_OMP) || defined(OPENMP)) && defined(NOREDUCE)
      if (threadnum == 0) {
	for (i = 0; i < prm->Natom; i++) {
	  sumdeijda[soff + i] = 0.0;
	}
      }
#pragma omp barrier
#endif

      /*
       * Initialize the iexw array used for skipping excluded atoms.
       *
       * Note that because of the manner in which iexw is used, it
       * is necessary to initialize it before only the first iteration of
       * the following loop.
       */

      for (i = -1; i < prm->Natom; i++) {
	iexw[eoff + i] = -1;
      }

      /*
       * Loop over all atoms i.
       *
       * If OPENMP and NOREDUCE are defined, this (i,j) loop nest will
       * update sumdeijda[i] and f[i34 + 0..3] only.
       *
       * Synchronization of OpenMP threads will occur following this
       * loop nest because of the '#pragma omp for'.
       * 
       * For MPI or ScaLAPACK, explicitly assign tasks to loop indices
       * for the following loop in a manner equivalent to (static, N)
       * scheduling for OpenMP.  For OpenMP use (dynamic, N) scheduling.
       */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
#pragma omp for schedule(dynamic, blocksize)
#endif
      for (i = 0; i < prm->Natom; i++) {

#if defined(MPI) || defined(SCALAPACK)
         if (!myroc(i, blocksize, numthreads, threadnum))
            continue;
#endif

         ri = reff[i];
         qi = q[i];

         /*
          * If atom i is not frozen, compute the "diagonal" energy that
          * is a function of only the effective radius Ri but not of the
          * interatomic distance Dij.  Compute also the contribution of
          * the diagonal energy term to the sum by which the derivative
          * of Ri will be multiplied.  Do not calculate the van der Waals
          * component of the non-polar solvation free energy for this
          * benchmark version of the code.
          */

         if (!frozen[i]) {
            expmkf = exp(-KSCALE * (*kappa) * ri) / (*diel_ext);
            dielfac = 1.0 - expmkf;
            qi2h = 0.5 * qi * qi;
            qid2h = qi2h * dielfac;
            epol += -qid2h / ri;

	    vdwterm = 0.0;
	    vdwdenom = 1.0;

            sumdeijda[soff + i] +=
                qid2h - KSCALE * (*kappa) * qi2h * expmkf * ri + vdwterm;
         }

         /*
          * Skip the pair calculations if there are no atoms j on the
          * pair list of atom i.
          */

         npairs = upears[i];
         if (npairs <= 0)
            continue;

         i34 = dim * i;

         xi = x[i34];
         yi = x[i34 + 1];
         zi = x[i34 + 2];

         if (dim == 4) {
            wi = x[i34 + 3];
         }

         iaci = prm->Ntypes * (prm->Iac[i] - 1);

         /*
          * Expand the excluded atom list into the iexw array by storing i
          * at array address j.
          */

         for (j = 0; j < prm->Iblo[i]; j++) {
            iexw[eoff + IexclAt[i][j] - 1] = i;
         }

         /* Initialize the derivative accumulators. */

         daix = daiy = daiz = daiw = 0.0;

         /* Select atom j from the pair list.  Non-graceful error handling. */

         for (k = lpears[i]; k < lpears[i] + npairs; k++) {

            if (pearlist[i] == NULL) {
               fprintf(nabout,
                       "NULL pair list entry in egb loop 3, taskid = %d\n",
                       mytaskid);
               fflush(nabout);
            }
            j = pearlist[i][k];

            j34 = dim * j;

            /* Continue computing the non-diagonal energy term. */

            xij = xi - x[j34];
            yij = yi - x[j34 + 1];
            zij = zi - x[j34 + 2];
            r2 = xij * xij + yij * yij + zij * zij;

            if (dim == 4) {
               wij = wi - x[j34 + 3];
               r2 += wij * wij;
            }

            /*
             * Because index j is retrieved from the pairlist array it is
             * not constrained to a particular range of values; therefore,
             * the threads that have loaded the reff array must be
             * synchronized prior to the use of reff below.
             */

            qiqj = qi * q[j];
            rj = reff[j];
            rb2 = ri * rj;
            efac = exp(-r2 / (4.0 * rb2));
            fgbi = 1.0 / sqrt(r2 + rb2 * efac);
            fgbk = -(*kappa) * KSCALE / fgbi;

            expmkf = exp(fgbk) / (*diel_ext);
            dielfac = 1.0 - expmkf;

            epol += -qiqj * dielfac * fgbi;

            temp4 = fgbi * fgbi * fgbi;
            temp6 = qiqj * temp4 * (dielfac + fgbk * expmkf);
            de = temp6 * (1.0 - 0.25 * efac);

            temp5 = 0.5 * efac * temp6 * (rb2 + 0.25 * r2);

            /*
             * Compute the contribution of the non-diagonal energy term to the
             * sum by which the derivatives of Ri and Rj will be multiplied.
             */

            sumdeijda[soff + i] += ri * temp5;

#if !(defined(SPEC_OMP) || defined(OPENMP)) || !defined(NOREDUCE)
            sumdeijda[soff + j] += rj * temp5;
#endif

            /*
             * Compute the Van der Waals and Coulombic energies for only
             * those pairs that are not on the excluded atom list.  Any
             * pair on the excluded atom list will have atom i stored at
             * address j of the iexw array.  It is not necessary to reset
             * the elements of the iexw array to -1 between successive
             * iterations in i because an i,j pair is uniquely identified
             * by atom i stored at array address j.  Thus for example, the
             * i+1,j pair would be stored at the same address as the i,j
             * pair but after the i,j pair were used.
             *
             * The de term contains one more factor of Dij in the denominator
             * so that terms such as dedx do not need to include 1/Dij. 
             */

            if (iexw[eoff + j] != i) {

               rinv = 1. / sqrt(r2);
               r2inv = rinv * rinv;

               /*  gas-phase Coulomb energy:  */

               eel = qiqj * rinv;
               elec += eel;
               de -= eel * r2inv;

               /* Lennard-Jones energy:   */

               ic = prm->Cno[iaci + prm->Iac[j] - 1] - 1;
               if (ic >= 0) {
                  r6inv = r2inv * r2inv * r2inv;
                  f6 = prm->Cn2[ic] * r6inv;
                  f12 = prm->Cn1[ic] * r6inv * r6inv;
                  evdw += f12 - f6;
                  de -= (12. * f12 - 6. * f6) * r2inv;
               }
            }

            /*
             * Sum to the gradient vector the derivatives of Dij that are
             * computed relative to the cartesian coordinates of atoms i and j.
             */

            dedx = de * xij;
            dedy = de * yij;
            dedz = de * zij;

            daix += dedx;
            daiy += dedy;
            daiz += dedz;

            if (dim == 4) {
               dedw = de * wij;
               daiw += dedw;
	    }

	    /* Update the j elements of the gradient array. */

#if !(defined(SPEC_OMP) || defined(OPENMP)) || !defined(NOREDUCE)
            f[foff + j34] -= dedx;
            f[foff + j34 + 1] -= dedy;
            f[foff + j34 + 2] -= dedz;

            if (dim == 4) {
               f[foff + j34 + 3] -= dedw;
	    }
#endif
	 }

         /* Update the i elements of the gradient array. */

         f[foff + i34] += daix;
         f[foff + i34 + 1] += daiy;
         f[foff + i34 + 2] += daiz;

         if (dim == 4) {
            f[foff + i34 + 3] += daiw;
         }
      }

      /*
       * If OPENMP and NOREDUCE are defined, execute a (j,i) loop nest
       * to update sumdeijda[j] and f[j34 + 0..3].
       */

#if (defined(SPEC_OMP) || defined(OPENMP)) && defined(NOREDUCE)

      /*
       * Initialize the iexw array used for skipping excluded atoms.
       *
       * Note that because of the manner in which iexw is used, it
       * is necessary to initialize it before only the first iteration of
       * the following loop.
       */

      for (i = -1; i < prm->Natom; i++) {
	iexw[eoff + i] = -1;
      }

      /*
       * Loop over all atoms j.
       *
       * Because OPENMP and NOREDUCE are defined, this (j,i) loop nest will
       * update sumdeijda[j] and f[j34 + 0..3] only.
       *
       * For MPI or ScaLAPACK, explicitly assign tasks to loop indices
       * for the following loop in a manner equivalent to (static, N)
       * scheduling for OpenMP.  For OpenMP use (dynamic, N) scheduling.
       */

#pragma omp for schedule(dynamic, blocksize)
      for (j = 0; j < prm->Natom; j++) {

         /*
          * Skip the pair calculations if there are no atoms i on the
          * pair list of atom j.
          */

         npairs = lpears[j];
         if (npairs <= 0)
            continue;

         qj = q[j];
         rj = reff[j];

         j34 = dim * j;

         xj = x[j34];
         yj = x[j34 + 1];
         zj = x[j34 + 2];

         if (dim == 4) {
            wj = x[j34 + 3];
         }

         /*
          * Expand the excluded atom list into the iexw array by storing j
          * at array address i.
          */

         for (i = 0; i < Jblo[j]; i++) {
            iexw[eoff + JexclAt[j][i] - 1] = j;
         }

         /* Initialize the derivative accumulators. */

         daix = daiy = daiz = daiw = 0.0;

         /* Select atom i from the pair list.  Non-graceful error handling. */

         for (k = 0; k < npairs; k++) {

            if (pearlist[j] == NULL) {
               printf("NULL pair list entry in egb loop 4, taskid = %d\n",
                      mytaskid);
               fflush(nabout);
            }
            i = pearlist[j][k];

            i34 = dim * i;

            xij = x[i34] - xj;
            yij = x[i34 + 1] - yj;
            zij = x[i34 + 2] - zj;
            r2 = xij * xij + yij * yij + zij * zij;

            if (dim == 4) {
               wij = x[i34 + 3] - wj;
               r2 += wij * wij;
            }

            iaci = prm->Ntypes * (prm->Iac[i] - 1);

            /*
             * Because index i is retrieved from the pairlist array it is
             * not constrained to a particular range of values; therefore,
             * the threads that have loaded the reff array must be
             * synchronized prior to the use of reff below.
             */

            qiqj = q[i] * qj;
            ri = reff[i];
            rb2 = ri * rj;
            efac = exp(-r2 / (4.0 * rb2));
            fgbi = 1.0 / sqrt(r2 + rb2 * efac);
            fgbk = -(*kappa) * KSCALE / fgbi;

            expmkf = exp(fgbk) / (*diel_ext);
            dielfac = 1.0 - expmkf;

            temp4 = fgbi * fgbi * fgbi;
            temp6 = qiqj * temp4 * (dielfac + fgbk * expmkf);
            de = temp6 * (1.0 - 0.25 * efac);

            temp5 = 0.5 * efac * temp6 * (rb2 + 0.25 * r2);

            /*
             * Compute the contribution of the non-diagonal energy term to the
             * sum by which the derivatives of Ri and Rj will be multiplied.
             */

            sumdeijda[j] += rj * temp5;

            /*
             * Compute the Van der Waals and Coulombic energies for only
             * those pairs that are not on the excluded atom list.  Any
             * pair on the excluded atom list will have atom j stored at
             * address i of the iexw array.  It is not necessary to reset
             * the elements of the iexw array to -1 between successive
             * iterations in j because an i,j pair is uniquely identified
             * by atom j stored at array address i.  Thus for example, the
             * i,j+1 pair would be stored at the same address as the i,j
             * pair but after the i,j pair were used.
             *
             * The de term contains one more factor of Dij in the denominator
             * so that terms such as dedx do not need to include 1/Dij. 
             */

            if (iexw[eoff + i] != j) {

               rinv = 1. / sqrt(r2);
               r2inv = rinv * rinv;

               /*  gas-phase Coulomb energy:  */

               eel = qiqj * rinv;
               de -= eel * r2inv;

               /* Lennard-Jones energy:   */

               ic = prm->Cno[iaci + prm->Iac[j] - 1] - 1;
               if (ic >= 0) {
                  r6inv = r2inv * r2inv * r2inv;
                  f6 = prm->Cn2[ic] * r6inv;
                  f12 = prm->Cn1[ic] * r6inv * r6inv;
                  de -= (12. * f12 - 6. * f6) * r2inv;
               }
            }

            /*
             * Sum to the gradient vector the derivatives of Dij that are
             * computed relative to the cartesian coordinates of atoms i and j.
             */

            dedx = de * xij;
            dedy = de * yij;
            dedz = de * zij;

            daix += dedx;
            daiy += dedy;
            daiz += dedz;

            if (dim == 4) {
               dedw = de * wij;
               daiw += dedw;
            }
         }

         /* Update the j elements of the gradient array. */

         f[j34] -= daix;
         f[j34 + 1] -= daiy;
         f[j34 + 2] -= daiz;

         if (dim == 4) {
            f[j34 + 3] -= daiw;
         }
      }

#endif

   }

   /*
    * If OPENMP is defined and NOREDUCE is not defined, perform
    * a reduction over sumdeijda either logarithmically or not.
    *
    * Note: for very large numbers of threads, the cost of reduction
    * may exceed the cost of separate (i, j) and (j, i) loop nests
    * that are used in this egb function and in the egb2 function.
    */

#if (defined(SPEC_OMP) || defined(OPENMP)) && !defined(NOREDUCE)

   t1 = seconds();

#undef LOGARITHMIC_REDUCTION

#ifdef LOGARITHMIC_REDUCTION

   /*
    * Here is the logarithmic reduction for OpenMP.
    * Initialize the reqack array.
    */

   for (i = 0; i < numcopies; i++) {
     reqack[i] = 0;
   }

#pragma omp parallel \
  private (i, iteration, mask, consumer, producer, threadnum)
   {

     /*
      * If EGB_OMP_FLUSH is not defined, synchronize the threads
      * via '#pragma omp barrier' which can be costly due to the
      * need to synchronize all of the threads.
      *
      * If EGB_OMP_FLUSH is defined, four-cycle signaling will be
      * used to synchronize the threads, as will be seen below,
      * but here we need a '#pragma omp flush' so that the request
      * and acknowledge flags are read correctly by all of the threads.
      */

#define EGB_OMP_FLUSH

#ifndef EGB_OMP_FLUSH
#pragma omp barrier
#else
#pragma omp flush
#endif

     /*
      * Calculate the iterations for the log2 reduction of the grad array.
      * Note that each OpenMP thread determines 'consumer' and 'producer'
      * from its thread number.
      */

     threadnum = omp_get_thread_num();
     iteration = maxthreads - 1;
     mask = 1;
     while (iteration > 0) {
       consumer = threadnum & (~mask);
       producer = consumer | ((mask + 1) >> 1);

       /*
	* 'Consumer' designates a thread to which to add data from a
	* 'producer' thread.  Perform reduction only when both consumer
	* and producer are less than maxthreads.
	*
	* For successive iterations of the loop mask will have the values
	* 1, 3, 7, 15../
	*
	* The for the example of maxThreads=14 (numThreads=0..13), the
	* following threads will be chosen by consumer and producer:
	*
	* (iteration 1, consumer) - 0, 2, 4, 6, 8, 10, 12
	* (iteration 1, producer) - 1, 3, 5, 7, 9, 11, 13
	*
	* (iteration 2, consumer) - 0, 4, 8,  12
	* (iteration 2, producer) - 2, 6, 10, 13
	*
	* (iteration 3, consumer) - 0, 8
	* (iteration 3, producer) - 4, 12
	*
	* (iteration 4, consumer) - 0
	* (iteration 4, producer) - 8
	*
	* As the example shows, the final result is found in the
	* sumdeijda array for thread 0.
	*
	* Note that the following if statement uses maxthreads to
	* determine whether to perform a reduction step.  Maxthreads
	* may not be used to determine whether to execute the while loop
	* (above) because all threads must execute the while loop in order
	* that the '#pragma omp barrier' within the loop not hang due to
	* threads that are not executing the loop.  The test for
	* (threadNum == consumer) guarantees that this thread will accept
	* data from a producer thread which has (threadNum == producer).
	* The test for (producer < maxthreads) guarantees that the producer
	* thread exists.
	*/

       if ( ( threadnum == consumer ) && ( producer < maxthreads ) ) {

	 /*
	  * If EGB_OMP_FLUSH is defined, four-cycle signaling is used to
	  * synchronize the threads.  The 'consumer' thread raises its
	  * request flag then waits for the 'producer' thread to raise
	  * its acknowledge flag.
	  *
	  * The request flag is set by assigning the value of 'iteration'
	  * to reqack[consumer].  The acknowledge flag is set by assigning
	  * the value of iteration to reqack[producer].  This value
	  * is used instead of 1 in order that each iteration of the loop
	  * have a unique value for the request and acknowledge flags.
	  * This approach avoids a race condition across iterations of
	  * the loop for access to the request and acknowledge flags.
	  *
	  * If EGB_OMP_FLUSH is not defined then no test is necessary
	  * because '#pragma omp barrier' is used to resynchronize
	  * all threads following each iteration of the while loop.
	  */

#ifdef EGB_OMP_FLUSH
	 reqack[consumer] = iteration;
#pragma omp flush
	 do {
#pragma omp flush
	 } while (reqack[producer] != iteration);
#endif

	 /*
	  * The producer and consumer threads are synchronized,
	  * so add the grad array from the producer to the
	  * grad array from the consumer.
	  */

	 for (i = 0; i < prm->Natom; i++) {
	   sumdeijda[consumer * prm->Natom + i] +=
	     sumdeijda[producer * prm->Natom + i];
	 }
	
	 /*
	  * If EGB_OMP_FLUSH is defined, four-cycle signaling is used to
	  * synchronize the threads.  The 'consumer' thread lowers its
	  * request flag then waits for the 'producer' thread to lower
	  * its acknowledge flag.
	  */

#ifdef EGB_OMP_FLUSH
	 reqack[consumer] = 0;
#pragma omp flush
	 do {
#pragma omp flush
	 } while (reqack[producer] == iteration);
#endif

       }

       /*
	* Here is the if statement that controls whether an OpenMP thread
	* is a producer.  Note that because consumer never equals producer,
	* a thread cannot be both the consumer and producer during a given
	* iteration of the while loop.
	*
	* Because the grad array contents are copied by the consumer thread,
	* the producer thread needs only to synchronize via four-cycle
	* signaling.
	*
	* It is necessary not only to check that (threadnum == producer)
	* but also that (threadnum < maxthreads) to ensure that the
	* producer thread exists.
	*/

       if ( ( threadnum == producer ) && ( threadnum < maxthreads) ) {

	 /*
	  * If EGB_OMP_FLUSH is defined, four-cycle signaling is used to
	  * synchronize the threads.  The 'producer' thread waits for
	  * the 'consumer' thread to raise its request flag, then raises
	  * its acknowledge flag, then waits for the 'consumer' thread
	  * to lower its request flag (indicating that the 'consumer'
	  * thread has read the data), then lowers its acknowledge flag.
	  */

#ifdef EGB_OMP_FLUSH
	 do {
#pragma omp flush
	 } while (reqack[consumer] != iteration);
	 reqack[producer] = iteration;
#pragma omp flush

	 do {
#pragma omp flush
	 } while (reqack[consumer] == iteration);
	 reqack[producer] = 0;
#pragma omp flush
#endif

       }

       /*
	* If EGB_OMP_FLUSH is not defined resynchronize via
	* '#pragma omp barrier'.
	*/

#ifndef EGB_OMP_FLUSH
#pragma omp barrier
#endif

       /* Prepare for the next iteration of the while loop. */

       mask = (mask << 1) + 1;
       iteration >>= 1;
     }
   }

#else

   /*
    * Here is the non-logarithmic reduction of the sumdeijda array
    * under OpenMP.  Add to the sumdeijda array for thread 0 all
    * of the sumdeijda arrays for non-zero threads.  The (j, i)
    * loop nest is more efficient than an (i, j) loop nest.
    *
    * Note: the following 'if' should not be needed, but works around a
    * bug in ifort 9.1 on ia64.
    */

   if (maxthreads > 1) {
#pragma omp parallel for private(i, j) schedule(dynamic, blocksize)
     for (j = 0; j < prm->Natom; j++) {
       for (i = 1; i < numcopies; i++) {
	 sumdeijda[j] += sumdeijda[prm->Natom * i + j];
       }
     }
   }

   /* Update the reduction time. */

   t2 = seconds();
   treduce += t2 - t1;
   t1 = t2;

#endif

   /* Perform a reduction of sumdeijda if MPI or SCALAPACK is defined. */

#elif defined(MPI) || defined(SCALAPACK)

   t1 = seconds();

   ierror = MPI_Allreduce(sumdeijda, reductarr, prm->Natom,
                          MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
   if (ierror != MPI_SUCCESS) {
      fprintf(nabout,
              "Error in egb sumdeijda reduction, error = %d  mytaskid = %d\n",
              ierror, mytaskid);
   }
   for (i = 0; i < prm->Natom; i++) {
      sumdeijda[i] = reductarr[i];
   }

   /* Update the reduction time. */

   t2 = seconds();
   treduce += t2 - t1;
   t1 = t2;

#endif

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
#pragma omp parallel \
  private (i, i34, ri, qi, expmkf, dielfac, qi2h, qid2h, iaci, \
           xi, yi, zi, wi, k, j, j34, xij, yij, zij, wij, r2, qiqj, \
           rj, rb2, efac, fgbi, fgbk, temp4, temp6, eel, de, temp5, \
           rinv, r2inv, ic, r6inv, f6, f12, dedx, dedy, dedz, dedw, \
           threadnum, numthreads, foff, xj, yj, zj, wj, \
           sumda, thi, ri1i, dij1i, datmp, daix, daiy, daiz, daiw, \
           dij2i, dij, sj, sj2, temp1, dij3i, tmpsd, dumbo, npairs)
#endif
   {
      /*
       * Get the thread number and the number of threads for multi-threaded
       * execution under OpenMP.  For all other cases, including ScaLAPACK,
       * MPI and single-threaded execution, use the values that have been
       * stored in mytaskid and numtasks, respectively.
       */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
      threadnum = omp_get_thread_num();
      numthreads = omp_get_num_threads();
#else
      threadnum = mytaskid;
      numthreads = numtasks;
#endif

      /*
       * Compute an offset into the gradient array for this thread,
       * but only if OPENMP is defined and NOREDUCE is not defined.
       * Even if NOREDUCE is defined, there is no need to compute
       * an offset into the sumdeijda array because all copies of
       * this array have been reduced into copy zero.
       */

#if (defined(SPEC_OMP) || defined(OPENMP)) && !defined(NOREDUCE)
      foff = prm->Natom * dim * threadnum;
#else
      foff = 0;
#endif

      /*
       * Compute the derivatives of the effective radius Ri of atom i
       * with respect to the cartesian coordinates of each atom j.  Sum
       * all of these derivatives into the gradient vector.
       *
       * Loop over all atoms i.
       *
       * If OPENMP and NOREDUCE are defined, this (i,j) loop nest will
       * update f[i34 + 0..3] only.
       *
       * Synchronization of OpenMP threads will occur following this
       * loop nest because of the '#pragma omp for'.
       * 
       * A reduction of the gradient array will occur in the mme34 function,
       * either for OpenMP or MPI.  This reduction will synchronize the MPI
       * tasks, so an explicit barrier is not necessary following this loop.
       *
       * For MPI or ScaLAPACK, explicitly assign tasks to loop indices
       * for the following loop in a manner equivalent to (static, N)
       * scheduling for OpenMP.  For OpenMP use (dynamic, N) scheduling.
       */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
#pragma omp for schedule(dynamic, blocksize)
#endif
      for (i = 0; i < prm->Natom; i++) {

#if defined(MPI) || defined(SCALAPACK)
         if (!myroc(i, blocksize, numthreads, threadnum))
            continue;
#endif

         /*
          * Don't calculate derivatives of the effective radius of atom i
          * if atom i is frozen or if there are no pair atoms j associated
          * with atom i.
          */

         npairs = lpears[i] + upears[i];
         if ( frozen[i] || (npairs <= 0) )
            continue;

         i34 = dim * i;

         xi = x[i34];
         yi = x[i34 + 1];
         zi = x[i34 + 2];

         if (dim == 4) {
            wi = x[i34 + 3];
         }

         ri = rborn[i] - BOFFSET;
         ri1i = 1. / ri;

         sumda = sumdeijda[i];

         if (gb > 1) {

            ri = rborn[i] - BOFFSET;
            thi =
                tanh((gbalpha - gbbeta * psi[i] +
                      gbgamma * psi[i] * psi[i]) * psi[i]);
            sumda *=
                (gbalpha - 2.0 * gbbeta * psi[i] +
                 3.0 * gbgamma * psi[i] * psi[i])
                * (1.0 - thi * thi) * ri / rborn[i];
         }

         /* Initialize the derivative accumulators. */

         daix = daiy = daiz = daiw = 0.0;

         /* Select atom j from the pair list.  Non-graceful error handling. */

         for (k = 0; k < npairs; k++) {

            if (pearlist[i] == NULL) {
               fprintf(nabout,
                       "NULL pair list entry in egb loop 5, taskid = %d\n",
                       mytaskid);
               fflush(nabout);
            }
            j = pearlist[i][k];

            j34 = dim * j;

            xij = xi - x[j34];
            yij = yi - x[j34 + 1];
            zij = zi - x[j34 + 2];
            r2 = xij * xij + yij * yij + zij * zij;

            if (dim == 4) {
               wij = wi - x[j34 + 3];
               r2 += wij * wij;
            }

            /* Ignore the ij atom pair if their separation exceeds the GB cutoff. */

            if (r2 > rgbmaxpsmax2)
               continue;

            dij1i = 1.0 / sqrt(r2);
            dij2i = dij1i * dij1i;
            dij = r2 * dij1i;

            sj = fs[j] * (rborn[j] - BOFFSET);
            sj2 = sj * sj;

            /*
             * The following are the numerator of the first derivatives of the
             * effective radius Ri with respect to the interatomic distance Dij.
             * They are derived from the equations from the Appendix of Schaefer
             * and Froemmel as well as from the Taylor series expansion for d>>s
             * by Andreas Svrcek-Seiler.  The smooth rgbmax idea is from Andreas
             * Svrcek-Seiler and Alexey Onufriev.  The complete derivative is
             * formed by multiplying the numerator by -Ri*Ri.  The factor of Ri*Ri
             * has been moved to the terms that are multiplied by the derivative.
             * The negation is deferred until later.  When the chain rule is used
             * to form the first derivatives of the effective radius with respect
             * to the cartesian coordinates, an additional factor of Dij appears
             * in the denominator.  That factor is included in the following
             * expressions.
             */

            if (dij > rgbmax + sj)
               continue;

            if (dij > rgbmax - sj) {

               temp1 = 1. / (dij - sj);
               dij3i = dij1i * dij2i;
               datmp = 0.125 * dij3i * ((r2 + sj2) *
                                        (temp1 * temp1 - rgbmax2i) -
                                        2.0 * log(rgbmax * temp1));

            } else if (dij > 4.0 * sj) {

               tmpsd = sj2 * dij2i;
               dumbo =
                   TE + tmpsd * (TF +
                                 tmpsd * (TG +
                                          tmpsd * (TH + tmpsd * THH)));
               datmp = tmpsd * sj * dij2i * dij2i * dumbo;

            } else if (dij > ri + sj) {

               temp1 = 1. / (r2 - sj2);
               datmp = temp1 * sj * (-0.5 * dij2i + temp1)
                   + 0.25 * dij1i * dij2i * log((dij - sj) / (dij + sj));

            } else if (dij > fabs(ri - sj)) {

               temp1 = 1. / (dij + sj);
               dij3i = dij2i * dij1i;
               datmp =
                   -0.25 * (-0.5 * (r2 - ri * ri + sj2) * dij3i
                            * ri1i * ri1i + dij1i * temp1 * (temp1 - dij1i)
                            - dij3i * log(ri * temp1));

            } else if (ri < sj) {

               temp1 = 1. / (r2 - sj2);
               datmp =
                   -0.5 * (sj * dij2i * temp1 - 2. * sj * temp1 * temp1 -
                           0.5 * dij2i * dij1i * log((sj - dij) /
                                                     (sj + dij)));

            } else {
               datmp = 0.;
            }

            /* Sum the derivatives into daix, daiy, daiz and daiw. */

            daix += xij * datmp;
            daiy += yij * datmp;
            daiz += zij * datmp;

            if (dim == 4) {
               daiw += wij * datmp;
            }

            /*
             * Sum the derivatives relative to atom j (weighted by -sumdeijda[i])
             * into the gradient vector.  For example, f[j34 + 2] contains the
             * derivatives of Ri with respect to the z-coordinate of atom j.
             */

#if !(defined(SPEC_OMP) || defined(OPENMP)) || !defined(NOREDUCE)
            datmp *= sumda;
            f[foff + j34] += xij * datmp;
            f[foff + j34 + 1] += yij * datmp;
            f[foff + j34 + 2] += zij * datmp;

            if (dim == 4) {
               f[foff + j34 + 3] += wij * datmp;
            }
#endif

         }

         /*
          * Update the gradient vector with the sums of derivatives of the
          * effective radius Ri with respect to the cartesian coordinates.
          * For example, f[i34 + 1] contains the sum of derivatives of Ri
          * with respect to the y-coordinate of each atom.  Multiply by
          * -sumdeijda[i] here (instead of merely using datmp multiplied by
          * -sumdeijda) in order to distribute the product across the sum of
          * derivatives in an attempt to obtain greater numeric stability.
          */

         f[foff + i34] -= sumda * daix;
         f[foff + i34 + 1] -= sumda * daiy;
         f[foff + i34 + 2] -= sumda * daiz;

         if (dim == 4) {
            f[foff + i34 + 3] -= sumda * daiw;
         }
      }

#if (defined(SPEC_OMP) || defined(OPENMP)) && defined(NOREDUCE)

      /*
       * Compute the derivatives of the effective radius Ri of atom i
       * with respect to the cartesian coordinates of each atom j.  Sum
       * all of these derivatives into the gradient vector.
       *
       * Loop over all atoms j.
       *
       * Because OPENMP and NOREDUCE are defined, this (j,i) loop nest will
       * update f[j34 + 0..3] only.
       *
       * Synchronization of OpenMP threads will occur following this loop
       * because of the '#pragma omp for'.  No reduction of the gradient
       * array is necessary because it will occur in the mme34 function.
       *
       * For MPI or ScaLAPACK, explicitly assign tasks to loop indices
       * for the following loop in a manner equivalent to (static, N)
       * scheduling for OpenMP.  For OpenMP use (dynamic, N) scheduling.
       */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
#pragma omp for schedule(dynamic, blocksize)
#endif
      for (j = 0; j < prm->Natom; j++) {

         /*
          * Don't calculate derivatives of the effective radius of atom i
          * if there are no pair atoms j associated with atom i.
          */

         npairs = lpears[j] + upears[j];
         if (npairs <= 0)
            continue;

         j34 = dim * j;

         xj = x[j34];
         yj = x[j34 + 1];
         zj = x[j34 + 2];

         if (dim == 4) {
            wj = x[j34 + 3];
         }

         sj = fs[j] * (rborn[j] - BOFFSET);
         sj2 = sj * sj;

         /* Initialize the derivative accumulators. */

         daix = daiy = daiz = daiw = 0.0;

         /* Select atom i from the pair list.  Non-graceful error handling. */

         for (k = 0; k < npairs; k++) {

            if (pearlist[j] == NULL) {
               printf("NULL pair list entry in egb loop 6, taskid = %d\n",
                      mytaskid);
               fflush(stdout);
            }
            i = pearlist[j][k];

            /*
             * Don't calculate derivatives of the effective radius of atom i
             * if atom i is frozen.
             */

            if (frozen[i])
               continue;

            i34 = dim * i;

            xij = x[i34] - xj;
            yij = x[i34 + 1] - yj;
            zij = x[i34 + 2] - zj;
            r2 = xij * xij + yij * yij + zij * zij;

	    if (dim == 4) {
	      wij = x[i34 + 3] - wj;
	      r2 += wij * wij;
	    }

            /* Ignore the ij atom pair if their separation exceeds the GB cutoff. */

            if (r2 > rgbmaxpsmax2)
               continue;

            dij1i = 1.0 / sqrt(r2);
            dij2i = dij1i * dij1i;
            dij = r2 * dij1i;

            ri = rborn[i] - BOFFSET;
            ri1i = 1. / ri;

            /*
             * The following are the numerator of the first derivatives of the
             * effective radius Ri with respect to the interatomic distance Dij.
             * They are derived from the equations from the Appendix of Schaefer
             * and Froemmel as well as from the Taylor series expansion for d>>s
             * by Andreas Svrcek-Seiler.  The smooth rgbmax idea is from Andreas
             * Svrcek-Seiler and Alexey Onufriev.  The complete derivative is
             * formed by multiplying the numerator by -Ri*Ri.  The factor of Ri*Ri
             * has been moved to the terms that are multiplied by the derivative.
             * The negation is deferred until later.  When the chain rule is used
             * to form the first derivatives of the effective radius with respect
             * to the cartesian coordinates, an additional factor of Dij appears
             * in the denominator.  That factor is included in the following
             * expressions.
             */

            if (dij > rgbmax + sj)
               continue;

            if (dij > rgbmax - sj) {

               temp1 = 1. / (dij - sj);
               dij3i = dij1i * dij2i;
               datmp = 0.125 * dij3i * ((r2 + sj2) *
                                        (temp1 * temp1 - rgbmax2i) -
                                        2.0 * log(rgbmax * temp1));

            } else if (dij > 4.0 * sj) {

               tmpsd = sj2 * dij2i;
               dumbo =
                   TE + tmpsd * (TF +
                                 tmpsd * (TG +
                                          tmpsd * (TH + tmpsd * THH)));
               datmp = tmpsd * sj * dij2i * dij2i * dumbo;

            } else if (dij > ri + sj) {

               temp1 = 1. / (r2 - sj2);
               datmp = temp1 * sj * (-0.5 * dij2i + temp1)
                   + 0.25 * dij1i * dij2i * log((dij - sj) / (dij + sj));

            } else if (dij > fabs(ri - sj)) {

               temp1 = 1. / (dij + sj);
               dij3i = dij2i * dij1i;
               datmp =
                   -0.25 * (-0.5 * (r2 - ri * ri + sj2) * dij3i
                            * ri1i * ri1i + dij1i * temp1 * (temp1 - dij1i)
                            - dij3i * log(ri * temp1));

            } else if (ri < sj) {

               temp1 = 1. / (r2 - sj2);
               datmp =
                   -0.5 * (sj * dij2i * temp1 - 2. * sj * temp1 * temp1 -
                           0.5 * dij2i * dij1i * log((sj - dij) /
                                                     (sj + dij)));

            } else {
               datmp = 0.;
            }

            /*
             * Because index i is retrieved from the pairlist array it is
             * not constrained to a particular range of values; therefore,
             * the threads that have loaded the sumdeijda array have been
             * synchronized above prior to the use of sumdeijda below.
             */

            sumda = sumdeijda[i];

	    if (gb > 1) {

	      ri = rborn[i] - BOFFSET;
	      thi =
                tanh((gbalpha - gbbeta * psi[i] +
                      gbgamma * psi[i] * psi[i]) * psi[i]);
	      sumda *=
                (gbalpha - 2.0 * gbbeta * psi[i] +
                 3.0 * gbgamma * psi[i] * psi[i])
                * (1.0 - thi * thi) * ri / rborn[i];
	    }

            /* Sum the derivatives into daix, daiy, daiz and daiw. */

            datmp *= sumda;
            daix += xij * datmp;
            daiy += yij * datmp;
            daiz += zij * datmp;

            if (dim == 4) {
               daiw += wij * datmp;
	    }
         }

         /*
          * Update the gradient vector with the sums of derivatives of the
          * effective radius Ri with respect to the cartesian coordinates.
          * For example, f[j34 + 1] contains the sum of derivatives of Ri
          * with respect to the y-coordinate of each atom.
          */

         f[j34] += daix;
         f[j34 + 1] += daiy;
         f[j34 + 2] += daiz;

         if (dim == 4) {
            f[j34 + 3] += daiw;
         }
      }

#endif

   }

   /* Free the static arrays if static_arrays is 0. */

   if (!static_arrays) {
     if (reff != NULL) {
       free_vector(reff, 0, natom);
       reff = NULL;
     }
     if (iexw != NULL) {
       free_ivector(iexw, -1, maxthreads*(natom+1));
       iexw = NULL;
     }
     if (sumdeijda != NULL) {
       free_vector(sumdeijda, 0, numcopies*natom);
       sumdeijda = NULL;
     }
     if (psi != NULL) {
       free_vector(psi, 0, natom);
       psi = NULL;
     }
     if (reqack != NULL) {
       free_ivector(reqack, 0, numcopies);
       reqack = NULL;
     }
#if defined(MPI) || defined(SCALAPACK)
     if (reductarr != NULL) {
       free_vector(reductarr, 0, natom);
       reductarr = NULL;
     }
#endif
   }

   /*
    * Return elec, evdw and evdwnp through the parameters eelt, enb and enp.
    * These variables are computed in parallel.
    */

   *eelt = elec;
   *enb = evdw;
   *enp = evdwnp;

   return (epol);
}

/***********************************************************************
                            MME34()
************************************************************************/

/*
 * Here is the mme function for 3D or 4D, depending upon the dim variable.
 *
 * Calling parameters are as follows:
 *
 * x - input: the atomic (x,y,z) coordinates
 * f - updated: the gradient vector
 * iter - the iteration counter, which if negative selects the following:
 *        -1 print some energy values
 *        -3 call egb to deallocate static arrays, then deallocate grad
 *        -(any other value) normal execution
 */

static
REAL_T mme34(REAL_T * x, REAL_T * f, int *iter)
{
   extern REAL_T tconjgrad;

   REAL_T ebh, eba, eth, eta, eph, epa, enb, eel, enb14, eel14, ecn;
   REAL_T e_gb, esurf, evdwnp, frms;
   REAL_T ene[20];
   REAL_T t1, t2;
   int i, j, k, goff, threadnum, numthreads, maxthreads;
   int iteration, mask, consumer, producer, numcopies;
   int dummy = 0;
   size_t n;

   static REAL_T *grad = NULL;
   static int *reqack = NULL;

#if defined(MPI) || defined(SCALAPACK)
   int ierror;
   REAL_T reductarr[20];
#endif

   t1 = seconds();
   n = (size_t) prm->Natom;

   /*
    * If OPENMP is defined, set maxthreads to the maximum number of
    * OpenMP threads then allocate the reqack array. Otherwise,
    * set maxthreads to 1.  If NOREDUCE is not defined, set
    * numcopies to maxthreads; otherwise, set it to 1.
    */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
   maxthreads = omp_get_max_threads();
   if (reqack == NULL) {
     reqack = ivector(0, maxthreads);
   }
#else
   maxthreads = 1;
#endif

#ifndef NOREDUCE
   numcopies = maxthreads;
#else
   numcopies = 1;
#endif

   /*
    * If the iteration count equals -3, call egb to deallocate the
    * static arrays, deallocate the gradient array, then return;
    * otherwise, simply return.
    */

   if (*iter == -3) {
      egb(lpairs, upairs, pairlist, lpairs, upairs, pairlist,
          x, grad, prm->Fs, prm->Rborn, prm->Charges,
          &kappa, &epsext, &enb, &eel, &esurf, &evdwnp, 1);
      if (grad != NULL) {
	free_vector(grad, 0, numcopies * dim * n);
	grad = NULL;
      }
      if (reqack != NULL) {
	free_ivector(reqack, 0, maxthreads);
	reqack = NULL;
      }
      return (0.0);
   }

   /* If the iteration count equals 0, print the header for task 0 only. */

   if (*iter == 0 && mytaskid == 0) {
      fprintf(nabout, "      iter    Total       bad      vdW     elect"
              "   nonpolar   genBorn      frms\n");
      fflush(nabout);
   }

   /* If the iteration count equals 0, initialize the timing variables. */

   if (*iter == 0) {
      tnonb = tpair = tbond = tangl = tphi = tborn = tcons = tmme = 0.0;
      tconjgrad = tmd = treduce = 0.0;
   }

   /*
    * Write the checkpoint file every nchk iterations if the chknm
    * variable is non-NULL.
    */

   if (chknm != NULL && (*iter > 0 && *iter % nchk == 0)) {
      checkpoint(chknm, prm->Natom, x, *iter);
   }

   /*
    * Build the non-bonded pair list if it hasn't already been built;
    * rebuild it every nsnb iterations.  The non-bonded pair list
    * uses blocksize to group OpenMP thread to loop index, or MPI task
    * to loop index, mapping in the nblist and egb functions.  It is
    * global and fully populated for OpenMP, and local and partially
    * populated for MPI and SCALAPACK.
    */

   if (nb_pairs < 0 || (*iter > 0 && *iter % nsnb == 0)) {
      nb_pairs = nblist(lpairs, upairs, pairlist, x, dummy, 1, cut,
                        prm->Natom, dim, frozen);
      t2 = seconds();
      tpair += t2 - t1;
      t1 = t2;
   }

   /*
    * If OPENMP is defined and NOREDUCE is not defined, allocate a
    * gradient vector for each thread, and let each thread initialize
    * its gradient vector so that the "first touch" strategy will
    * allocate local memory.  If OpenMP is not defined or if NOREDUCE
    * is defined, allocate one gradient vector only.
    *
    * Note: the following allocations assume that the dimensionality
    * of the problem does not change during one invocation of NAB.
    * If, for example, mme34 were called with dim==3 and then with dim==4,
    * these allocations would not be repeated for the larger value
    * of n that would be necessitated by dim==4.
    */

   if (grad == NULL) {
      grad = vector(0, numcopies * dim * n);
   }
#if (defined(SPEC_OMP) || defined(OPENMP)) && !defined(NOREDUCE)
#pragma omp parallel private (i, goff)
   {
      goff = dim * n * omp_get_thread_num();
      for (i = 0; i < dim * prm->Natom; i++) {
         grad[goff + i] = 0.0;
      }
   }
#else
   for (i = 0; i < dim * prm->Natom; i++) {
      grad[i] = 0.0;
   }
#endif

   t2 = seconds();
   tmme += t2 - t1;
   t1 = t2;

   ebh = ebond(prm->Nbonh, prm->BondHAt1, prm->BondHAt2,
               prm->BondHNum, prm->Rk, prm->Req, x, grad);
   eba = ebond(prm->Mbona, prm->BondAt1, prm->BondAt2,
               prm->BondNum, prm->Rk, prm->Req, x, grad);
   ene[3] = ebh + eba;
   t2 = seconds();
   tbond += t2 - t1;
   t1 = t2;

   eth = eangl(prm->Ntheth, prm->AngleHAt1, prm->AngleHAt2,
               prm->AngleHAt3, prm->AngleHNum, prm->Tk, prm->Teq, x, grad);
   eta =
       eangl(prm->Ntheta, prm->AngleAt1, prm->AngleAt2, prm->AngleAt3,
             prm->AngleNum, prm->Tk, prm->Teq, x, grad);
   ene[4] = eth + eta;
   t2 = seconds();
   tangl += t2 - t1;
   t1 = t2;

   eph = ephi(prm->Nphih, prm->DihHAt1, prm->DihHAt2,
              prm->DihHAt3, prm->DihHAt4, prm->DihHNum,
              prm->Pk, prm->Pn, prm->Phase, x, grad);
   epa = ephi(prm->Mphia, prm->DihAt1, prm->DihAt2,
              prm->DihAt3, prm->DihAt4, prm->DihNum,
              prm->Pk, prm->Pn, prm->Phase, x, grad);
   ene[5] = eph + epa;
   ene[6] = 0.0;                /*  hbond term not in Amber-94 force field */
   t2 = seconds();
   tphi += t2 - t1;
   t1 = t2;

   /* In the following lpairs is a dummy argument that is not used. */

   nbond(lpairs, prm->N14pairs, N14pearlist, 1, x, grad, &enb14, &eel14,
         scnb, scee);
   ene[7] = enb14;
   ene[8] = eel14;
   t2 = seconds();
   tnonb += t2 - t1;
   t1 = t2;

   if (e_debug) {
      EXPR("%9.3f", enb14);
      EXPR("%9.3f", eel14);
   }

   if (nconstrained) {
      ecn = econs(x, grad);
      t2 = seconds();
      tcons += t2 - t1;
      t1 = t2;
   } else
      ecn = 0.0;
   ene[9] = ecn;

   if (gb) {
     e_gb =
       egb(lpairs, upairs, pairlist, lpairsnp, upairsnp,
	   pairlistnp, x, grad, prm->Fs, prm->Rborn, prm->Charges,
	   &kappa, &epsext, &enb, &eel, &esurf, &evdwnp, 0);
     t2 = seconds();
     tborn += t2 - t1;
     t1 = t2;
     ene[1] = enb;
     ene[2] = eel;
     ene[10] = e_gb;
     ene[11] = esurf;
     ene[12] = evdwnp;
     if (e_debug) {
       EXPR("%9.3f", enb);
       EXPR("%9.3f", eel);
       EXPR("%9.3f", e_gb);
       EXPR("%9.3f", esurf);
       EXPR("%9.3f", evdwnp);
     }
   } else {
     nbond(lpairs, upairs, pairlist, 0, x, grad, &enb, &eel, 1.0, 1.0);
     t2 = seconds();
     tnonb += t2 - t1;
     t1 = t2;
     ene[1] = enb;
     ene[2] = eel;
     ene[10] = 0.0;
     ene[11] = 0.0;
     ene[12] = 0.0;
     if (e_debug) {
       EXPR("%9.3f", enb);
       EXPR("%9.3f", eel);
     }
   }

   /*
    * Perform a reduction over the gradient vector if OPENMP is defined
    * and NOREDUCE is not defined, or if MPI is defined.
    *
    * If OPENMP is defined and NOREDUCE is not defined, the reduction
    * is performed either logarithmically or not.
    *
    * If MPI is defined, the reduction is performed by MPI_Allreduce.
    */

#if (defined(SPEC_OMP) || defined(OPENMP)) && !defined(NOREDUCE)

   t1 = seconds();

#undef MME_LOGARITHMIC_REDUCTION

#ifdef MME_LOGARITHMIC_REDUCTION

   /*
    * Here is the logarithmic reduction for OpenMP.
    * Initialize the reqack array.
    */

   for (i = 0; i < maxthreads; i++) {
     reqack[i] = 0;
   }

#pragma omp parallel \
  private (i, iteration, mask, consumer, producer, threadnum, goff)
   {

     /*
      * If MME_OMP_FLUSH is not defined, synchronize the threads
      * via '#pragma omp barrier' which can be costly due to the
      * need to synchronize all of the threads.
      *
      * If MME_OMP_FLUSH is defined, four-cycle signaling will be
      * used to synchronize the threads, as will be seen below,
      * but here we need a '#pragma omp flush' so that the request
      * and acknowledge flags are read correctly by all of the threads.
      */

#define MME_OMP_FLUSH

#ifndef MME_OMP_FLUSH
#pragma omp barrier
#else
#pragma omp flush
#endif

     /*
      * Calculate the iterations for the log2 reduction of the grad array.
      * Note that each OpenMP thread determines 'consumer' and 'producer'
      * from its thread number.
      */

     threadnum = omp_get_thread_num();
     iteration = maxthreads - 1;
     mask = 1;
     while (iteration > 0) {
       consumer = threadnum & (~mask);
       producer = consumer | ((mask + 1) >> 1);

       /*
	* 'Consumer' designates a thread to which to add data from a
	* 'producer' thread.  Perform reduction only when both consumer
	* and producer are less than maxthreads.
	*
	* For successive iterations of the loop mask will have the values
	* 1, 3, 7, 15../
	*
	* The for the example of maxThreads=14 (numThreads=0..13), the
	* following threads will be chosen by consumer and producer:
	*
	* (iteration 1, consumer) - 0, 2, 4, 6, 8, 10, 12
	* (iteration 1, producer) - 1, 3, 5, 7, 9, 11, 13
	*
	* (iteration 2, consumer) - 0, 4, 8,  12
	* (iteration 2, producer) - 2, 6, 10, 13
	*
	* (iteration 3, consumer) - 0, 8
	* (iteration 3, producer) - 4, 12
	*
	* (iteration 4, consumer) - 0
	* (iteration 4, producer) - 8
	*
	* As the example shows, the final result is found in the
	* grad array for thread 0.
	*
	* Note that the following if statement uses maxthreads to
	* determine whether to perform a reduction step.  Maxthreads
	* may not be used to determine whether to execute the while loop
	* (above) because all threads must execute the while loop in order
	* that the '#pragma omp barrier' within the loop not hang due to
	* threads that are not executing the loop.  The test for
	* (threadNum == consumer) guarantees that this thread will accept
	* data from a producer thread which has (threadNum == producer).
	* The test for (producer < maxthreads) guarantees that the producer
	* thread exists.
	*/

       if ( ( threadnum == consumer ) && ( producer < maxthreads ) ) {

	 /*
	  * If MME_OMP_FLUSH is defined, four-cycle signaling is used to
	  * synchronize the threads.  The 'consumer' thread raises its
	  * request flag then waits for the 'producer' thread to raise
	  * its acknowledge flag.
	  *
	  * The request flag is set by assigning the value of 'iteration'
	  * to reqack[consumer].  The acknowledge flag is set by assigning
	  * the value of iteration to reqack[producer].  This value
	  * is used instead of 1 in order that each iteration of the loop
	  * have a unique value for the request and acknowledge flags.
	  * This approach avoids a race condition across iterations of
	  * the loop for access to the request and acknowledge flags.
	  *
	  * If MME_OMP_FLUSH is not defined then no test is necessary
	  * because '#pragma omp barrier' is used to resynchronize
	  * all threads following each iteration of the while loop.
	  */

#ifdef MME_OMP_FLUSH
	 reqack[consumer] = iteration;
#pragma omp flush
	 do {
#pragma omp flush
	 } while (reqack[producer] != iteration);
#endif

	 /*
	  * The producer and consumer threads are synchronized,
	  * so add the grad array from the producer to the
	  * grad array from the consumer.
	  */

	 goff = dim * prm->Natom;
	 for (i = 0; i < goff; i++) {
	   grad[consumer * goff + i] += grad[producer * goff + i];
	 }
	
	 /*
	  * If MME_OMP_FLUSH is defined, four-cycle signaling is used to
	  * synchronize the threads.  The 'consumer' thread lowers its
	  * request flag then waits for the 'producer' thread to lower
	  * its acknowledge flag.
	  */

#ifdef MME_OMP_FLUSH
	 reqack[consumer] = 0;
#pragma omp flush
	 do {
#pragma omp flush
	 } while (reqack[producer] == iteration);
#endif

       }

       /*
	* Here is the if statement that controls whether an OpenMP thread
	* is a producer.  Note that because consumer never equals producer,
	* a thread cannot be both the consumer and producer during a given
	* iteration of the while loop.
	*
	* Because the grad array contents are copied by the consumer thread,
	* the producer thread needs only to synchronize via four-cycle
	* signaling.
	*
	* It is necessary not only to check that (threadnum == producer)
	* but also that (threadnum < maxthreads) to ensure that the
	* producer thread exists.
	*/

       if ( ( threadnum == producer ) && ( threadnum < maxthreads) ) {

	 /*
	  * If MME_OMP_FLUSH is defined, four-cycle signaling is used to
	  * synchronize the threads.  The 'producer' thread waits for
	  * the 'consumer' thread to raise its request flag, then raises
	  * its acknowledge flag, then waits for the 'consumer' thread
	  * to lower its request flag (indicating that the 'consumer'
	  * thread has read the data), then lowers its acknowledge flag.
	  */

#ifdef MME_OMP_FLUSH
	 do {
#pragma omp flush
	 } while (reqack[consumer] != iteration);
	 reqack[producer] = iteration;
#pragma omp flush

	 do {
#pragma omp flush
	 } while (reqack[consumer] == iteration);
	 reqack[producer] = 0;
#pragma omp flush
#endif

       }

       /*
	* If MME_OMP_FLUSH is not defined resynchronize via
	* '#pragma omp barrier'.
	*/

#ifndef MME_OMP_FLUSH
#pragma omp barrier
#endif

       /* Prepare for the next iteration of the while loop. */

       mask = (mask << 1) + 1;
       iteration >>= 1;
     }
   }

   /* Now copy the reduced grad array into the f array. */

   for (i = 0; i < dim * prm->Natom; i++) {
     f[i] = grad[i];
   }

#else

   /*
    * Here is the non-logarithmic reduction of the grad array
    * under OpenMP.  Begin by copying the grad array for thread 0
    * into the f array.
    */

   goff = dim * prm->Natom;
   for (i = 0; i < goff; i++) {
     f[i] = grad[i];
   }

   /*
    * Now add the grad arrays for all other threads to the f array.
    * Each thread copies a portion of each array.  The (j,i) loop
    * nesting is more efficient than (i,j) nesting.
    *
    * Note: the following 'if' should not be needed, but works around a
    * bug in ifort 9.1 on ia64.
    */

   if (maxthreads > 1) {
#pragma omp parallel for private(i, j) schedule(dynamic, blocksize)
     for (j = 0; j < goff; j++) {
       for (i = 1; i < maxthreads; i++) {
	 f[j] += grad[goff * i + j];
       }
     }
   }

#endif

   /* Update the reduction time. */

   t2 = seconds();
   treduce += t2 - t1;
   t1 = t2;

#elif defined(MPI) || defined(SCALAPACK)

   /* Here is the reduction of the grad array under MPI. */

   ierror = MPI_Allreduce(grad, f, dim * prm->Natom,
			  MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
   if (ierror != MPI_SUCCESS) {
     fprintf( nabout,"Error in mme34 grad reduction, error = %d  mytaskid = %d\n",
	      ierror, mytaskid);
   }

   /* Update the reduction time. */

   t2 = seconds();
   treduce += t2 - t1;
   t1 = t2;

#else

   /* Here is no reduction of the grad array.  Copy it to the f array. */

   for (i = 0; i < dim * prm->Natom; i++) {
     f[i] = grad[i];
   }

#endif

   for (k = 0; k < prm->Natom; k++) {   /* zero out frozen forces */
      if (frozen[k]) {
         f[dim * k + 0] = f[dim * k + 1] = f[dim * k + 2] = 0.0;

         if (dim == 4) {
            f[dim * k + 3] = 0.0;
         }
      }
   }

#ifdef PRINT_DERIV
   k = 0;
   for (i = 0; i < 105; i++) {
      k++;
      fprintf(nabout, "%10.5f", f[i]);
      if (k % 8 == 0)
         fprintf(nabout, "\n");
   }
   fprintf(nabout, "\n");
#endif

   /* Calculate the rms gradient. */

   frms = 0.0;
   for (i = 0; i < dim * prm->Natom; i++)
      frms += f[i] * f[i];
   frms = sqrt(frms / (dim * prm->Natom));

   /* Calculate the total energy. */

   ene[0] = 0.0;
   for (k = 1; k <= 12; k++) {
      ene[0] += ene[k];
   }

   /* If MPI is defined perform a reduction of the ene array. */

#if defined(MPI) || defined(SCALAPACK)
   ierror = MPI_Allreduce(ene, reductarr, 13, MPI_DOUBLE,
                          MPI_SUM, MPI_COMM_WORLD);
   if (ierror != MPI_SUCCESS) {
      fprintf(nabout,
              "Error in mme34 ene reduction, error = %d  mytaskid = %d\n",
              ierror, mytaskid);
   }
   for (i = 0; i <= 12; i++) {
      ene[i] = reductarr[i];
   }
#endif

   /*
    * Print the energies and rms gradient but only for task zero,
    * and only for positive values of the iteration counter.
    */

   if (mytaskid == 0) {
      if (*iter > -1 && (*iter == 0 || *iter % ntpr == 0)) {
         fprintf(nabout,
                 "ff:%6d %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2e\n",
                 *iter, ene[0], ene[3] + ene[4] + ene[5],
                 ene[1] + ene[7], ene[2] + ene[8],
                 ene[9] + ene[11] + ene[12], ene[10], frms);
         fflush(nabout);
      }
   }

   /* A value of -1 for the iteration counter is reserved for printing. */

   if (*iter == -1) {
      fprintf(nabout, "     bond:  %15.9f\n", ene[3]);
      fprintf(nabout, "    angle:  %15.9f\n", ene[4]);
      fprintf(nabout, " dihedral:  %15.9f\n", ene[5]);
      fprintf(nabout, "    enb14:  %15.9f\n", ene[7]);
      fprintf(nabout, "    eel14:  %15.9f\n", ene[8]);
      fprintf(nabout, "      enb:  %15.9f\n", ene[1]);
      fprintf(nabout, "      eel:  %15.9f\n", ene[2]);
      fprintf(nabout, "      egb:  %15.9f\n", ene[10]);
      fprintf(nabout, "    econs:  %15.9f\n", ene[9]);
      fprintf(nabout, "    esurf:  %15.9f\n", ene[11]);
      fprintf(nabout, "    Total:  %15.9f\n", ene[0]);
   }

   /* If static_arrays is 0, deallocate the gradient and reqack arrays. */

   if (!static_arrays) {
     if (grad != NULL) {
       free_vector(grad, 0, maxthreads * dim * n);
       grad = NULL;
     }
     if (reqack != NULL) {
       free_ivector(reqack, 0, maxthreads);
       reqack = NULL;
     }
   }

   t2 = seconds();
   tmme += t2 - t1;

   return (ene[0]);
}

/***********************************************************************
                            MME_TIMER()
************************************************************************/

/* Print a timing summary but only for task zero. */

int mme_timer(void)
{
   /* Use the maximum time from all MPI tasks or SCALAPACK processes. */

#if defined(MPI) || defined(SCALAPACK)

   REAL_T timarr[10], reductarr[10];

   timarr[0] = tcons;
   timarr[1] = tbond;
   timarr[2] = tangl;
   timarr[3] = tphi;
   timarr[4] = tpair;
   timarr[5] = tnonb;
   timarr[6] = tborn;
   timarr[7] = tmme;
   timarr[8] = tconjgrad;
   timarr[9] = tmd;

   MPI_Allreduce(timarr, reductarr, 10, MPI_DOUBLE, MPI_MAX,
                 MPI_COMM_WORLD);

   tcons = reductarr[0];
   tbond = reductarr[1];
   tangl = reductarr[2];
   tphi = reductarr[3];
   tpair = reductarr[4];
   tnonb = reductarr[5];
   tborn = reductarr[6];
   tmme = reductarr[7];
   tconjgrad = reductarr[8];
   tmd = reductarr[9];

#endif

   if (mytaskid == 0) {
      fprintf(nabout, "\nFirst derivative timing summary:\n");
      fprintf(nabout, "   constraints %10.2f\n", tcons);
      fprintf(nabout, "   bonds       %10.2f\n", tbond);
      fprintf(nabout, "   angles      %10.2f\n", tangl);
      fprintf(nabout, "   torsions    %10.2f\n", tphi);
      fprintf(nabout, "   pairlist    %10.2f\n", tpair);
      fprintf(nabout, "   nonbonds    %10.2f\n", tnonb);
      fprintf(nabout, "   gen. Born   %10.2f\n", tborn);
      fprintf(nabout, "   mme         %10.2f\n", tmme);
      fprintf(nabout, "   Total       %10.2f\n\n",
              tcons + tbond + tangl + tphi + tpair + tnonb + tborn + tmme);
      fprintf(nabout, "   reduction   %10.2f\n", treduce);
      fprintf(nabout, "   molec. dyn. %10.2f\n\n", tmd);
      fflush(nabout);
   }

   return (0);
}

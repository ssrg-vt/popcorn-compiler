
/***********************************************************************
                            INTERSECT()
************************************************************************/

/*
 * Calculate the volume and surface area of a set of intersecting atoms.
 *
 * Calling parameters are as follows:
 *
 * s - input: the array of atom numbers
 * x - input: the atomic (x,y,z) coordinates
 * f - uptated: the gradient vector
 * rborn - input: atomic radii
 * area - updated: the intersection area
 * n - input: number of atoms in the set
 * gbsa - input: the gbsa flag (1=LCPO; 2/4=volume; 3/5=surface area)
 */

static
REAL_T intersect(INT_T * s, REAL_T * x, REAL_T * f, REAL_T * rborn,
                 REAL_T * area, INT_T n, INT_T gbsa)
#define PIx4 (12.5663706143591724639918)
#define PIx2 ( 6.2831853071795862319959)
#define PIx1 ( 3.1415926535897931159979)
{
   int i, j, k, o, i34, j34, k34;
   REAL_T vg, vgs, ag, agk, p, rn, pn, ci, cj;
   REAL_T sumci, sumci1i, sumcicjdij2, sumcjdij2, sumcjdkj2;
   REAL_T xi, yi, zi, wi, xj, yj, zj, wj, xij, yij, zij, wij;
   REAL_T xk, yk, zk, wk, xkj, ykj, zkj, wkj, dkj2;
   REAL_T ri, rj, rk, dij2, de, cicjsci, kvgr3ci;
   REAL_T dedx, dedy, dedz, dedw, daix, daiy, daiz, daiw;

   /* Calculate p^n. */

   p = 4.0 * kappanp * sqrt(kappanp / PIx1) / 3.0;
   rn = (REAL_T) n;
   pn = pow(p, rn);

   /*
    * Loop over all atom pairs to form sumcicjdij2.
    *
    * All addressing of atoms is indirect through the array s.
    *
    * Note that for OpenMP execution, this function is part of
    * a parallel region because it was called from within a parallel
    * region by the egb function.  Thus, there is no need to
    * parallelize from within this function.
    */

   sumci = sumcicjdij2 = 0.0;

   for (i = 0; i < n; i++) {

      i34 = dim * s[i];

      xi = x[i34];
      yi = x[i34 + 1];
      zi = x[i34 + 2];

      if (dim == 4) {
         wi = x[i34 + 3];
      }

      ri = rborn[s[i]] + dradius;
      ci = kappanp / (ri * ri);
      sumci += ci;

      sumcjdij2 = 0.0;

      for (j = i + 1; j < n; j++) {

         j34 = dim * s[j];

         xj = x[j34];
         yj = x[j34 + 1];
         zj = x[j34 + 2];

         xij = xi - xj;
         yij = yi - yj;
         zij = zi - zj;
         dij2 = xij * xij + yij * yij + zij * zij;

         if (dim == 4) {
            wj = x[j34 + 3];
            wij = wi - wj;
            dij2 += wij * wij;
         }

         rj = rborn[s[j]] + dradius;
         cj = kappanp / (rj * rj);
         sumcjdij2 += cj * dij2;
      }
      sumcicjdij2 += ci * sumcjdij2;
   }

   /*
    * Calculate then negate the volume for sets that contain even numbers
    * of atoms.  There is no need to divide by the number of atoms in
    * the set because the volume calculation is not performed on a per-atom
    * basis, and is confined to the lower or upper triangle of the pair lists.
    * See equation (2) of Gallicchio and Levy. 
    */

   sumci1i = 1.0 / sumci;
   vg = pn * exp(-sumcicjdij2 * sumci1i) * PIx1 * sumci1i * sqrt(PIx1 *
                                                                 sumci1i);
   if ((n % 2) == 0) {
      vg = -vg;
   }

   /* Set the surface area to zero in case a volume calculation is requested. */

   ag = 0.0;

   /* Is a volume calculation requested? */

   if ((gbsa == 2) || (gbsa == 4)) {

      /*
       * Calculate the derivatives.  The proper offset into the global gradient
       * array was provided by calling the volumeset function with the &f[foff]
       * parameter.
       */

      for (i = 0; i < n; i++) {

         i34 = dim * s[i];

         xi = x[i34];
         yi = x[i34 + 1];
         zi = x[i34 + 2];

         if (dim == 4) {
            wi = x[i34 + 3];
         }

         ri = rborn[s[i]] + dradius;
         ci = kappanp / (ri * ri);

         /* Initialize the derivative accumulators. */

         daix = daiy = daiz = daiw = 0.0;

         for (j = i + 1; j < n; j++) {

            j34 = dim * s[j];

            xj = x[j34];
            yj = x[j34 + 1];
            zj = x[j34 + 2];

            xij = xi - xj;
            yij = yi - yj;
            zij = zi - zj;

            if (dim == 4) {
               wj = x[j34 + 3];
               wij = wi - wj;
            }

            rj = rborn[s[j]] + dradius;
            cj = kappanp / (rj * rj);

            /* The first derivative with respect to Dij, divided by Dij. */

            cicjsci = ci * cj * sumci1i;
            de = -2.0 * cicjsci * vg * surften;

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

            f[j34] -= dedx;
            f[j34 + 1] -= dedy;
            f[j34 + 2] -= dedz;

            if (dim == 4) {
               dedw = de * wij;
               daiw += dedw;
               f[j34 + 3] -= dedw;
            }
         }

         /* Update the i elements of the gradient. */

         f[i34] += daix;
         f[i34 + 1] += daiy;
         f[i34 + 2] += daiz;

         if (dim == 4) {
            f[i34 + 3] += daiw;
         }
      }
   }

   /* Is a surface area calculation requested? */

   else if ((gbsa == 3) || (gbsa == 5)) {

      /*
       * Divide Vg by the number of atoms because, for the derivative and area
       * calculation that follows, k ranges over all atoms and results are summed.
       *
       * Each atom in the set takes its turn as the principal atom k.  This
       * approach permits the more efficient use of summation indices as in
       * equation (2) instead of equation (3) of Gallicchio and Levy, and also
       * permits re-used of Vg in the calculation of several Ag's, as shown
       * by equations (47) and (48) of Gallicchio and Levy.  However, if
       * use_lower_tri==1 equation (3) is used instead of equation (2).
       */

      vgs = vg / rn;

      if (use_lower_tri) {
         o = 1;
      } else {
         o = n;
      }
      for (k = 0; k < o; k++) {

         k34 = dim * s[k];

         xk = x[k34];
         yk = x[k34 + 1];
         zk = x[k34 + 2];

         if (dim == 4) {
            wk = x[k34 + 3];
         }

         /* Loop over all atoms except atom k to form sumcjdkj2. */

         sumcjdkj2 = 0.0;
         for (j = 0; j < n; j++) {
            if (k == j)
               continue;

            j34 = dim * s[j];

            xj = x[j34];
            yj = x[j34 + 1];
            zj = x[j34 + 2];

            xkj = xk - xj;
            ykj = yk - yj;
            zkj = zk - zj;
            dkj2 = xkj * xkj + ykj * ykj + zkj * zkj;

            if (dim == 4) {
               wj = x[j34 + 3];
               wkj = wk - wj;
               dkj2 += wkj * wkj;
            }

            rj = rborn[s[j]] + dradius;
            cj = kappanp / (rj * rj);
            sumcjdkj2 += cj * dkj2;
         }

         /* Calculate the contribution of atom k to surface area Ag = d(Vg)/d(Rm). */

         rk = rborn[s[k]] + dradius;
         kvgr3ci = 2.0 * kappanp * vgs * sumci1i / (rk * rk * rk);
         agk = (sumcjdkj2 - sumcicjdij2 * sumci1i + 1.5) * kvgr3ci;

         /*
          * Calculate the derivatives.  The proper offset into the global gradient
          * array was provided by calling the gradset function with the &f[foff]
          * parameter.
          */

         for (i = 0; i < n; i++) {

            i34 = dim * s[i];

            xi = x[i34];
            yi = x[i34 + 1];
            zi = x[i34 + 2];

            if (dim == 4) {
               wi = x[i34 + 3];
            }

            ri = rborn[s[i]] + dradius;
            ci = kappanp / (ri * ri);

            /* Initialize the derivative accumulators. */

            daix = daiy = daiz = daiw = 0.0;

            for (j = i + 1; j < n; j++) {

               j34 = dim * s[j];

               xj = x[j34];
               yj = x[j34 + 1];
               zj = x[j34 + 2];

               xij = xi - xj;
               yij = yi - yj;
               zij = zi - zj;

               if (dim == 4) {
                  wj = x[j34 + 3];
                  wij = wi - wj;
               }

               rj = rborn[s[j]] + dradius;
               cj = kappanp / (rj * rj);

               /*
                * Calculate d(Ag)/d(Dij) but because this derivative must be
                * divided by Dij, Dij does not appear in the expression.
                *
                * Start with the Kronecker deltas inside of the parenthesis.
                */

               if (i == k) {
                  de = cj;
               } else if (j == k) {
                  de = ci;
               } else {
                  de = 0.0;
               }

               /* Finish the parenthesis and continue with the multipliers. */

               cicjsci = ci * cj * sumci1i;
               de -= cicjsci;
               de *= 2.0 * kvgr3ci;

               /* Subtract off the second term and multiply by the surface tension. */

               de -= 2.0 * cicjsci * agk;
               de *= surften;

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

               f[j34] -= dedx;
               f[j34 + 1] -= dedy;
               f[j34 + 2] -= dedz;

               if (dim == 4) {
                  dedw = de * wij;
                  daiw += dedw;
                  f[j34 + 3] -= dedw;
               }
            }

            /* Update the i elements of the gradient. */

            f[i34] += daix;
            f[i34 + 1] += daiy;
            f[i34 + 2] += daiz;

            if (dim == 4) {
               f[i34 + 3] += daiw;
            }
         }

         /* Sum the contribution of atom k to the surface area. */

         ag += agk;
      }
   }

   /*
    * Return the area via a by reference calling parameter,
    * and return the volume as the value of this function.
    */

   *area = ag;
   if (use_lower_tri) {
      return (vgs);
   } else {
      return (vg);
   }
}

/***********************************************************************
                            ATOMSET()
************************************************************************/

/*
 * Conduct a "depth first" calculation of the intersection volume
 * of a set of possibly intersecting atoms treated as 3D gaussian
 * functions, per Gallicchio and Levy, J. Comput. Chem. 25: 479 (2004)
 * and Weiser, Shenkin and Still, J. Comput. Chem. 20: 688 (1999),
 * as follows.  For the atom i determine whether the pair list contains
 * at least one atom, and if so, calculate the intersection volume
 * for the doublet ij1.  If the volume is less than min_volume, continue
 * with the calculation of the intersection volume for the doublet
 * ij2.  If, however, the volume of the doublet ij1 is greater than
 * min_volume, calculate the higher-order intersection volume for
 * the triplet ij1j2 (if the pair list contains at least two atoms),
 * then test the volume of ij1j2 against min_volume.  If the volume
 * of ij1j2 is less than min_volume, continue with the calculation of
 * the intersection volume of the triplet ij1j3 (if the pair list
 * contains at least three atoms).  If the volume of the triplet ij1j2
 * is greater than min_volume, calculate the higher-order intersection
 * volume for the quadruplet ij1j2j3 (if the pair list contains
 * at least three atoms).  Whenever the pair list does not contain enough
 * atoms to form the desired multiplet, return to the next lower order
 * and continue the computation.  For example, consider the above case
 * where the volume of triplet ij1j2 is greater than min_volume and hence
 * the computation should proceed to the next higher order with the
 * calculation of the intersection volume  of the quartet ij1j2j3.
 * If in this case the pair list does not contain at least four atoms,
 * the computation will continue with the calculation of the intersection
 * volume of the doublet ij2.
 *
 * Calculate the intersection surface area as well.
 *
 * Calling parameters are as follows:
 *
 * setlist - input: the pair list for the principal atom
 * start - input: the starting index in the pair list
 * finish - input: the ending index in the pair list
 * index - input: the index into setarray
 * gbsa - input: the gbsa flag (2/4=volume 3/5=surface area)
 * setarray - updated: the array of atom numbers
 * x - input: the atomic (x,y,z) coordinates
 * f - updated: the gradient vector
 * rborn - input: atomic radii
 * surfarea - updated: the surface area of the set of atoms
 * maxdepth - updated: the maximum depth of recursion
 */

static
REAL_T atomset(INT_T * setlist, INT_T start, INT_T finish, INT_T index,
               INT_T gbsa, INT_T * setarray, REAL_T * x, REAL_T * f,
               REAL_T * rborn, REAL_T * surfarea, INT_T * maxdepth)
{
   int j, i34, j34;
   REAL_T volume, totvolume, area, ri, rj;
   REAL_T xi, yi, zi, wi, xj, yj, zj, wj, xij, yij, zij, wij, dij2;

   totvolume = 0.0;
   if (index >= max_set_size) {
      printf
          ("Overflow in atomset; increase either max_set_size or min_volume!\n");
      return (totvolume);
   }
   if (*maxdepth < index) {
      *maxdepth = index;
   }

   if (cull_np_lists) {
      i34 = dim * setarray[0];

      xi = x[i34];
      yi = x[i34 + 1];
      zi = x[i34 + 2];

      if (dim == 4) {
         wi = x[i34 + 3];
      }
   }

   ri = rborn[setarray[0]] + dradius;

   for (j = start; j < finish; j++) {

      /* Don't add atom j to the atom set if dij2 is too large. */

      if (cull_np_lists) {
         j34 = dim * setlist[j];

         xj = x[j34];
         yj = x[j34 + 1];
         zj = x[j34 + 2];

         xij = xi - xj;
         yij = yi - yj;
         zij = zi - zj;
         dij2 = xij * xij + yij * yij + zij * zij;

         if (dim == 4) {
            wj = x[j34 + 3];
            wij = wi - wj;
            dij2 += wij * wij;
         }

         rj = rborn[setlist[j]] + dradius;

         if (dij2 > (ri + rj + deltar) * (ri + rj + deltar))
            continue;
      }
      setarray[index] = setlist[j];
      volume = intersect(setarray, x, f, rborn, &area, index + 1, gbsa);
      totvolume += volume;
      *surfarea += area;

      /* Compute the next higher order intersection volume if necessary. */

      if (use_lower_tri) {
         if (fabs(((REAL_T) (index + 1)) * volume) < min_volume)
            continue;
      } else {
         if (fabs(volume) < min_volume)
            continue;
      }
      totvolume += atomset(setlist, j + 1, finish, index + 1, gbsa,
                           setarray, x, f, rborn, surfarea, maxdepth);
   }
   return (totvolume);
}



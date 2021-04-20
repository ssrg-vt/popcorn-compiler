   /*
    * Calculate either the volume or the surface area component of the AGB
    * nonpolar hydration free energy.  Use Eqs 2-15 of Gallicchio and Levy,
    * J. Comput. Chem. 25: 479 (2004).
    *
    * The gbsa option is interpreted as follows:
    * 0 - no nonpolar calculation
    * 1 - original LCPO approximation
    * 2 - volume cavity term
    * 3 - surface area cavity term
    * 4 - van der Waals term plus volume cavity term
    * 5 - van der Waals term plus surface area cavity term
    *
    * Contributed by Russ Brown (russ.brown@sun.com)
    *
    * This implementation requires many terms of the series in order that
    * the first and second derivatives converge.  Apparently, Gallicchio
    * and Levy use a "truncated Gaussian" that is not described in their
    * J. Comput. Chem. article and that permits convergence with fewer
    * terms.  This implementation might benefit from that enhancement.
    */

   if ((gbsa >= 2) && (gbsa <= 5)) {

      /* Initialize some summation variables. */

      maxmaxdepth = 0;
      evdwnp = totalvolume = surfacearea = 0.0;

      /*
       * Dynamically schedule iterations of the following loop for MPI
       * and SCALAPACK but only if there are enough MPI processes to
       * justify using one of them as a master scheduler.  Otherwise,
       * use the code that schedules the loop in a static, round-robin
       * manner with a block size of 1.  Dynamic scheduling is preferred
       * because the "depth first" algorithm causes an unpredictable and
       * non-uniform distribution of processing load.  Dynamic scheduling
       * is disabled for MPI and SCALAPACK if dynamic_loops is zero.
       */

#if defined(MPI) || defined(SCALAPACK)

      if ((dynamic_loops) && (numtasks >= MPI_min_tasks)
          && (numtasks > 1)) {

         /* Is this MPI process the master process? */

         if (mytaskid == 0) {

            /*
             * The master MPI process (mytaskid==0) loops over all of the
             * principal atoms, assigning the next principal atom and its
             * pair list to the next available non-master process.  The
             * master process waits via MPI_Recv for a request from any
             * non-master process, and acknowleges that request by sending
             * the atom number i of the principal atom to the non-master
             * process via MPI_send.
             */

            for (i = 0; i < prm->Natom; i++) {

               /* Do nothing if the principal atom is frozen. */

               if (frozen[i])
                  continue;

               /* Check for correctness of the pair list.  Non-graceful error handling. */

               if (pearlistnp[i] == NULL) {
                  fprintf(nabout,
                          "NULL non-polar pair list in egb, taskid = %d\n",
                          mytaskid);
                  fflush(nabout);
               }

               if (MPI_Recv(&sender, 1, MPI_INT, MPI_ANY_SOURCE,
                            MPI_ANY_TAG, MPI_COMM_WORLD,
                            &status) != MPI_SUCCESS) {
                  printf("egb: cannot recv sender in first loop!\n");
                  MPI_Finalize();
                  exit(1);
               }

               if ((sender <= 0) || (sender >= numtasks)) {
                  printf
                      ("egb: received illegal sender task = %d in first loop\n",
                       sender);
                  MPI_Finalize();
                  exit(1);
               }

               if (MPI_Send(&i, 1, MPI_INT, sender,
                            0, MPI_COMM_WORLD) != MPI_SUCCESS) {
                  printf("egb: cannot send index to task %d\n", sender);
                  MPI_Finalize();
                  exit(1);
               }
            }

            /*
             * When the master process has finished looping over all of
             * the principal atoms, it enters a second loop that sends
             * a termination code of -1 (instead of the atom number i)
             * to each of the numtasks-1 non-master processes.
             */

            for (i = 1; i < numtasks; i++) {

               if (MPI_Recv(&sender, 1, MPI_INT, MPI_ANY_SOURCE,
                            MPI_ANY_TAG, MPI_COMM_WORLD,
                            &status) != MPI_SUCCESS) {
                  printf("egb: cannot recv sender in second loop!\n");
                  MPI_Finalize();
                  exit(1);
               }

               if ((sender <= 0) || (sender >= numtasks)) {
                  printf
                      ("egb: received illegal sender task = %d in second loop\n",
                       sender);
                  MPI_Finalize();
                  exit(1);
               }

               if (MPI_Send(&minusone, 1, MPI_INT, sender,
                            0, MPI_COMM_WORLD) != MPI_SUCCESS) {
                  printf("egb: cannot send -1 to task %d\n", sender);
                  MPI_Finalize();
                  exit(1);
               }
            }
         }

         /* This MPI process is not the master process. */

         else {

            /* Allocate setarray for this MPI process. */

            setarray = ivector(0, max_set_size);

            /*
             * Each non-master MPI process requests via MPI_Send a
             * principal atom number i from the master MPI process.
             * Then the non-master process waits for an acknowlege
             * via MPI_Recv.  If the atom number i that it receives
             * is in the allowed range, the non-master process calls
             * atomset for the atom number.  If the atom number is
             * not in the allowed range (i.e., i < 0), the non-master
             * process breaks out of the send/receive loop.
             */

            while (1) {


               if (MPI_Send(&mytaskid, 1, MPI_INT, 0,
                            mytaskid, MPI_COMM_WORLD) != MPI_SUCCESS) {
                  printf
                      ("egb: task %d cannot send index to task 0\n",
                       mytaskid);
                  MPI_Finalize();
                  exit(1);
               }

               if (MPI_Recv(&i, 1, MPI_INT, 0,
                            0, MPI_COMM_WORLD, &status) != MPI_SUCCESS) {
                  printf("egb: task %d cannot recv i!\n", mytaskid);
                  MPI_Finalize();
                  exit(1);
               }

               /* Check message for correctness and break from loop if necessary. */

               if (i == -1) {
                  break;
               } else if ((i < -1) || (i >= prm->Natom)) {
                  printf("egb: task %d received illegal value %d\n", mytaskid, i);
                  MPI_Finalize();
                  exit(1);
               }

               /*
                * Store the principal atom number into the array of atom numbers
                * and initialize the maximum depth variable.
                */

               setarray[0] = i;
               maxdepth = 0;

               /*
                * Use the atoms from the upper triangle pair list because equation
                * (2) of Gallicchio and Levy will be used to compute the volume.
                * The per-atom volume will not be calculated because that volume
                * uses equation (3) of Gallicchio and Levy, and therefore requires
                * the lower triangle pair list as well.  Instead, the contribution
                * of the atom to the volume (and surface area) is calculated and
                * summed.  This approach executes more rapidly than calculating
                * the per-atom volume and surface area.  However, identical results
                * are obtained either way.  Also, if use_lower_tri==1 use eq. (3).
                *
                * The principal atom is not on its own pair list.
                *
                * It appears that the switching function described by equation (40) of
                * Gallicchio and Levy is not required because the atomic surface area,
                * calculated using both pair lists, was never observed to be negative.
                */

               if (use_lower_tri) {
                  fpair = 0;
               } else {
                  fpair = lpearsnp[i];
               }
               npairs = lpearsnp[i] + upearsnp[i];

               /* Execute the "depth-first" algorithm recursively. */

               surfarea = 0.0;
               totvolume =
                   atomset(pearlistnp[i], fpair, npairs, 1, gbsa,
                           setarray, x, &f[foff], rborn, &surfarea,
                           &maxdepth);

               /*
                * Add the volume and surface area of the principal atom to the
                * atomic volume and surface area, respectively.
                */

               radius = rborn[i] + dradius;
               totvolume += PIx4 * radius * radius * radius / 3.0;
               surfarea += PIx4 * radius * radius;

               /*
                * If the both the lower and upper triangle pair lists
                * are used, per-atom volume and surface area are
                * calculated and may be checked for correctness.
                */

               if (gbsa_debug && use_lower_tri) {
                  if (totvolume < 0.0) {
                     printf("atom = %d  totvolume = %e\n", i, totvolume);
                  }
                  if (surfarea < 0.0) {
                     printf("atom = %d  surfarea = %e\n", i, surfarea);
                  }
               }

               /*
                * Add the volume and surface area contributed by atom i
                * to the molecular volume and surface area.
                */

               totalvolume += totvolume;
               surfacearea += surfarea;
               if (maxmaxdepth < maxdepth) {
                  maxmaxdepth = maxdepth;
               }
            }

            /* Deallocate setarray for this MPI process. */

            free_ivector(setarray, 0, max_set_size);
         }
      } else
#endif

         /*
          * The following {} serves either to begin a compound statement
          * for the above "else" or to begin an OpenMP parallel region.
          */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
#pragma omp parallel reduction (+: totalvolume, surfacearea) \
  private (i, foff, fpair, npairs, setarray, threadnum, radius, \
           maxdepth, maxmaxdepth, totvolume, surfarea)
#endif
      {

         /*
          * This region of code is always executed for OpenMP, but for MPI
          * and SCALAPACK only if DYNAMIC_SCHEDULING is not defined or the
          * number of MPI processes is too small to justify using one of
          * them as a master scheduler.
          *
          * Get the OpenMP thread number and compute an offset
          * into the gradient array.
          */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
         threadnum = omp_get_thread_num();
         foff = dim * (prm->Natom) * threadnum;
#else
         foff = 0;
#endif

         /*
	  * Allocate setarray within the OpenMP parallel region.
	  *
	  * Note: instead of allocating and deallocating setarray
	  * for each call to the egb function, it may be better to
	  * allocate and deallocate it once in the eff.c file, as
	  * is done for the sumdeijda and iexw arrays.  For OpenMP
	  * the setarray would be max_set_size * maxthreads in size.
	  * For MPI this array would be max_set_size in size.
	  */

         setarray = ivector(0, max_set_size);

         /*
          * Loop over all atoms, where each atom is the principal atom relative
          * to the atoms on its pair list.  Compose sets of atoms of sizes
          * 2, 3, 4... using the principal atom and all combinations of atoms
          * on the pair list.  Ignore the principal atom and compose no sets
          * if it is frozen.  It is not necessary to test whether atoms from
          * the pair list are frozen because the searchkdtree function does
          * not append frozen atoms to the pair list.
          *
          * Explicitly assign threads to loop indices for the following loop
          * in a static, round-robin manner with a block size of 1 for MPI
          * and SCALAPACK, and use dynamic scheduling with a block size of
          * 1 for OpenMP.
          */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
#pragma omp for schedule (dynamic, 1)
#endif

         for (i = 0; i < prm->Natom; i++) {

#if defined(MPI) || defined(SCALAPACK)

            if ((i % numtasks) != mytaskid)
               continue;

#endif
            /* Do nothing if the principal atom is frozen. */

            if (frozen[i])
               continue;

            /* Check for correctness of the pair list.  Non-graceful error handling. */

            if (pearlistnp[i] == NULL) {
               fprintf(nabout,
                       "NULL non-polar pair list in egb, taskid = %d\n",
                       mytaskid);
               fflush(nabout);
            }

            /*
             * Store the principal atom number into the array of atom numbers
             * and initialize the maximum depth variable.
             */

            setarray[0] = i;
            maxdepth = 0;

            /*
             * Use the atoms from the upper triangle pair list because equation
             * (2) of Gallicchio and Levy will be used to compute the volume.
             * The per-atom volume will not be calculated because that volume
             * uses equation (3) of Gallicchio and Levy, and therefore requires
             * the lower triangle pair list as well.  Instead, the contribution
             * of the atom to the volume (and surface area) is calculated and
             * summed.  This approach executes more rapidly than calculating
             * the per-atom volume and surface area.  However, identical results
             * are obtained either way.  Also, if use_lower_tri==1 use eq. (3).
             *
             * The principal atom is not on its own pair list.
             *
             * It appears that the switching function described by equation (40) of
             * Gallicchio and Levy is not required because the atomic surface area,
             * calculated using both pair lists, was never observed to be negative.
             */

            if (use_lower_tri) {
               fpair = 0;
            } else {
               fpair = lpearsnp[i];
            }
            npairs = lpearsnp[i] + upearsnp[i];

            /* Execute the "depth-first" algorithm recursively. */

            surfarea = 0.0;
            totvolume = atomset(pearlistnp[i], fpair, npairs, 1, gbsa,
                                setarray, x, &f[foff], rborn,
                                &surfarea, &maxdepth);

            /*
             * Add the volume and surface area of the principal atom to the
             * atomic volume and surface area, respectively.
             */

            radius = rborn[i] + dradius;
            totvolume += PIx4 * radius * radius * radius / 3.0;
            surfarea += PIx4 * radius * radius;

            /*
             * If the both the lower and upper triangle pair lists
             * are used, per-atom volume and surface area are
             * calculated and may be checked for correctness.
             */

            if (gbsa_debug && use_lower_tri) {
               if (totvolume < 0.0) {
                  printf("atom = %d  totvolume = %e\n", i, totvolume);
               }
               if (surfarea < 0.0) {
                  printf("atom = %d  surfarea = %e\n", i, surfarea);
               }
            }

            /*
             * Add the volume and surface area contributed by atom i
             * to the molecular volume and surface area.
             */

            totalvolume += totvolume;
            surfacearea += surfarea;
            if (maxmaxdepth < maxdepth) {
               maxmaxdepth = maxdepth;
            }
         }

         /* Deallocate setarray within the OpenMP parallel region. */

         free_ivector(setarray, 0, max_set_size);
      }

      /* Check whether the total molecular volume or surface area is negative. */

      if (gbsa_debug) {
         if (totalvolume < 0.0) {
            printf("total volume = %f  maxmaxdepth = %d\n",
                   totalvolume, maxmaxdepth);
         }
         if (surfacearea < 0.0) {
            printf("surface area = %f  maxmaxdepth = %d\n",
                   surfacearea, maxmaxdepth);
         }
      }

   }

   /*
    * Calculate the cavity (volume or surface area) component of the nonpolar
    * GB energy.  Note that for OpenMP execution, the totalvolume and surfacearea
    * variables are reduction variables.  For MPI, the reduction occurs in the
    * mme34 function via reduction of the ene array.
    */

   if ((gbsa == 2) || (gbsa == 4)) {
      *esurf = totalvolume * surften;
   } else if ((gbsa == 3) || (gbsa == 5)) {
      *esurf = surfacearea * surften;
   } else if (gbsa == 1) {

      /*
       * Main LCPO stuff follows.
       */

      /* Loop over all atoms i. */

      count = 0;
      for (i = 0; i < prm->Natom; i++) {

         xi = x[3 * i];
         yi = x[3 * i + 1];
         zi = x[3 * i + 2];
         ri = rborn[i] - BOFFSET;
         ri1i = 1. / ri;
         sumi = 0.;

         /* Select atom j from the pair list. */

         for (k = 0; k < lpears[i] + upears[i]; k++) {

            j = pearlist[i][k];

            xij = xi - x[3 * j];
            yij = yi - x[3 * j + 1];
            zij = zi - x[3 * j + 2];
            r2 = xij * xij + yij * yij + zij * zij;
            if (r2 > rgbmaxpsmax2)
               continue;
            dij1i = 1.0 / sqrt(r2);
            dij = r2 * dij1i;

            if ((P0[i] + P0[j]) > dij) {
               if (P0[i] > 2.5 && P0[j] > 2.5) {

                  ineighbor[count] = j + 1;
                  /* this +1 is VERY important */
                  count += 1;
               }
            }
         }

         ineighbor[count] = 0;
         count = count + 1;
      }

      totsasa = 0.0;
      count = 0;
      for (i = 0; i < prm->Natom; i++) {
         if (ineighbor[count] == 0) {
            count = count + 1;
         } else {

            si = PIx4 * P0[i] * P0[i];
            sumAij = 0.0;
            sumAjk = 0.0;
            sumAjk2 = 0.0;
            sumAijAjk = 0.0;
            sumdAijddijdxi = 0.0;
            sumdAijddijdyi = 0.0;
            sumdAijddijdzi = 0.0;
            sumdAijddijdxiAjk = 0.0;
            sumdAijddijdyiAjk = 0.0;
            sumdAijddijdziAjk = 0.0;

            icount = count;

          L70:j = ineighbor[count] - 1;
            /* mind the -1 , fortran again */
            xi = x[3 * i];
            yi = x[3 * i + 1];
            zi = x[3 * i + 2];

            xj = x[3 * j];
            yj = x[3 * j + 1];
            zj = x[3 * j + 2];

            r2 = (xi - xj) * (xi - xj) + (yi - yj) * (yi - yj) + (zi -
                                                                  zj) *
                (zi - zj);
            dij1i = 1. / sqrt(r2);
            rij = r2 * dij1i;
            tmpaij =
                P0[i] - rij * 0.5 - (P0[i] * P0[i] -
                                     P0[j] * P0[j]) * 0.5 * dij1i;
            Aij = PIx2 * P0[i] * tmpaij;

            dAijddij =
                PIx1 * P0[i] * (dij1i * dij1i *
                                (P0[i] * P0[i] - P0[j] * P0[j]) - 1.0);

            dAijddijdxj = dAijddij * (xj - xi) * dij1i;
            dAijddijdyj = dAijddij * (yj - yi) * dij1i;
            dAijddijdzj = dAijddij * (zj - zi) * dij1i;

            sumAij = sumAij + Aij;

            count2 = icount;
            sumAjk2 = 0.0;
            sumdAjkddjkdxj = 0.0;
            sumdAjkddjkdyj = 0.0;
            sumdAjkddjkdzj = 0.0;

            p3p4Aij = -surften * (P3[i] + P4[i] * Aij);

          L80:k = ineighbor[count2] - 1;
            /*same as above, -1 ! */
            if (j == k)
               goto L85;

            xk = x[3 * k];
            yk = x[3 * k + 1];
            zk = x[3 * k + 2];

            rjk2 =
                (xj - xk) * (xj - xk) + (yj - yk) * (yj - yk) + (zj -
                                                                 zk) *
                (zj - zk);

            djk1i = 1.0 / sqrt(rjk2);
            rjk = rjk2 * djk1i;

            if (P0[j] + P0[k] > rjk) {
               vdw2dif = P0[j] * P0[j] - P0[k] * P0[k];
               tmpajk = 2.0 * P0[j] - rjk - vdw2dif * djk1i;

               Ajk = PIx1 * P0[j] * tmpajk;

               sumAjk = sumAjk + Ajk;
               sumAjk2 = sumAjk2 + Ajk;

               dAjkddjk =
                   PIx1 * P0[j] * djk1i * (djk1i * djk1i * vdw2dif - 1.0);

               dAjkddjkdxj = dAjkddjk * (xj - xk);
               dAjkddjkdyj = dAjkddjk * (yj - yk);
               dAjkddjkdzj = dAjkddjk * (zj - zk);

               f[3 * k] = f[3 * k] + dAjkddjkdxj * p3p4Aij;
               f[3 * k + 1] = f[3 * k + 1] + dAjkddjkdyj * p3p4Aij;
               f[3 * k + 2] = f[3 * k + 2] + dAjkddjkdzj * p3p4Aij;

               sumdAjkddjkdxj = sumdAjkddjkdxj + dAjkddjkdxj;
               sumdAjkddjkdyj = sumdAjkddjkdyj + dAjkddjkdyj;
               sumdAjkddjkdzj = sumdAjkddjkdzj + dAjkddjkdzj;

            }

          L85:count2 = count2 + 1;
            if (ineighbor[count2] != 0) {
               goto L80;
            } else {
               count2 = icount;
            }


            sumAijAjk = sumAijAjk + Aij * sumAjk2;

            sumdAijddijdxi = sumdAijddijdxi - dAijddijdxj;
            sumdAijddijdyi = sumdAijddijdyi - dAijddijdyj;
            sumdAijddijdzi = sumdAijddijdzi - dAijddijdzj;
            sumdAijddijdxiAjk = sumdAijddijdxiAjk - dAijddijdxj * sumAjk2;
            sumdAijddijdyiAjk = sumdAijddijdyiAjk - dAijddijdyj * sumAjk2;
            sumdAijddijdziAjk = sumdAijddijdziAjk - dAijddijdzj * sumAjk2;

            lastxj = dAijddijdxj * sumAjk2 + Aij * sumdAjkddjkdxj;
            lastyj = dAijddijdyj * sumAjk2 + Aij * sumdAjkddjkdyj;
            lastzj = dAijddijdzj * sumAjk2 + Aij * sumdAjkddjkdzj;

            dAidxj = surften * (P2[i] * dAijddijdxj +
                                P3[i] * sumdAjkddjkdxj + P4[i] * lastxj);
            dAidyj =
                surften * (P2[i] * dAijddijdyj +
                           P3[i] * sumdAjkddjkdyj + P4[i] * lastyj);
            dAidzj =
                surften * (P2[i] * dAijddijdzj +
                           P3[i] * sumdAjkddjkdzj + P4[i] * lastzj);

            f[3 * j] = f[3 * j] + dAidxj;
            f[3 * j + 1] = f[3 * j + 1] + dAidyj;
            f[3 * j + 2] = f[3 * j + 2] + dAidzj;

            count = count + 1;
            if (ineighbor[count] != 0) {
               goto L70;
            } else {
               count = count + 1;
            }

            Ai = P1[i] * si + P2[i] * sumAij + P3[i] * sumAjk +
                P4[i] * sumAijAjk;

            dAidxi =
                surften * (P2[i] * sumdAijddijdxi +
                           P4[i] * sumdAijddijdxiAjk);
            dAidyi =
                surften * (P2[i] * sumdAijddijdyi +
                           P4[i] * sumdAijddijdyiAjk);
            dAidzi =
                surften * (P2[i] * sumdAijddijdzi +
                           P4[i] * sumdAijddijdziAjk);

            f[3 * i] = f[3 * i] + dAidxi;
            f[3 * i + 1] = f[3 * i + 1] + dAidyi;
            f[3 * i + 2] = f[3 * i + 2] + dAidzi;

            totsasa = totsasa + Ai;
            /*  printf("totsasa up to %d %f\n",i,totsasa); */
         }
      }
      /*      printf("SASA %f , ESURF %f \n",totsasa,totsasa*surften); */
      *esurf = totsasa * surften;

   } else {
      *esurf = 0.0;
   }

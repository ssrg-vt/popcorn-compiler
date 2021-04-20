#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "defreal.h"

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))
#include <omp.h>
#endif

#if defined(MPI) || defined(SCALAPACK)
#include "mpi.h"
#endif

/* Fundamental NAB types */

typedef int INT_T;
typedef size_t SIZE_T;

/* Necessary function definitions */

INT_T *ivector(INT_T, INT_T);
void free_ivector(INT_T*, INT_T, INT_T);
INT_T myroc(INT_T, INT_T, INT_T, INT_T);
INT_T get_mytaskid(void);
INT_T get_numtasks(void);
INT_T get_blocksize(void);

/* Here is the structure for a kd tree node. */

typedef struct kdnode {
   INT_T n;
   struct kdnode *lo, *hi;
} KDNODE_T;


/***********************************************************************
                            DOWNHEAP_PAIRS()
************************************************************************/

/*
 * The downheap function from Robert Sedgewick's "Algorithms in C++" p. 152,
 * corrected for the fact that Sedgewick indexes the heap array from 1 to n
 * whereas Java indexes the heap array from 0 to n-1. Note, however, that
 * the heap should be indexed conceptually from 1 to n in order that for
 * any node k the two children are found at nodes 2*k and 2*k+1. Move down
 * the heap, exchanging the node at position k with the larger of its two
 * children if necessary and stopping when the node at k is larger than both
 * children or the bottom of the heap is reached. Note that it is possible
 * for the node at k to have only one child: this case is treated properly.
 * A full exchange is not necessary because the variable 'v' is involved in
 * the exchanges.  The 'while' loop has two exits: one for the case that
 * the bottom of the heap is hit, and another for the case that the heap
 * condition (the parent is greater than or equal to both children) is
 * satisfied somewhere in the interior of the heap.
 *
 * Used by the heapsort_pairs function which sorts the pair list arrays.
 *
 * Calling parameters are as follows:
 *
 * a - array of indices into the atomic coordinate array x
 * n - the number of items to be sorted
 * k - the exchange node (or element)
 */

static
void downheap_pairs(int *a, int n, int k)
{

   int j, v;

   v = a[k - 1];
   while (k <= n / 2) {
      j = k + k;
      if ((j < n) && (a[j - 1] < a[j]))
         j++;
      if (v >= a[j - 1])
         break;
      a[k - 1] = a[j - 1];
      k = j;
   }
   a[k - 1] = v;
}

/***********************************************************************
                            HEAPSORT_PAIRS()
************************************************************************/

/*
 * The heapsort function from Robert Sedgewick's "Algorithms in C++" p. 156,
 * corrected for the fact that Sedgewick indexes the heap array from 1 to n
 * whereas Java indexes the heap array from 0 to n-1. Note, however, that
 * the heap should be indexed conceptually from 1 to n in order that for
 * any node k the two children are found at nodes 2*k and 2*k+1.  In what
 * follows, the 'for' loop heaporders the array in linear time and the
 * 'while' loop exchanges the largest element with the last element then
 * repairs the heap.
 *
 * Calling parameters are as follows:
 *
 * a - array of indices into the atomic coordinate array x
 * n - the number of items to be sorted
 *
 * Used by the nblist function to sort the pair list arrays.
 */

static
void heapsort_pairs(int *a, int n)
{

   int k, v;

   for (k = n / 2; k >= 1; k--)
      downheap_pairs(a, n, k);
   while (n > 1) {
      v = a[0];
      a[0] = a[n - 1];
      a[n - 1] = v;
      downheap_pairs(a, --n, 1);
   }
}

/***********************************************************************
                            DOWNHEAP_INDEX()
************************************************************************/

/*
 * The downheap function from Robert Sedgewick's "Algorithms in C++" p. 152,
 * corrected for the fact that Sedgewick indexes the heap array from 1 to n
 * whereas Java indexes the heap array from 0 to n-1. Note, however, that
 * the heap should be indexed conceptually from 1 to n in order that for
 * any node k the two children are found at nodes 2*k and 2*k+1. Move down
 * the heap, exchanging the node at position k with the larger of its two
 * children if necessary and stopping when the node at k is larger than both
 * children or the bottom of the heap is reached. Note that it is possible
 * for the node at k to have only one child: this case is treated properly.
 * A full exchange is not necessary because the variable 'v' is involved in
 * the exchanges.  The 'while' loop has two exits: one for the case that
 * the bottom of the heap is hit, and another for the case that the heap
 * condition (the parent is greater than or equal to both children) is
 * satisfied somewhere in the interior of the heap.
 *
 * Used by the heapsort_index function which sorts the index arrays indirectly
 * by comparing components of the Cartesian coordinates.
 *
 * Calling parameters are as follows:
 *
 * a - array of indices into the atomic coordinate array x
 * n - the number of items to be sorted
 * k - the exchange node (or element)
 * x - the atomic coordinate array
 * p - the partition (x, y, z or w) on which sorting occurs
 * dim - 3 or 4: dimension of the coordinate space
 */

static
void downheap_index(int *a, int n, int k, REAL_T * x, int p, int dim)
{

   int j, v;

   v = a[k - 1];
   while (k <= n / 2) {
      j = k + k;
      if ((j < n) && (x[dim * a[j - 1] + p] < x[dim * a[j] + p]))
         j++;
      if (x[dim * v + p] >= x[dim * a[j - 1] + p])
         break;
      a[k - 1] = a[j - 1];
      k = j;
   }
   a[k - 1] = v;
}

/***********************************************************************
                            HEAPSORT_INDEX()
************************************************************************/

/*
 * The heapsort function from Robert Sedgewick's "Algorithms in C++" p. 156,
 * corrected for the fact that Sedgewick indexes the heap array from 1 to n
 * whereas Java indexes the heap array from 0 to n-1. Note, however, that
 * the heap should be indexed conceptually from 1 to n in order that for
 * any node k the two children are found at nodes 2*k and 2*k+1.  In what
 * follows, the 'for' loop heaporders the array in linear time and the
 * 'while' loop exchanges the largest element with the last element then
 * repairs the heap.
 *
 * Calling parameters are as follows:
 *
 * a - array of indices into the atomic coordinate array x
 * n - the number of items to be sorted
 * x - the atomic coordinate array
 * p - the partition (x, y, z or w) on which sorting occurs
 * dim - 3 or 4: dimension of the coordinate space
 *
 * Used by the nblist function to sort the xn, yn, zn, wn and on arrays.
 */

static
void heapsort_index(int *a, int n, REAL_T * x, int p, int dim)
{

   int k, v;

   for (k = n / 2; k >= 1; k--)
      downheap_index(a, n, k, x, p, dim);
   while (n > 1) {
      v = a[0];
      a[0] = a[n - 1];
      a[n - 1] = v;
      downheap_index(a, --n, 1, x, p, dim);
   }
}

/***********************************************************************
                            BUILDKDTREE()
************************************************************************/

/*
 * Build the kd tree by recursively subdividing the atom number
 * arrays and adding nodes to the tree.  Note that the arrays
 * are permuted cyclically as control moves down the tree in
 * order that sorting occur on x, y, z and (for 4D) w.  Also,
 * if it is desired that the kd tree provide a partial atom
 * order, the sorting will occur on o, x, y, z and (for 4D) w.  The
 * temporary array is provided for the copy and partition operation.
 *
 * Calling parameters are as follows:
 *
 * xn - x sorted array of atom numbers
 * yn - y sorted array of atom numbers
 * zn - z sorted array of atom numbers
 * wn - w sorted array of atom numbers
 * on - ordinal array of atom numbers
 * tn - temporary array for atom numbers
 * start - first element of array 
 * end - last element of array
 * kdpptr - pointer to pointer to kd tree node next available for allocation
 * that - the node currently visited, the equivalent of 'this' in C++
 * x - atomic coordinate array
 * p - the partition (x, y, z, w or o) on which sorting occurs
 * dim - 3 or 4: dimension of the coordinate space
 */

#define SORT_ATOM_NUMBERS

static
void buildkdtree(int *xn, int *yn, int *zn, int *wn, int *on, int *tn,
                 int start, int end, KDNODE_T ** kdpptr, KDNODE_T * that,
                 REAL_T * x, int p, int dim)
{
   int i, middle, imedian, lower, upper;
   REAL_T median;

   /*
    * The partition cycles by dim unless SORT_ATOM_NUMBERS is defined,
    * in which case it cycles by dim+1.  Note that if SORT_ATOM_NUMBERS
    * is defined and the partition equals zero, sorting will occur
    * on the ordinal atom number instead of the atom's cartesian
    * coordinate.
    */

#ifndef SORT_ATOM_NUMBERS
   p %= dim;
#else
   p %= (dim + 1);
#endif

   /* If only one element is passed to this function, add it to the tree. */

   if (end == start) {
      that->n = xn[start];
   }

   /*
    * Otherwise, if two elements are passed to this function, determine
    * whether the first element is the low child or the high child.  Or,
    * if neither element is the low child, choose the second element to
    * be the high child.  Allocate a new KDNODE_T and make it one or the
    * other of the children.
    */

   else if (end == start + 1) {

      /* Check whether the first element is the low child. */

#ifdef SORT_ATOM_NUMBERS
      if (((p == 0) && (xn[start] < xn[end])) ||
          ((p != 0) && (x[dim * xn[start] + p - 1] <
                        x[dim * xn[end] + p - 1])))
#else
      if (x[dim * xn[start] + p] < x[dim * xn[end] + p])
#endif
      {
         that->n = xn[end];
         (*kdpptr)->n = xn[start];
         (*kdpptr)->lo = NULL;
         (*kdpptr)->hi = NULL;
         that->lo = (*kdpptr)++;
      }

      /* Check whether the second element is the low child. */

#ifdef SORT_ATOM_NUMBERS
      else if (((p == 0) && (xn[start] > xn[end])) ||
               ((p != 0) && (x[dim * xn[start] + p - 1] >
                             x[dim * xn[end] + p - 1])))
#else
      else if (x[dim * xn[start] + p] > x[dim * xn[end] + p])
#endif
      {
         that->n = xn[start];
         (*kdpptr)->n = xn[end];
         (*kdpptr)->lo = NULL;
         (*kdpptr)->hi = NULL;
         that->lo = (*kdpptr)++;
      }

      /* Neither element is the low child so use the second as the high child. */

      else {
         that->n = xn[start];
         (*kdpptr)->n = xn[end];
         (*kdpptr)->lo = NULL;
         (*kdpptr)->hi = NULL;
         that->hi = (*kdpptr)++;
      }
   }

   /* Otherwise, more than two elements are passed to this function. */

   else {

      /*
       * The middle element of the xn array is taken as the element about
       * which the yn and zn arrays will be partitioned.  However, search
       * lower elements of the xn array to ensure that the p values of the
       * atomic coordinate array that correspond to these elements are indeed
       * less than the median value because they may be equal to it.  This
       * approach is consistent with partitioning between < and >=.
       *
       * The search described above is not necessary if SORT_ATOM_NUMBERS is
       * defined and p==0 because in this case sorting occurs on the
       * ordinal atom number instead of the atomic coordinate, and the
       * ordinal atom numbers are all unique.
       */

      middle = (start + end) / 2;
#ifdef SORT_ATOM_NUMBERS
      if (p == 0) {
         imedian = xn[middle];
      } else {
         median = x[dim * xn[middle] + p - 1];
         for (i = middle - 1; i >= start; i--) {
            if (x[dim * xn[i] + p - 1] < median) {
               break;
            } else {
               middle = i;
            }
         }
      }
#else
      median = x[dim * xn[middle] + p];
      for (i = middle - 1; i >= start; i--) {
         if (x[dim * xn[i] + p] < median) {
            break;
         } else {
            middle = i;
         }
      }
#endif

      /* Store the middle element at this kd node. */

      that->n = xn[middle];

      /*
       * Scan the yn array in ascending order and compare the p value of
       * each corresponding element of the atomic coordinate array to the
       * median value.  If the p value is less than the median value, copy
       * the element of the yn array into the lower part of the tn array.
       * If the p value is greater than or equal to the median value, copy
       * the element of the yn array into the upper part of the tn array.
       * The lower part of the tn array begins with the start index, and the
       * upper part of the tn array begins one element above the middle index.
       * At the end of this scan and copy operation, the tn array will have
       * been subdivided into three groups: (1) a group of indices beginning
       * with start and continuing up to but not including middle, which indices
       * point to atoms for which the p value is less than the median value;
       * (2) the middle index that has been stored in this node of  the kd tree;
       * and (3) a group of indices beginning one address above middle and
       * continuing up to and including end, which indices point to atoms for
       * which the p value is greater than or equal to the median value.
       *
       * This approach preserves the relative heapsorted order of elements
       * of the atomic coordinate array that correspond to elements of the
       * yn array while those elements are partitioned about the p median.
       *
       * Note: when scanning the yn array, skip the element (i.e., the atom
       * number) that equals the middle element because that atom number has
       * been stored at this node of the kd-tree.
       */

      lower = start - 1;
      upper = middle;
      for (i = start; i <= end; i++) {
         if (yn[i] != xn[middle]) {

#ifdef SORT_ATOM_NUMBERS
            if (((p == 0) && (yn[i] < imedian)) ||
                ((p != 0) && (x[dim * yn[i] + p - 1] < median)))
#else
            if (x[dim * yn[i] + p] < median)
#endif
            {
               tn[++lower] = yn[i];
            } else {
               tn[++upper] = yn[i];
            }
         }
      }

      /*
       * All elements of the yn array between start and end have been copied
       * and partitioned into the tn array, so the yn array is available for
       * elements of the zn array to be copied and partitioned into the yn
       * array, in the same manner in which elements of the yn array were
       * copied and partitioned into the tn array.
       *
       * This approach preserves the relative heapsorted order of elements
       * of the atomic coordinate array that correspond to elements of the
       * zn array while those elements are partitioned about the p median.
       *
       * Note: when scanning the zn array, skip the element (i.e., the atom
       * number) that equals the middle element because that atom number has
       * been stored at this node of the kd-tree.
       */

      lower = start - 1;
      upper = middle;
      for (i = start; i <= end; i++) {
         if (zn[i] != xn[middle]) {

#ifdef SORT_ATOM_NUMBERS
            if (((p == 0) && (zn[i] < imedian)) ||
                ((p != 0) && (x[dim * zn[i] + p - 1] < median)))
#else
            if (x[dim * zn[i] + p] < median)
#endif
            {
               yn[++lower] = zn[i];
            } else {
               yn[++upper] = zn[i];
            }
         }
      }

      /*
       * Execute the following region of code if SORT_ATOM_NUMBERS is defined,
       * or if SORT_ATOM_NUMBERS is not defined and dim==4.
       */

#ifndef SORT_ATOM_NUMBERS
      if (dim == 4)
#endif

      {

         /*
          * All elements of the zn array between start and end have been copied
          * and partitioned into the yn array, so the zn array is available for
          * elements of the wn array to be copied and partitioned into the zn
          * array, in the same manner in which elements of the zn array were
          * copied and partitioned into the yn array.
          *
          * This approach preserves the relative heapsorted order of elements
          * of the atomic coordinate array that correspond to elements of the
          * wn array while those elements are partitioned about the p median.
          *
          * Note: when scanning the wn array, skip the element (i.e., the atom
          * number) that equals the middle element because that atom number has
          * been stored at this node of the kd-tree.
          */

         lower = start - 1;
         upper = middle;
         for (i = start; i <= end; i++) {
            if (wn[i] != xn[middle]) {

#ifdef SORT_ATOM_NUMBERS
               if (((p == 0) && (wn[i] < imedian)) ||
                   ((p != 0) && (x[dim * wn[i] + p - 1] < median)))
#else
               if (x[dim * wn[i] + p] < median)
#endif
               {
                  zn[++lower] = wn[i];
               } else {
                  zn[++upper] = wn[i];
               }
            }
         }
      }

      /*
       * Execute the following region of code if SORT_ATOM_NUMBERS is defined
       * and dim==4.
       */

#ifdef SORT_ATOM_NUMBERS

      if (dim == 4) {

         /*
          * All elements of the wn array between start and end have been copied
          * and partitioned into the zn array, so the wn array is available for
          * elements of the on array to be copied and partitioned into the wn
          * array, in the same manner in which elements of the wn array were
          * copied and partitioned into the zn array.
          *
          * This approach preserves the relative heapsorted order of elements
          * of the atomic coordinate array that correspond to elements of the
          * wn array while those elements are partitioned about the p median.
          *
          * Note: when scanning the on array, skip the element (i.e., the atom
          * number) that equals the middle element because that atom number has
          * been stored at this node of the kd-tree.
          */

         lower = start - 1;
         upper = middle;
         for (i = start; i <= end; i++) {
            if (on[i] != xn[middle]) {
               if (((p == 0) && (on[i] < imedian)) ||
                   ((p != 0) && (x[dim * on[i] + p - 1] < median))) {
                  wn[++lower] = on[i];
               } else {
                  wn[++upper] = on[i];
               }
            }
         }
      }
#endif

      /*
       * Recurse down the lo branch of the tree if the lower group of
       * the tn array is non-null.  Note permutation of the xn, yn, zn, wn,
       * on and tn arrays.  In particular, xn was used for partitioning at
       * this level of the tree.  At one level down the tree, yn (which
       * has been copied into tn) will be used for partitioning.  At two
       * levels down the tree, zn (which has been copied into yn) will
       * be used for partitioning.  If SORT_ATOM_NUMBERS is defined, or if
       * SORT_ATOM_NUMBERS is not defined and dim==4, at three levels down the
       * tree, wn (which has been copied into zn) will be used for partitoning.
       * At four levels down the tree, xn will be used for partitioning.
       * In this manner, partitioning cycles through xn, yn, zn and wn
       * at successive levels of the tree.
       *
       * Note that for 3D the wn array isn't allocated so don't permute it
       * cyclically along with the other arrays in the recursive call.
       *
       * Note also that if SORT_ATOM_NUMBERS isn't defined the on array isn't
       * allocated so don't permute it cyclically in the recursive call.
       */

      if (lower >= start) {
         (*kdpptr)->lo = NULL;
         (*kdpptr)->hi = NULL;
         that->lo = (*kdpptr)++;

#ifndef SORT_ATOM_NUMBERS
         if (dim == 4) {
            buildkdtree(tn, yn, zn, xn, on, wn,
                        start, lower, kdpptr, that->lo, x, p + 1, dim);
         } else {
            buildkdtree(tn, yn, xn, wn, on, zn,
                        start, lower, kdpptr, that->lo, x, p + 1, dim);
         }
#else
         if (dim == 4) {
            buildkdtree(tn, yn, zn, wn, xn, on,
                        start, lower, kdpptr, that->lo, x, p + 1, dim);
         } else {
            buildkdtree(tn, yn, zn, xn, on, wn,
                        start, lower, kdpptr, that->lo, x, p + 1, dim);
         }
#endif

      }

      /*
       * Recurse down the hi branch of the tree if the upper group of
       * the tn array is non-null.  Note permutation of the xn, yn, zn, wn
       * and tn arrays, as explained above for recursion down the lo
       * branch of the tree.
       *
       * Note that for 3D the wn array isn't allocated so don't permute it
       * cyclically along with the other arrays in the recursive call.
       *
       * Note also that if SORT_ATOM_NUMBERS isn't defined the on array isn't
       * allocated so don't permute it cyclically in the recursive call.
       */

      if (upper > middle) {
         (*kdpptr)->lo = NULL;
         (*kdpptr)->hi = NULL;
         that->hi = (*kdpptr)++;

#ifndef SORT_ATOM_NUMBERS
         if (dim == 4) {
            buildkdtree(tn, yn, zn, xn, on, wn,
                        middle + 1, end, kdpptr, that->hi, x, p + 1, dim);
         } else {
            buildkdtree(tn, yn, xn, wn, on, zn,
                        middle + 1, end, kdpptr, that->hi, x, p + 1, dim);
         }
#else
         if (dim == 4) {
            buildkdtree(tn, yn, zn, wn, xn, on,
                        middle + 1, end, kdpptr, that->hi, x, p + 1, dim);
         } else {
            buildkdtree(tn, yn, zn, xn, on, wn,
                        middle + 1, end, kdpptr, that->hi, x, p + 1, dim);
         }
#endif

      }
   }
}

/***********************************************************************
                            SEARCHKDTREE()
************************************************************************/

/*
 * Walk the kd tree and generate the pair lists for the upper and lower
 * triangles.  The pair lists are partially ordered in descending atom
 * number if SORT_ATOM_NUMBERS is defined.  Descending order is preferred
 * by the subsequent heap sort of the pair lists that will occur if
 * HEAP_SORT_PAIRS is defined.
 *
 * Calling parameters are as follows:
 *
 * that - the node currently visited, equivalent to 'this' in C++
 * x - atomic coordinate array
 * p - the partition (x, y, z, w or o) on which sorting occurs
 * q - the query atom number
 * loindexp - pointer to pair count array index for the lower triangle
 * upindexp - pointer to pair count array index for the upper triangle
 * lopairlist - the pair list for the lower triangle
 * uppairlist - the pair list for the upper triangle
 * cut - the cutoff distance
 * cut2 - the cutoff distance
 */

static
void searchkdtree(KDNODE_T * that, REAL_T * x, INT_T p, INT_T q,
                  INT_T * loindexp, INT_T * upindexp,
                  INT_T * lopairlist, INT_T * uppairlist,
                  REAL_T cut, REAL_T cut2, int dim, int *frozen)
{
   REAL_T xij, yij, zij, wij, r2;

   /*
    * The partition cycles by dim unless SORT_ATOM_NUMBERS is defined,
    * in which case it cycles by dim+1.  Note that if SORT_ATOM_NUMBERS
    * is defined and the partition equals zero, sorting has occured
    * on the ordinal atom number instead of the atom's cartesian
    * coordinate.
    */

#ifndef SORT_ATOM_NUMBERS
   p %= dim;
#else
   p %= (dim + 1);
#endif

   /*
    * Search the high branch of the tree if the atomic coordinate of the
    * query atom plus the cutoff radius is greater than or equal to the
    * atomic coordinate of the kd node atom.
    *
    * If SORT_ATOM_NUMBERS is defined and p==0, always search the high branch.
    */

#ifdef SORT_ATOM_NUMBERS
   if (((p == 0) && (that->hi != NULL)) ||
       ((p != 0) && (that->hi != NULL) &&
        (x[dim * q + p - 1] + cut >= x[dim * that->n + p - 1])))
#else
   if ((that->hi != NULL) &&
       (x[dim * q + p] + cut >= x[dim * that->n + p]))
#endif
   {
      searchkdtree(that->hi, x, p + 1, q, loindexp, upindexp,
                   lopairlist, uppairlist, cut, cut2, dim, frozen);
   }

   /*
    * If the query atom number does not equal the kd tree node atom number
    * and at least one of the two atoms is not frozen, calculate the interatomic
    * distance and add the kd tree node atom to one of the pair lists if the
    * distance is less than the cutoff distance.  The atom belongs on the lower
    * triangle pair list if the atom number is less than the query node atom
    * number.  Otherwise, it belongs on the upper triangle pair list.
    */

   if ((q != that->n) && (!frozen[q] || !frozen[that->n])) {
      xij = x[dim * q + 0] - x[dim * that->n + 0];
      yij = x[dim * q + 1] - x[dim * that->n + 1];
      zij = x[dim * q + 2] - x[dim * that->n + 2];
      r2 = xij * xij + yij * yij + zij * zij;
      if (dim == 4) {
         wij = x[dim * q + 3] - x[dim * that->n + 3];
         r2 += wij * wij;
      }
      if (r2 < cut2) {
         if (that->n < q) {
            lopairlist[*loindexp] = that->n;
            (*loindexp)++;
         } else {
            uppairlist[*upindexp] = that->n;
            (*upindexp)++;
         }
      }
   }

   /*
    * Search the low branch of the tree if the atomic coordinate of the
    * query atom minus the cutoff radius is less than the atomic coordinate
    * of the kd node atom.
    *
    * If SORT_ATOM_NUMBERS is defined and p==0, always search the low branch.
    */

#ifdef SORT_ATOM_NUMBERS
   if (((p == 0) && (that->lo != NULL)) ||
       ((p != 0) && (that->lo != NULL) &&
        (x[dim * q + p - 1] - cut < x[dim * that->n + p - 1])))
#else
   if ((that->lo != NULL) && (x[dim * q + p] - cut < x[dim * that->n + p]))
#endif
   {
      searchkdtree(that->lo, x, p + 1, q, loindexp, upindexp,
                   lopairlist, uppairlist, cut, cut2, dim, frozen);
   }
}

/***********************************************************************
                            NBLIST()
************************************************************************/

/*
 * Create the non-bonded and non-polar pairlists using a kd-tree.
 * The kd-tree nodes are allocated from the kdtree array.
 *
 * Calling parameters are as follows:
 *
 * lpears - the number of pairs on the lower triangle pair list
 * upears - the number of pairs on the upper triangle pair list
 * pearlist - the pair list, contiguous for the upper and lower triangles
 * x - atomic coordinate array
 * context_PxQ - the ScaLAPACK context
 * derivs - the derivative flag: -1 for 2nd derivs, 1 for 1st derivs
 * cutoff - the cutoff radius
 * natom - number of atoms
 * dim - 3 or 4: dimension of the coordinate space
 * frozen[] - list of frozen atoms
 *
 * This function returns the total number of pairs.
 */

INT_T nblist(INT_T * lpears, INT_T * upears, INT_T ** pearlist, REAL_T * x,
             INT_T context_PxQ, INT_T derivs, REAL_T cutoff, int natom,
             int dim, int *frozen)
{
   int i, j, locnt, upcnt, totpair, numthreads, threadnum, blocksize;
   int *xn, *yn, *zn, *wn, *on, *tn, *lopairlist, *uppairlist;
   REAL_T cutoff2;
   KDNODE_T *kdtree, *kdptr, *root;

#ifdef SCALAPACK
   int myrow, mycol, nprow, npcol, lotot, uptot;
   int *lopearlist, *uppearlist, *divblk, *modrow;
#endif

   /* Square the cutoff distances for use in searchkdtree. */

   cutoff2 = cutoff * cutoff;

   /* Get the block size. */

   blocksize = get_blocksize();

   /* Allocate the kdtree array that must hold one node per atom. */

   if ((kdtree = (KDNODE_T *) malloc(natom * sizeof(KDNODE_T))) == NULL) {
      fprintf(stderr, "Error allocate kdnode array in nbtree!\n");
      exit(1);
   }

   /*
    * Allocate, initialize and sort the arrays that hold the results of the
    * heapsort on x,y,z.  These arrays are used as pointers (via array indices)
    * into the atomic coordinate array x.  Allocate an additional temp array
    * so that the buildkdtree function can cycle through x,y,z.  Also allocate
    * and sort an additional array for the w coordinate if dim==4, and
    * allocate an array for the ordinal atom number if SORT_ATOM_NUMBERS is
    * defined.
    *
    * The temp array and the ordinal atom array are not sorted.
    */

   xn = ivector(0, natom);
   yn = ivector(0, natom);
   zn = ivector(0, natom);
   tn = ivector(0, natom);

   if (dim == 4) {
      wn = ivector(0, natom);
   }
#ifdef SORT_ATOM_NUMBERS
   on = ivector(0, natom);
#endif

   for (i = 0; i < natom; i++) {
      xn[i] = yn[i] = zn[i] = i;
      if (dim == 4) {
         wn[i] = i;
      }
#ifdef SORT_ATOM_NUMBERS
      on[i] = i;
#endif

   }

   heapsort_index(xn, natom, x, 0, dim);
   heapsort_index(yn, natom, x, 1, dim);
   heapsort_index(zn, natom, x, 2, dim);

   if (dim == 4) {
      heapsort_index(wn, natom, x, 3, dim);
   }

   /*
    * Build the kd tree.  For 3D the wn array is ignored because it wasn't
    * allocated.  When SORT_ATOM_NUMBERS is not defined the on array is
    * ignored because it wasn't allocated either.  See the recursive calls
    * to the buildkdtree function from within that function to verify that
    * arrays that are ignored do not participate in the cyclic permutation
    * of arrays in the recursive calls.
    *
    * But if SORT_ATOM_NUMBERS is defined the xn, yn, zn, wn and on array order
    * is permuted in the non-recursive call to the buildkdtree function
    * (below) so that the sort at the root node of the tree occurs on the
    * ordinal atom number.
    */

   kdptr = kdtree;
   root = kdptr++;
   root->lo = NULL;
   root->hi = NULL;

#ifndef SORT_ATOM_NUMBERS

   buildkdtree(xn, yn, zn, wn, on, tn, 0, natom - 1, &kdptr, root, x, 0,
               dim);

#else

   buildkdtree(on, xn, yn, zn, wn, tn, 0, natom - 1, &kdptr, root, x, 0,
               dim);

#endif

   /*
    * Search the kd tree with each atom and record pairs into temporary
    * arrays for the lower and upper triangle pair lists for one atom.
    * Copy the temporary pair lists into a pair list array that is
    * allocated separately for each atom and that contains the lower
    * and upper triangle pair lists contiguously packed.
    *
    * The pairlist array is an array of pair list arrays.
    */

   totpair = 0;

#if defined(SPEC_OMP) || defined(OPENMP)
#pragma omp parallel reduction (+: totpair) \
  private (i, j, locnt, upcnt, lopairlist, uppairlist, threadnum, numthreads)
#endif

   {
      /*
       * Get the thread number and the number of threads for multi-threaded
       * execution under OpenMP, ScaLAPACK or MPI.  These variables are not
       * used for single-threaded execution.
       *
       * If MPI is defined, the manner of assignment of the threadnum
       * and numthreads variables depends upon derivs, as follows.
       *
       * If derivs >= 0, the call to nblist is intended to build
       * a pair list for the first derivative calculation.  Therefore,
       * the threadnum and numthreads variables are assigned in
       * a row cyclic manner that is required for the first derivative
       * calculation.  This row cyclic approach makes optimal use of
       * the MPI tasks in that each task has the minimum number of
       * pair lists for the first derivative calculation.
       *
       * If derivs < 0 the call to nblist is intended to build a pair
       * list for the Born second derivative calculation.  However,
       * the Born second derivative calculation is not parallelized
       * for MPI.  Therefore, the pair list will be fully populated
       * for each MPI task.
       *
       * If OPENMP is defined, the threadnum and numthreads variables
       * are assigned in a row cyclic manner for both the first and
       * second derivative calculations.  Thus for both calculations
       * each OpenMP thread has the minimum number of pair lists.
       *
       * If SCALAPACK is defined, the manner of assignment of the threadnum
       * and numthreads variables depends upon derivs variable, as follows.
       *
       * If derivs > 0, the call to nblist is intended to build
       * a pair list for the first derivative calculation.  Therefore,
       * the threadnum and numthreads variables are assigned in
       * a row cyclic manner that is required for the first derivative
       * calculation.  As in the MPI case, each MPI task has the minimum
       * number of pair list for the first derivative calculation.
       *
       * If derivs < 0, the call to nblist is intended to build a pair
       * list either for the Born second derivative calculation or for
       * the nonbonded second derivative calculation.  For the nonbonded
       * case, the calculation is not parallelized; therefore, the pair
       * list will be fully populated for each ScaLAPACK process.
       *
       * For the Born case, the threadnum and numthreads variables are
       * assigned from the process column and the number of process columns,
       * respectively.  Each row of a particular column receives the same
       * pair list from the search of the kd tree, but thereafter the
       * pair list is culled using the process row and number of process
       * rows so that each row ultimately receives a unique pair list.
       * Thus, although for the Born second derivative calculation each process
       * column receives more pair lists than does each MPI task for the first
       * derivative calculation, the pair lists are shorter, and therefore the
       * total number of pairs that are processed by each process column is the
       * same as in the first derivative calculation, with the following.
       * caveat.  For processes that do not lie of the process grid, the
       * process column is -1 which will result in no pair list for that process
       * due to the fact that the myroc function returns 0.  Hence for the
       * second derivative calculation, fewer processes have pair lists than
       * for the first derivative calculation unless the total number of
       * MPI tasks is a square such as 1, 4, 9, et cetera.
       *
       */

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP))

      threadnum = omp_get_thread_num();
      numthreads = omp_get_num_threads();

#elif defined(SCALAPACK)

      if (derivs < 0) {
         blacs_gridinfo_(&context_PxQ, &nprow, &npcol, &myrow, &mycol);
         threadnum = mycol;
         numthreads = npcol;
      } else if (derivs > 0) {
         threadnum = get_mytaskid();
         numthreads = get_numtasks();
      } else {
         threadnum = 0;
         numthreads = 1;
      }

#elif defined(MPI)

      if (derivs <= 0) {
         threadnum = 0;
         numthreads = 1;
      } else {
         threadnum = get_mytaskid();
         numthreads = get_numtasks();
      }

#endif

      /*
       * Allocate the temporary arrays for the lower and upper triangles.
       * These arrays must be large enough to hold a maximum size pair
       * list for one atom.  For the ScaLAPACK second derivatives an
       * extra set of temporary arrays is used from which the final pair
       * lists are culled.
       *
       * Also, for ScaLAPACK allocate and initialize lookup tables
       * for division and modulus operations.
       */

      lopairlist = ivector(0, natom);
      uppairlist = ivector(0, natom);

#ifdef SCALAPACK

      if (derivs < 0 && gb) {
         lopearlist = ivector(0, natom);
         uppearlist = ivector(0, natom);

         divblk = ivector(0, natom);
         modrow = ivector(0, natom);

         for (i = 0; i < natom; i++) {
            divblk[i] = i / blocksize;
            modrow[i] = i % nprow;
         }
      }
#endif

      /*
       * Search the kd tree with each atom.  If no pair list array
       * has been allocated and there are pair atoms, allocate a
       * pair list array.  If a pair list array has been allocated
       * but it is too small for the number of pair atoms, deallocate
       * the pair list array and allocate a larger array.  If it is
       * at least 33% larger than is necessary for the pair atoms,
       * deallocate it and allocate a smaller pair list array.  Copy the
       * lower and upper triangle pair lists into the pair list array,
       * packed contiguously with the lower triangle pair list followed
       * by the upper triangle pair list.
       *
       * Explicitly assign threads to loop indices for the following loop,
       * in a manner equivalent to (static, N) scheduling with OpenMP,
       * and identical to the manner in which threads are assigned in
       * nbond, egb and egb2.
       *
       * There is an implied barrier end of this OpenMP parallel region.
       * Because the following loop is the only loop in this parallel
       * region, there is no need for an explicit barrier.  Furthermore,
       * all OpenMP parallelized loops that use the pair list also use
       * loop index to thread mapping that is identical to what is used for
       * this loop.  Hence, no race condition would exist even if OpenMP
       * threads were not synchronized at the end of this parallel region
       * because each thread constructs the pair list that it uses thereafter.
       * This same argument applies to MPI tasks: no synchronization is
       * necessary.
       */

      for (i = 0; i < natom; i++) {

#if defined(SPEC_OMP) || (!defined(SPEC) && defined(OPENMP)) || defined(MPI) || defined(SCALAPACK)

         if (!myroc(i, blocksize, numthreads, threadnum))
            continue;

#endif

#ifdef SCALAPACK

         if (derivs < 0 && gb) {
            lotot = uptot = 0;
            searchkdtree(root, x, 0, i, &lotot, &uptot,
                         lopearlist, uppearlist, cutoff, cutoff2, dim,
                         frozen);
         } else {
            locnt = upcnt = 0;
            searchkdtree(root, x, 0, i, &locnt, &upcnt,
                         lopairlist, uppairlist, cutoff, cutoff2, dim,
                         frozen);
         }

#else

         locnt = upcnt = 0;
         searchkdtree(root, x, 0, i, &locnt, &upcnt,
                      lopairlist, uppairlist, cutoff, cutoff2, dim,
                      frozen);

#endif

         /*
          * If SORT_ATOM_NUMBERS is defined, the upper and lower triangle
          * pair lists are partially sorted by ordinal atom number using
          * the kd tree.
          *
          * If HEAP_SORT_PAIRS is defined, sort the upper and lower triangle
          * pair lists using heap sort.
          *
          * If the pair lists are sorted by ordinal atom number using the
          * kd tree, the subsequent heap sort of the pair lists is quicker,
          * but the kd tree sort is not necessary.
          */

#define HEAP_SORT_PAIRS
#ifdef HEAP_SORT_PAIRS

#ifdef SCALAPACK

         if (derivs < 0 && gb) {
            heapsort_pairs(lopearlist, lotot);
            heapsort_pairs(uppearlist, uptot);
         } else {
            heapsort_pairs(lopairlist, locnt);
            heapsort_pairs(uppairlist, upcnt);
         }

#else

         heapsort_pairs(lopairlist, locnt);
         heapsort_pairs(uppairlist, upcnt);

#endif

#endif

         /*
          * For the ScaLAPACK second derivatives cull the pair lists
          * by copying to the final pair lists only those atoms that
          * are active in a particular process row.  Use the lookup
          * tables for a faster form of calls to myroc of the form:
          *
          *       myroc(lopearlist[j], blocksize, npcol, mycol)
          */

#ifdef SCALAPACK

         if (derivs < 0 && gb) {
            locnt = 0;
            for (j = 0; j < lotot; j++) {
               if (myrow >= 0 && modrow[divblk[lopearlist[j]]] == myrow) {
                  lopairlist[locnt++] = lopearlist[j];
               }
            }
            upcnt = 0;
            for (j = 0; j < uptot; j++) {
               if (myrow >= 0 && modrow[divblk[uppearlist[j]]] == myrow) {
                  uppairlist[upcnt++] = uppearlist[j];
               }
            }
         }
#endif

         if ((pearlist[i] == NULL) && (locnt + upcnt > 0)) {
            pearlist[i] = ivector(0, locnt + upcnt);
         } else if ((pearlist[i] != NULL) &&
                    ((locnt + upcnt > lpears[i] + upears[i]) ||
                     (4 * (locnt + upcnt) <
                      3 * (lpears[i] + upears[i])))) {
            free_ivector(pearlist[i], 0, lpears[i] + upears[i]);
            pearlist[i] = ivector(0, locnt + upcnt);
         }
         lpears[i] = locnt;
         upears[i] = upcnt;
         for (j = 0; j < locnt; j++) {
            pearlist[i][j] = lopairlist[j];
         }
         for (j = 0; j < upcnt; j++) {
            pearlist[i][locnt + j] = uppairlist[j];
         }
         totpair += locnt + upcnt;
      }

      /*
       * Deallocate the temporary arrays for the lower and upper triangles.
       * For ScaLAPACK deallocate the addtional temporary arrays as well as
       * the lookup tables.
       */

      free_ivector(lopairlist, 0, natom);
      free_ivector(uppairlist, 0, natom);

#ifdef SCALAPACK

      if (derivs < 0 && gb) {
         free_ivector(lopearlist, 0, natom);
         free_ivector(uppearlist, 0, natom);

         free_ivector(divblk, 0, natom);
         free_ivector(modrow, 0, natom);
      }
#endif

   }

   /* Free the temporary arrays. */

   free(kdtree);
   free_ivector(xn, 0, natom);
   free_ivector(yn, 0, natom);
   free_ivector(zn, 0, natom);
   free_ivector(tn, 0, natom);

   if (dim == 4) {
      free_ivector(wn, 0, natom);
   }
#ifdef SORT_ATOM_NUMBERS

   free_ivector(on, 0, natom);

#endif

   return totpair;
}

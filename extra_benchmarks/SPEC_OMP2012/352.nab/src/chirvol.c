#include <stdio.h>
#include <math.h>
#include "nab.h"
/*
 * for incoming atoms i0,i1,i2,i3, coordinates in array pos:
 *   compute signed volume and derivatives (into vol and d).
 */

void	chirvol( int dim, int i0, int i1, int i2, int i3,
	REAL_T pos[], REAL_T d[], REAL_T *vol )
/* int	dim;      dimension of incoming positon array  */
{
	REAL_T	x0, y0, z0;
	REAL_T	x1, y1, z1;
	REAL_T	x2, y2, z2;
	REAL_T	x3, y3, z3;
	REAL_T	a1,a2,a3, b1,b2,b3, c1,c2,c3;
	REAL_T	gq1, gq2, gq3;
	int		i;

	x0 = pos[ dim*i0+0 ];
	y0 = pos[ dim*i0+1 ];
	z0 = pos[ dim*i0+2 ];
	x1 = pos[ dim*i1+0 ];
	y1 = pos[ dim*i1+1 ];
	z1 = pos[ dim*i1+2 ];
	x2 = pos[ dim*i2+0 ];
	y2 = pos[ dim*i2+1 ];
	z2 = pos[ dim*i2+2 ];
	x3 = pos[ dim*i3+0 ];
	y3 = pos[ dim*i3+1 ];
	z3 = pos[ dim*i3+2 ];

	a1 = x1 - x0; a2 = y1 - y0; a3 = z1 - z0;
	b1 = x2 - x0; b2 = y2 - y0; b3 = z2 - z0;
	c1 = x3 - x0; c2 = y3 - y0; c3 = z3 - z0;

	gq1 = b2*c3 - b3*c2;
	gq2 = b3*c1 - b1*c3;
	gq3 = b1*c2 - b2*c1;
	*vol = a1*gq1 + a2*gq2 + a3*gq3;

	d[3] =  gq1; d[4] =  gq2; d[5] =  gq3;
	d[0] = -gq1; d[1] = -gq2; d[2] = -gq3;

	gq1 = c2*a3 - c3*a2;
	gq2 = c3*a1 - c1*a3;
	gq3 = c1*a2 - c2*a1;

	d[6] =  gq1; d[7] =  gq2; d[8] =  gq3;
	d[0] += -gq1; d[1] += -gq2; d[2] += -gq3;

	gq1 = a2*b3 - a3*b2;
	gq2 = a3*b1 - a1*b3;
	gq3 = a1*b2 - a2*b1;

	d[9] =  gq1; d[10] =  gq2; d[11] =  gq3;
	d[0] += -gq1; d[1] += -gq2; d[2] += -gq3;

#define SIXTH 0.1666666667

	*vol *= SIXTH;
	for( i=0; i<12; i++ ) d[i] *= SIXTH;

}

/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  pf3.c - routines for gaussian and box filtering
 *
 *     8/13/86
 */

#include  <stdio.h>

#include  <math.h>

#include  "color.h"

#define	 FTINY		1e-7

extern double  rad;		/* output pixel radius for filtering */

extern int  nrows;		/* number of rows for output */
extern int  ncols;		/* number of columns for output */

extern int  boxfilt;		/* do box filtering? */

extern int  xres, yres;		/* resolution of input */

extern double  x_c, y_r;	/* conversion factors */

extern int  xrad;		/* x window size */
extern int  yrad;		/* y window size */

extern int  barsize;		/* size of input scan bar */
extern COLOR  **scanin;		/* input scan bar */
extern COLOR  *scanout;		/* output scan line */

extern char  *progname;

float  *gausstable;		/* gauss lookup table */

#define	 lookgauss(x)		gausstable[(int)(10.*(x)+.5)]


initmask()			/* initialize gaussian lookup table */
{
	extern char  *malloc();
	double  d;
	register int  x;

	gausstable = (float *)malloc(100*sizeof(float));
	if (gausstable == NULL) {
		fprintf(stderr, "%s: out of memory in initmask\n", progname);
		quit(1);
	}
	d = x_c*y_r*0.25/rad/rad;
	gausstable[0] = exp(-d)/sqrt(d);
	for (x = 1; x < 100; x++)
		if ((gausstable[x] = exp(-x*0.1)/sqrt(x*0.1)) > gausstable[0])
			gausstable[x] = gausstable[0];
}


dobox(csum, xcent, ycent, c, r)			/* simple box filter */
COLOR  csum;
int  xcent, ycent;
int  c, r;
{
	int  wsum;
	double  d;
	int  y;
	register int  x;
	register COLOR	*scan;

	wsum = 0;
	setcolor(csum, 0.0, 0.0, 0.0);
	for (y = ycent+1-yrad; y <= ycent+yrad; y++) {
		if (y < 0) continue;
		if (y >= yres) break;
		d = y_r < 1.0 ? y_r*y - r : (double)(y - ycent);
		if (d > 0.5+FTINY || d < -0.5-FTINY)
			continue;
		scan = scanin[y%barsize];
		for (x = xcent+1-xrad; x <= xcent+xrad; x++) {
			if (x < 0) continue;
			if (x >= xres) break;
			d = x_c < 1.0 ? x_c*x - c : (double)(x - xcent);
			if (d > 0.5+FTINY || d < -0.5-FTINY)
				continue;
			wsum++;
			addcolor(csum, scan[x]);
		}
	}
	if (wsum > 1)
		scalecolor(csum, 1.0/wsum);
}


dogauss(csum, xcent, ycent, c, r)		/* gaussian filter */
COLOR  csum;
int  xcent, ycent;
int  c, r;
{
	double  dy, dx, weight, wsum;
	COLOR  ctmp;
	int  y;
	register int  x;
	register COLOR	*scan;

	wsum = FTINY;
	setcolor(csum, 0.0, 0.0, 0.0);
	for (y = ycent-yrad; y <= ycent+yrad; y++) {
		if (y < 0) continue;
		if (y >= yres) break;
		dy = (y_r*(y+.5) - (r+.5))/rad;
		scan = scanin[y%barsize];
		for (x = xcent-xrad; x <= xcent+xrad; x++) {
			if (x < 0) continue;
			if (x >= xres) break;
			dx = (x_c*(x+.5) - (c+.5))/rad;
			weight = lookgauss(dx*dx + dy*dy);
			wsum += weight;
			copycolor(ctmp, scan[x]);
			scalecolor(ctmp, weight);
			addcolor(csum, ctmp);
		}
	}
	scalecolor(csum, 1.0/wsum);
}

/* Copyright (c) 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Convert IES luminaire data to Radiance description
 *
 *	07Apr90		Greg Ward
 */

#include <stdio.h>
#include <ctype.h>
#include "color.h"

#define PI		3.14159265358979323846
					/* floating comparisons */
#define FTINY		1e-6
#define FEQ(a,b)	((a)<=(b)+FTINY&&(a)>=(b)-FTINY)
					/* tilt specs */
#define TLTSTR		"TILT="
#define TLTSTRLEN	5
#define TLTNONE		"NONE"
#define TLTINCL		"INCLUDE"
#define TLT_VERT	1
#define TLT_H0		2
#define TLT_H90		3
					/* photometric types */
#define PM_C		1
#define PM_B		2
					/* unit types */
#define U_FEET		1
#define U_METERS	2
					/* string lengths */
#define MAXLINE		132
#define MAXWORD		76
#define MAXPATH		128
					/* file types */
#define T_RAD		".rad"
#define T_DST		".dat"
#define T_TLT		"+.dat"
					/* shape types */
#define RECT		1
#define DISK		2
#define SPHERE		3

#define MINDIM		.001		/* minimum dimension (point source) */

#define F_M		.3048		/* feet to meters */

#define abspath(p)	((p)[0] == '/' || (p)[0] == '.')

static char	default_name[] = "default";

char	*libdir = NULL;			/* library directory location */
char	*prefdir = NULL;		/* subdirectory */
char	*lampdat = "lamp.tab";		/* lamp data file */

double	meters2out = 1.0;		/* conversion from meters to output */
char	*lamptype = NULL;		/* selected lamp type */
char	*deflamp = NULL;		/* default lamp type */
float	defcolor[3] = {1.,1.,1.};	/* default lamp color */
float	*lampcolor = defcolor;		/* pointer to current lamp color */
double	multiplier = 1.0;		/* multiplier for all light sources */
char	units[64] = "meters";		/* output units */
double	minaspect = 0.0;		/* minimum allowed aspect ratio */
int	maxemitters = 1;		/* maximum emitters per hemisphere */
double	illumrad = 0.0;			/* radius for illum sphere */

typedef struct {
	int	type;				/* RECT, DISK, SPHERE */
	double	w, l, h;			/* width, length, height */
	double	area;				/* effective radiating area */
} SHAPE;				/* a source shape */

int	gargc;				/* global argc (minus filenames) */
char	**gargv;			/* global argv */

extern char	*strcpy(), *strcat(), *stradd(), *tailtrunc(), *filetrunc(),
		*filename(), *libname(), *fullname(), *malloc();
extern double	atof();
extern float	*matchlamp();


main(argc, argv)
int	argc;
char	*argv[];
{
	char	*outfile = NULL;
	int	status;
	char	outname[MAXWORD];
	double	d1;
	int	i;
	
	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 'd':		/* dimensions */
			if (argv[i][2] == '\0')
				goto badopt;
			if (argv[i][3] == '\0')
				d1 = 1.0;
			else if (argv[i][3] == '/') {
				d1 = atof(argv[i]+4);
				if (d1 <= FTINY)
					goto badopt;
			} else
				goto badopt;
			switch (argv[i][2]) {
			case 'c':		/* centimeters */
				if (FEQ(d1,10.))
					strcpy(units,"millimeters");
				else {
					strcpy(units,"centimeters");
					strcat(units,argv[i]+3);
				}
				meters2out = 100.*d1;
				break;
			case 'm':		/* meters */
				if (FEQ(d1,1000.))
					strcpy(units,"millimeters");
				else if (FEQ(d1,100.))
					strcpy(units,"centimeters");
				else {
					strcpy(units,"meters");
					strcat(units,argv[i]+3);
				}
				meters2out = d1;
				break;
			case 'i':		/* inches */
				strcpy(units,"inches");
				strcat(units,argv[i]+3);
				meters2out = d1*(12./F_M);
				break;
			case 'f':		/* feet */
				if (FEQ(d1,12.))
					strcpy(units,"inches");
				else {
					strcpy(units,"feet");
					strcat(units,argv[i]+3);
				}
				meters2out = d1/F_M;
				break;
			default:
				goto badopt;
			}
			break;
		case 'l':		/* library directory */
			libdir = argv[++i];
			break;
		case 'p':		/* prefix subdirectory */
			prefdir = argv[++i];
			break;
		case 'f':		/* lamp data file */
			lampdat = argv[++i];
			break;
		case 'o':		/* output file name */
			outfile = argv[++i];
			break;
		case 's':		/* square emitters */
			minaspect = .6;
			if (argv[i][2] == '/') {
				maxemitters = atoi(argv[i]+3);
				if (maxemitters < 1)
					goto badopt;
			}
			break;
		case 'i':		/* illum */
			illumrad = atof(argv[++i]);
			if (illumrad < MINDIM)
				illumrad = MINDIM;
			break;
		case 't':		/* override lamp type */
			lamptype = argv[++i];
			break;
		case 'u':		/* default lamp type */
			deflamp = argv[++i];
			break;
		case 'c':		/* default lamp color */
			defcolor[0] = atof(argv[++i]);
			defcolor[1] = atof(argv[++i]);
			defcolor[2] = atof(argv[++i]);
			break;
		case 'm':		/* multiplier */
			multiplier = atof(argv[++i]);
			break;
		default:
		badopt:
			fprintf(stderr, "%s: bad option: %s\n",
					argv[0], argv[i]);
			exit(1);
		}
	gargc = i;
	gargv = argv;
	initlamps();			/* get lamp data (if needed) */
					/* convert ies file(s) */
	if (outfile != NULL) {
		if (i == argc)
			exit(ies2rad(NULL, outfile) == 0 ? 0 : 1);
		else if (i == argc-1)
			exit(ies2rad(argv[i], outfile) == 0 ? 0 : 1);
		else {
			fprintf(stderr, "%s: single input file required\n",
					argv[0]);
			exit(1);
		}
	} else if (i >= argc) {
		fprintf(stderr, "%s: missing output file specification\n",
				argv[0]);
		exit(1);
	}
	status = 0;
	for ( ; i < argc; i++) {
		tailtrunc(strcpy(outname,filename(argv[i])));
		if (ies2rad(argv[i], outname) != 0)
			status = 1;
	}
	exit(status);
}


initlamps()				/* set up lamps */
{
	float	*lcol;
	int	status;

	if (lamptype != NULL && !strcmp(lamptype, default_name) &&
			deflamp == NULL)
		return;				/* no need for data */
						/* else load file */
	if ((status = loadlamps(lampdat)) < 0)
		exit(1);
	if (status == 0) {
		fprintf(stderr, "%s: warning - no lamp data\n", lampdat);
		lamptype = default_name;
		return;
	}
	if (deflamp != NULL) {			/* match default type */
		if ((lcol = matchlamp(deflamp)) == NULL)
			fprintf(stderr,
				"%s: warning - unknown default lamp type\n",
					deflamp);
		else
			copycolor(defcolor, lcol);
	}
	if (lamptype != NULL) {			/* match selected type */
		if (strcmp(lamptype, default_name)) {
			if ((lcol = matchlamp(lamptype)) == NULL) {
				fprintf(stderr,
					"%s: warning - unknown lamp type\n",
						lamptype);
				lamptype = default_name;
			} else
				copycolor(defcolor, lcol);
		}
		freelamps();			/* all done with data */
	}
						/* else keep lamp data */
}


char *
stradd(dst, src, sep)			/* add a string at dst */
register char	*dst, *src;
int	sep;
{
	if (src && *src) {
		do
			*dst++ = *src++;
		while (*src);
		if (sep && dst[-1] != sep)
			*dst++ = sep;
	}
	*dst = '\0';
	return(dst);
}


char *
fullname(path, fname, suffix)		/* return full path name */
char	*path, *fname, *suffix;
{
	if (prefdir != NULL && abspath(prefdir))
		libname(path, fname, suffix);
	else if (abspath(fname))
		strcpy(stradd(path, fname, 0), suffix);
	else
		libname(stradd(path, libdir, '/'), fname, suffix);

	return(path);
}


char *
libname(path, fname, suffix)		/* return library relative name */
char	*path, *fname, *suffix;
{
	if (abspath(fname))
		strcpy(stradd(path, fname, 0), suffix);
	else
		strcpy(stradd(stradd(path, prefdir, '/'), fname, 0), suffix);

	return(path);
}


char *
filename(path)			/* get final component of pathname */
register char	*path;
{
	register char	*cp;

	for (cp = path; *path; path++)
		if (*path == '/')
			cp = path+1;
	return(cp);
}


char *
filetrunc(path)				/* truncate filename at end of path */
char	*path;
{
	register char	*p1, *p2;

	for (p1 = p2 = path; *p2; p2++)
		if (*p2 == '/')
			p1 = p2;
	*p1 = '\0';
	return(path);
}


char *
tailtrunc(name)				/* truncate tail of filename */
char	*name;
{
	register char	*p1, *p2;

	for (p1 = filename(name); *p1 == '.'; p1++)
		;
	p2 = NULL;
	for ( ; *p1; p1++)
		if (*p1 == '.')
			p2 = p1;
	if (p2 != NULL)
		*p2 = '\0';
	return(name);
}


blanktrunc(s)				/* truncate spaces at end of line */
char	*s;
{
	register char	*cp;

	for (cp = s; *cp; cp++)
		;
	while (cp-- > s && isspace(*cp))
		;
	*++cp = '\0';
}


putheader(out)				/* print header */
FILE	*out;
{
	register int	i;
	
	putc('#', out);
	for (i = 0; i < gargc; i++) {
		putc(' ', out);
		fputs(gargv[i], out);
	}
	fputs("\n# Dimensions in ", out);
	fputs(units, out);
	putc('\n', out);
}


ies2rad(inpname, outname)		/* convert IES file */
char	*inpname, *outname;
{
	char	buf[MAXLINE], tltid[MAXWORD];
	FILE	*inpfp, *outfp;

	if (inpname == NULL) {
		inpname = "<stdin>";
		inpfp = stdin;
	} else if ((inpfp = fopen(inpname, "r")) == NULL) {
		perror(inpname);
		return(-1);
	}
	if ((outfp = fopen(fullname(buf,outname,T_RAD), "w")) == NULL) {
		perror(buf);
		fclose(inpfp);
		return(-1);
	}
	putheader(outfp);
	if (lamptype == NULL)
		lampcolor = NULL;
	while (fgets(buf,sizeof(buf),inpfp) != NULL
			&& strncmp(buf,TLTSTR,TLTSTRLEN)) {
		blanktrunc(buf);
		if (!buf[0])
			continue;
		fputs("#<", outfp);
		fputs(buf, outfp);
		putc('\n', outfp);
		if (lampcolor == NULL)
			lampcolor = matchlamp(buf);
	}
	if (lampcolor == NULL) {
		fprintf(stderr, "%s: warning - no lamp type\n", inpname);
		lampcolor = defcolor;
	}
	if (feof(inpfp)) {
		fprintf(stderr, "%s: not in IES format\n", inpname);
		goto readerr;
	}
	sscanf(buf+TLTSTRLEN, "%s", tltid);
	if (inpfp == stdin)
		buf[0] = '\0';
	else
		filetrunc(strcpy(buf, inpname));
	if (dotilt(inpfp, outfp, buf, tltid, outname, tltid) != 0) {
		fprintf(stderr, "%s: bad tilt data\n", inpname);
		goto readerr;
	}
	if (dosource(inpfp, outfp, tltid, outname) != 0) {
		fprintf(stderr, "%s: bad luminaire data\n", inpname);
		goto readerr;
	}
	fclose(outfp);
	fclose(inpfp);
	return(0);
readerr:
	fclose(outfp);
	fclose(inpfp);
	unlink(fullname(buf,outname,T_RAD));
	return(-1);
}


dotilt(in, out, dir, tltspec, dfltname, tltid)	/* convert tilt data */
FILE	*in, *out;
char	*dir, *tltspec, *dfltname, *tltid;
{
	int	nangles, tlt_type;
	double	minmax[2];
	char	buf[MAXPATH], tltname[MAXWORD];
	FILE	*datin, *datout;

	if (!strcmp(tltspec, TLTNONE)) {
		datin = NULL;
		strcpy(tltid, "void");
	} else if (!strcmp(tltspec, TLTINCL)) {
		datin = in;
		strcpy(tltname, dfltname);
	} else {
		if (tltspec[0] == '/')
			strcpy(buf, tltspec);
		else
			strcpy(stradd(buf, dir, '/'), tltspec);
		if ((datin = fopen(buf, "r")) == NULL) {
			perror(buf);
			return(-1);
		}
		tailtrunc(strcpy(tltname,filename(tltspec)));
	}
	if (datin != NULL) {
		if ((datout = fopen(fullname(buf,tltname,T_TLT),"w")) == NULL) {
			perror(buf);
			if (datin != in)
				fclose(datin);
			return(-1);
		}
		if (fscanf(datin, "%d %d", &tlt_type, &nangles) != 2
			|| cvdata(datin,datout,1,&nangles,1.,minmax) != 0) {
			fprintf(stderr, "%s: data format error\n", tltspec);
			fclose(datout);
			if (datin != in)
				fclose(datin);
			unlink(fullname(buf,tltname,T_TLT));
			return(-1);
		}
		fclose(datout);
		if (datin != in)
			fclose(datin);
		strcat(strcpy(tltid, filename(tltname)), "_tilt");
		fprintf(out, "\nvoid brightdata %s\n", tltid);
		libname(buf,tltname,T_TLT);
		switch (tlt_type) {
		case TLT_VERT:			/* vertical */
			fprintf(out, "4 noop %s tilt.cal %s\n", buf,
				minmax[1]>90.+FTINY ? "tilt_ang" : "tilt_ang2");
			break;
		case TLT_H0:			/* horiz. in 0 deg. plane */
			fprintf(out, "6 noop %s tilt.cal %s -rz 90\n", buf,
			minmax[1]>90.+FTINY ? "tilt_xang" : "tilt_xang2");
			break;
		case TLT_H90:
			fprintf(out, "4 noop %s tilt.cal %s\n", buf,
			minmax[1]>90.+FTINY ? "tilt_xang" : "tilt_xang2");
			break;
		default:
			fprintf(stderr,
				"%s: illegal lamp to luminaire geometry (%d)\n",
				tltspec, tlt_type);
			return(-1);
		}
		fprintf(out, "0\n0\n");
	}
	return(0);
}


dosource(in, out, mod, name)		/* create source and distribution */
FILE	*in, *out;
char	*mod, *name;
{
	SHAPE	srcshape;
	char	buf[MAXPATH], id[MAXWORD];
	FILE	*datout;
	double	mult, bfactor, pfactor, width, length, height, wattage;
	double	bounds[2][2];
	int	nangles[2], pmtype, unitype;
	double	d1;

	if (fscanf(in, "%*d %*f %lf %d %d %d %d %lf %lf %lf %lf %lf %lf",
			&mult, &nangles[0], &nangles[1], &pmtype, &unitype,
			&width, &length, &height, &bfactor, &pfactor,
			&wattage) != 11) {
		fprintf(stderr, "dosource: bad lamp specification\n");
		return(-1);
	}
	if (nangles[0] < 2 || nangles[1] < 1) {
		fprintf(stderr, "dosource: too few measured angles\n");
		return(-1);
	}
	if (unitype == U_FEET) {
		width *= F_M;
		length *= F_M;
		height *= F_M;
	}
	if (makeshape(&srcshape, width, length, height) != 0) {
		fprintf(stderr, "dosource: illegal source dimensions");
		return(-1);
	}
	if ((datout = fopen(fullname(buf,name,T_DST), "w")) == NULL) {
		perror(buf);
		return(-1);
	}
	if (cvdata(in, datout, 2, nangles, 1./683., bounds) != 0) {
		fprintf(stderr, "dosource: bad distribution data\n");
		fclose(datout);
		unlink(fullname(buf,name,T_DST));
		return(-1);
	}
	fclose(datout);
	fprintf(out, "# %g watt luminaire, lamp*ballast factor = %g\n",
			wattage, bfactor*pfactor);
	strcat(strcpy(id, filename(name)), "_dist");
	fprintf(out, "\n%s brightdata %s\n", mod, id);
	if (nangles[1] < 2)
		fprintf(out, "4 ");
	else if (pmtype == PM_B)
		fprintf(out, "5 ");
	else if (FEQ(bounds[1][0],90.) && FEQ(bounds[1][1],270.))
		fprintf(out, "8 ");
	else
		fprintf(out, "6 ");
	fprintf(out, "%s %s source.cal ",
			srcshape.type==SPHERE ? "corr" : "flatcorr",
			libname(buf,name,T_DST));
	if (pmtype == PM_B) {
		if (FEQ(bounds[1][0],0.))
			fprintf(out, "srcB_horiz2 ");
		else
			fprintf(out, "srcB_horiz ");
		fprintf(out, "srcB_vert ");
	} else {
		if (nangles[1] >= 2) {
			d1 = bounds[1][1] - bounds[1][0];
			if (d1 <= 90.+FTINY)
				fprintf(out, "src_phi4 ");
			else if (d1 <= 180.+FTINY)
				fprintf(out, "src_phi2 ");
			else
				fprintf(out, "src_phi ");
			fprintf(out, "src_theta -my ");
			if (FEQ(bounds[1][0],90.) && FEQ(bounds[1][1],270.))
				fprintf(out, "-rz -90 ");
		} else
			fprintf(out, "src_theta ");
	}
	fprintf(out, "\n0\n1 %g\n", multiplier*mult*bfactor*pfactor);
	if (putsource(&srcshape, out, id, filename(name),
			bounds[0][0]<90., bounds[0][1]>90.) != 0)
		return(-1);
	return(0);
}


putsource(shp, fp, mod, name, dolower, doupper)		/* put out source */
SHAPE	*shp;
FILE	*fp;
char	*mod, *name;
int	dolower, doupper;
{
	char	buf[MAXWORD];
	
	fprintf(fp, "\n%s %s %s_light\n", mod,
			illumrad>=MINDIM/2. ? "illum" : "light",
			name);
	fprintf(fp, "0\n0\n3 %g %g %g\n",
			lampcolor[0]/shp->area,
			lampcolor[1]/shp->area,
			lampcolor[2]/shp->area);
	if (doupper && dolower && shp->type != SPHERE && shp->h > MINDIM) {
		fprintf(fp, "\n%s glow %s_glow\n", mod, name);
		fprintf(fp, "0\n0\n4 %g %g %g 0\n",
				lampcolor[0]/shp->area,
				lampcolor[1]/shp->area,
				lampcolor[2]/shp->area);
	}
	switch (shp->type) {
	case RECT:
		strcat(strcpy(buf, name), "_light");
		if (dolower)
			putrectsrc(shp, fp, buf, name, 0);
		if (doupper)
			putrectsrc(shp, fp, buf, name, 1);
		if (doupper && dolower && shp->h > MINDIM) {
			strcat(strcpy(buf, name), "_glow");
			putsides(shp, fp, buf, name);
		}
		break;
	case DISK:
		strcat(strcpy(buf, name), "_light");
		if (dolower)
			putdisksrc(shp, fp, buf, name, 0);
		if (doupper)
			putdisksrc(shp, fp, buf, name, 1);
		if (doupper && dolower && shp->h > MINDIM) {
			strcat(strcpy(buf, name), "_glow");
			putcyl(shp, fp, buf, name);
		}
		break;
	case SPHERE:
		strcat(strcpy(buf, name), "_light");
		putspheresrc(shp, fp, buf, name);
		break;
	}
	return(0);
}


makeshape(shp, width, length, height)		/* make source shape */
register SHAPE	*shp;
double	width, length, height;
{
	if (illumrad >= MINDIM/2.) {
		shp->type = SPHERE;
		shp->w = shp->l = shp->h = 2.*illumrad;
	} else if (width < MINDIM) {
		width = -width;
		if (width < MINDIM) {
			shp->type = SPHERE;
			shp->w = shp->l = shp->h = MINDIM;
		} else if (height < .5*width) {
			shp->type = DISK;
			shp->w = shp->l = width;
			if (height >= MINDIM)
				shp->h = height;
			else
				shp->h = .5*MINDIM;
		} else {
			shp->type = SPHERE;
			shp->w = shp->l = shp->h = width;
		}
	} else {
		shp->type = RECT;
		shp->w = width;
		if (length >= MINDIM)
			shp->l = length;
		else
			shp->l = MINDIM;
		if (height >= MINDIM)
			shp->h = height;
		else
			shp->h = .5*MINDIM;
	}
	switch (shp->type) {
	case RECT:
		shp->area = shp->w * shp->l;
		break;
	case DISK:
		shp->area = PI/4. * shp->w * shp->w;
		break;
	case SPHERE:
		shp->area = PI * shp->w * shp->w;
		break;
	}
	return(0);
}


putrectsrc(shp, fp, mod, name, up)		/* rectangular source */
SHAPE	*shp;
FILE	*fp;
char	*mod, *name;
int	up;
{
	if (up)
		putrect(shp, fp, mod, name, ".u", 4, 5, 7, 6);
	else
		putrect(shp, fp, mod, name, ".d", 0, 2, 3, 1);
}


putsides(shp, fp, mod, name)			/* put out sides of box */
register SHAPE	*shp;
FILE	*fp;
char	*mod, *name;
{
	putrect(shp, fp, mod, name, ".1", 0, 1, 5, 4);
	putrect(shp, fp, mod, name, ".2", 1, 3, 7, 5);
	putrect(shp, fp, mod, name, ".3", 3, 2, 6, 7);
	putrect(shp, fp, mod, name, ".4", 2, 0, 4, 6);
}
	

putrect(shp, fp, mod, name, suffix, a, b, c, d)	/* put out a rectangle */
SHAPE	*shp;
FILE	*fp;
char	*mod, *name, *suffix;
int	a, b, c, d;
{
	fprintf(fp, "\n%s polygon %s%s\n0\n0\n12\n", mod, name, suffix);
	putpoint(shp, fp, a);
	putpoint(shp, fp, b);
	putpoint(shp, fp, c);
	putpoint(shp, fp, d);
}


putpoint(shp, fp, p)				/* put out a point */
register SHAPE	*shp;
FILE	*fp;
int	p;
{
	static double	mult[2] = {-.5, .5};

	fprintf(fp, "\t%g\t%g\t%g\n",
			mult[p&1]*shp->l*meters2out,
			mult[p>>1&1]*shp->w*meters2out,
			mult[p>>2]*shp->h*meters2out);
}


putdisksrc(shp, fp, mod, name, up)		/* put out a disk source */
register SHAPE	*shp;
FILE	*fp;
char	*mod, *name;
int	up;
{
	if (up) {
		fprintf(fp, "\n%s ring %s.u\n", mod, name);
		fprintf(fp, "0\n0\n8\n");
		fprintf(fp, "\t0 0 %g\n", .5*shp->h*meters2out);
		fprintf(fp, "\t0 0 1\n");
		fprintf(fp, "\t0 %g\n", .5*shp->w*meters2out);
	} else {
		fprintf(fp, "\n%s ring %s.d\n", mod, name);
		fprintf(fp, "0\n0\n8\n");
		fprintf(fp, "\t0 0 %g\n", -.5*shp->h*meters2out);
		fprintf(fp, "\t0 0 -1\n");
		fprintf(fp, "\t0 %g\n", .5*shp->w*meters2out);
	}
}


putcyl(shp, fp, mod, name)			/* put out a cylinder */
register SHAPE	*shp;
FILE	*fp;
char	*mod, *name;
{
	fprintf(fp, "\n%s cylinder %s.c\n", mod, name);
	fprintf(fp, "0\n0\n7\n");
	fprintf(fp, "\t0 0 %g\n", .5*shp->h*meters2out);
	fprintf(fp, "\t0 0 %g\n", -.5*shp->h*meters2out);
	fprintf(fp, "\t%g\n", .5*shp->w*meters2out);
}


putspheresrc(shp, fp, mod, name)		/* put out a sphere source */
SHAPE	*shp;
FILE	*fp;
char	*mod, *name;
{
	fprintf(fp, "\n%s sphere %s.s\n", mod, name);
	fprintf(fp, "0\n0\n4 0 0 0 %g\n", .5*shp->w*meters2out);
}


cvdata(in, out, ndim, npts, mult, lim)		/* convert data */
FILE	*in, *out;
int	ndim, npts[];
double	mult, lim[][2];
{
	register double	*pt[4];
	register int	i, j;
	double	val;
	int	total;

	total = 1; j = 0;
	for (i = 0; i < ndim; i++)
		if (npts[i] > 1) {
			total *= npts[i];
			j++;
		}
	fprintf(out, "%d\n", j);
					/* get coordinates */
	for (i = 0; i < ndim; i++) {
		pt[i] = (double *)malloc(npts[i]*sizeof(double));
		for (j = 0; j < npts[i]; j++)
			fscanf(in, "%lf", &pt[i][j]);
		if (lim != NULL) {
			lim[i][0] = pt[i][0];
			lim[i][1] = pt[i][npts[i]-1];
		}
	}
					/* write out in reverse */
	for (i = ndim-1; i >= 0; i--) {
		if (npts[i] > 1) {
			for (j = 1; j < npts[i]-1; j++)
				if (!FEQ(pt[i][j]-pt[i][j-1],
						pt[i][j+1]-pt[i][j]))
					break;
			if (j == npts[i]-1)
				fprintf(out, "%g %g %d\n", pt[i][0], pt[i][j],
						npts[i]);
			else {
				fprintf(out, "0 0 %d", npts[i]);
				for (j = 0; j < npts[i]; j++) {
					if (j%4 == 0)
						putc('\n', out);
					fprintf(out, "\t%g", pt[i][j]);
				}
				putc('\n', out);
			}
		}
		free((char *)pt[i]);
	}
	for (i = 0; i < total; i++) {
		if (i%4 == 0)
			putc('\n', out);
		if (fscanf(in, "%lf", &val) != 1)
			return(-1);
		fprintf(out, "\t%g", val*mult);
	}
	putc('\n', out);
	return(0);
}

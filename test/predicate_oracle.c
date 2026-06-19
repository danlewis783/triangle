/*****************************************************************************/
/*                                                                           */
/*  predicate_oracle.c                                                       */
/*                                                                           */
/*  Captures the EXACT SIGN of Triangle's robust geometric predicates        */
/*  (counterclockwise / orient2d, and incircle) over a battery of inputs,    */
/*  writing <outdir>/predicates.txt as "<inputs> <sign>" lines.              */
/*                                                                           */
/*  Purpose: a fine-grained, language-independent oracle for the Java port.  */
/*  The sign of these determinants - NOT their magnitude - is what the mesh  */
/*  algorithm depends on, and getting it right on near-degenerate inputs is  */
/*  the entire reason Triangle uses adaptive exact arithmetic instead of     */
/*  naive doubles.  A ported predicate must reproduce every sign here.       */
/*  Magnitude is deliberately discarded: a re-derived predicate (FMA-based   */
/*  or BigInteger-based) may return a different value but must agree on sign. */
/*                                                                           */
/*  White-box harness: it #includes triangle.c to reach the internal         */
/*  predicate functions and the globals exactinit() sets up.  All inputs are */
/*  exact, explicit constants (no PRNG) so the Java oracle can use the same   */
/*  coordinates and compare signs directly.                                  */
/*                                                                           */
/*  Usage:  predicate_oracle <output-directory>                             */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define REAL double
#include "triangle.c"

static struct mesh m;
static struct behavior b;
static FILE *fp;

static int sgn(REAL v)
{
  return (v > 0.0) - (v < 0.0);
}

/* orient2d: sign is +1 if (a,b,c) make a left turn (counterclockwise), */
/* -1 for a right turn, 0 if exactly collinear.                         */
static void o2d(REAL ax, REAL ay, REAL bx, REAL by, REAL cx, REAL cy)
{
  REAL pa[2], pb[2], pc[2];
  pa[0] = ax; pa[1] = ay;
  pb[0] = bx; pb[1] = by;
  pc[0] = cx; pc[1] = cy;
  fprintf(fp, "orient2d %.17g %.17g %.17g %.17g %.17g %.17g %d\n",
          ax, ay, bx, by, cx, cy, sgn(counterclockwise(&m, &b, pa, pb, pc)));
}

/* incircle: sign is +1 if d is inside the circle through (a,b,c) given */
/* counterclockwise (a,b,c), -1 if outside, 0 if exactly cocircular.    */
static void inc(REAL ax, REAL ay, REAL bx, REAL by,
                REAL cx, REAL cy, REAL dx, REAL dy)
{
  REAL pa[2], pb[2], pc[2], pd[2];
  pa[0] = ax; pa[1] = ay;
  pb[0] = bx; pb[1] = by;
  pc[0] = cx; pc[1] = cy;
  pd[0] = dx; pd[1] = dy;
  fprintf(fp, "incircle %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %d\n",
          ax, ay, bx, by, cx, cy, dx, dy,
          sgn(incircle(&m, &b, pa, pb, pc, pd)));
}

int main(int argc, char **argv)
{
  char path[1024];
  int i, j;
  /* Perturbation magnitudes: exact powers of two straddling the double      */
  /* precision threshold (2^-52), plus a few non-power-of-two decimals.      */
  static const int p2exp[] = { -20, -30, -40, -48, -52, -53, -60, -70, -100 };
  static const double decimals[] = { 3e-16, 1.7e-15, 2.5e-10, 9.9e-13, 7e-18 };
  static const double scales[] = { 0.5, 1.0, 100.0, 1.0e6, 1.0e-6 };

  if (argc != 2) {
    fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
    return 1;
  }

  memset(&m, 0, sizeof(m));
  memset(&b, 0, sizeof(b));
  b.noexact = 0;                 /* use the exact predicates */
  exactinit();                   /* initialise splitter and the error bounds */

  snprintf(path, sizeof(path), "%s/predicates.txt", argv[1]);
  fp = fopen(path, "wb");
  if (!fp) {
    fprintf(stderr, "predicate_oracle: cannot write %s\n", path);
    return 2;
  }

  /* --- orient2d: curated sanity cases --------------------------------- */
  o2d(0,0,  1,0,  0,1);          /* left turn  -> + */
  o2d(0,0,  0,1,  1,0);          /* right turn -> - */
  o2d(0,0,  1,1,  2,2);          /* collinear  -> 0 */
  o2d(0,0,  5,0,  2,0);          /* collinear (x axis) -> 0 */
  o2d(-3,-7, 11,2, 4,9);         /* arbitrary  */

  /* --- orient2d: near-degenerate sweep -------------------------------- */
  /* Points (0,0) and (s,s) define a line of slope 1; the third point sits */
  /* on it at (s/2, s/2) and is nudged off by +/-d.  The exact orientation */
  /* sign is sign(d), with magnitude ~d -> exercises adaptive escalation.  */
  for (i = 0; i < (int)(sizeof(scales)/sizeof(scales[0])); i++) {
    REAL s = scales[i];
    REAL mx = s * 0.5, my = s * 0.5;
    o2d(0,0, s,s, mx, my);                       /* exact on the line -> 0 */
    for (j = 0; j < (int)(sizeof(p2exp)/sizeof(p2exp[0])); j++) {
      REAL d = ldexp(s, p2exp[j]);               /* d = s * 2^p2exp[j] */
      o2d(0,0, s,s, mx, my + d);
      o2d(0,0, s,s, mx, my - d);
    }
    for (j = 0; j < (int)(sizeof(decimals)/sizeof(decimals[0])); j++) {
      REAL d = s * decimals[j];
      o2d(0,0, s,s, mx, my + d);
      o2d(0,0, s,s, mx, my - d);
    }
  }

  /* --- incircle: curated sanity cases --------------------------------- */
  inc(1,0,  0,1,  -1,0,   0,0);    /* centre, inside    -> + */
  inc(1,0,  0,1,  -1,0,   2,2);    /* far outside       -> - */
  inc(1,0,  0,1,  -1,0,   0,-1);   /* on unit circle    -> 0 */

  /* --- incircle: near-cocircular sweep -------------------------------- */
  /* (R,0),(0,R),(-R,0) lie on the circle of radius R about the origin.    */
  /* The fourth point starts at (0,-R) (exactly cocircular) and is moved   */
  /* radially by +/-d, so the exact in/out sign turns on a tiny margin.    */
  for (i = 0; i < (int)(sizeof(scales)/sizeof(scales[0])); i++) {
    REAL R = scales[i];
    inc(R,0, 0,R, -R,0,  0,-R);                  /* exactly cocircular -> 0 */
    for (j = 0; j < (int)(sizeof(p2exp)/sizeof(p2exp[0])); j++) {
      REAL d = ldexp(R, p2exp[j]);
      inc(R,0, 0,R, -R,0,  0, -(R + d));
      inc(R,0, 0,R, -R,0,  0, -(R - d));
    }
    for (j = 0; j < (int)(sizeof(decimals)/sizeof(decimals[0])); j++) {
      REAL d = R * decimals[j];
      inc(R,0, 0,R, -R,0,  0, -(R + d));
      inc(R,0, 0,R, -R,0,  0, -(R - d));
    }
  }

  fclose(fp);
  printf("wrote %s\n", path);
  return 0;
}

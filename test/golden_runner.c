/*****************************************************************************/
/*                                                                           */
/*  golden_runner.c                                                          */
/*                                                                           */
/*  Characterization ("golden") harness for Triangle.  For each scenario it  */
/*  runs triangulate() through the library API and writes a deterministic    */
/*  text dump of the output mesh to <outdir>/<name>.txt.                     */
/*                                                                           */
/*  The dumps captured from the current, known-good build are the baseline   */
/*  ("golden") files.  Re-running after a code change and diffing against     */
/*  the baseline catches ANY change in output - the tripwire we need before   */
/*  stripping unused code.  Triangle is deterministic for a given input and   */
/*  switch set, so "identical output" is a meaningful, strict contract.       */
/*                                                                           */
/*  The scenarios deliberately exercise the full feature set the port keeps:  */
/*  PSLG + segment markers, constrained triangulation, quality meshing (q),   */
/*  regional area constraints (a) and region attributes (A), holes, neighbor  */
/*  output (n), segment/edge output, all zero-based (z) and quiet (Q).        */
/*                                                                           */
/*  Usage:  golden_runner <output-directory>                                 */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REAL double
#include "triangle.h"

/* --- Output serialization ------------------------------------------------ */

/* Written in binary mode with explicit '\n' so the on-disk bytes are LF on   */
/* every platform; full %.17g precision makes doubles round-trip exactly.     */
static void dump(FILE *f, struct triangulateio *io)
{
  int i, j;
  int c = io->numberofcorners;

  fprintf(f, "numberofpoints %d\n", io->numberofpoints);
  fprintf(f, "numberofcorners %d\n", c);
  fprintf(f, "numberoftriangles %d\n", io->numberoftriangles);
  fprintf(f, "numberoftriangleattributes %d\n", io->numberoftriangleattributes);
  fprintf(f, "numberofsegments %d\n", io->numberofsegments);
  fprintf(f, "numberofedges %d\n", io->numberofedges);

  fprintf(f, "points\n");
  for (i = 0; i < io->numberofpoints; i++) {
    fprintf(f, "%d %.17g %.17g\n", i,
            io->pointlist[i * 2], io->pointlist[i * 2 + 1]);
  }

  fprintf(f, "triangles\n");
  for (i = 0; i < io->numberoftriangles; i++) {
    fprintf(f, "%d", i);
    for (j = 0; j < c; j++) {
      fprintf(f, " %d", io->trianglelist[i * c + j]);
    }
    fprintf(f, "\n");
  }

  if (io->numberoftriangleattributes > 0 && io->triangleattributelist) {
    int na = io->numberoftriangleattributes;
    fprintf(f, "triangleattributes\n");
    for (i = 0; i < io->numberoftriangles; i++) {
      fprintf(f, "%d", i);
      for (j = 0; j < na; j++) {
        fprintf(f, " %.17g", io->triangleattributelist[i * na + j]);
      }
      fprintf(f, "\n");
    }
  }

  if (io->neighborlist) {
    fprintf(f, "neighbors\n");
    for (i = 0; i < io->numberoftriangles; i++) {
      fprintf(f, "%d %d %d %d\n", i,
              io->neighborlist[i * 3],
              io->neighborlist[i * 3 + 1],
              io->neighborlist[i * 3 + 2]);
    }
  }

  if (io->segmentlist) {
    fprintf(f, "segments\n");
    for (i = 0; i < io->numberofsegments; i++) {
      fprintf(f, "%d %d %d\n", i,
              io->segmentlist[i * 2], io->segmentlist[i * 2 + 1]);
    }
  }

  if (io->segmentmarkerlist) {
    fprintf(f, "segmentmarkers\n");
    for (i = 0; i < io->numberofsegments; i++) {
      fprintf(f, "%d %d\n", i, io->segmentmarkerlist[i]);
    }
  }

  if (io->edgelist) {
    fprintf(f, "edges\n");
    for (i = 0; i < io->numberofedges; i++) {
      fprintf(f, "%d %d %d\n", i,
              io->edgelist[i * 2], io->edgelist[i * 2 + 1]);
    }
  }
}

static void free_output(struct triangulateio *out)
{
  free(out->pointlist);
  free(out->pointattributelist);
  free(out->pointmarkerlist);
  free(out->trianglelist);
  free(out->triangleattributelist);
  free(out->neighborlist);
  free(out->segmentlist);
  free(out->segmentmarkerlist);
  free(out->edgelist);
  free(out->edgemarkerlist);
}

static const char *g_outdir;

/* Run one scenario and write its dump.  `flags' is copied to a writable      */
/* buffer because triangulate() parses it like a command line.                */
static void run(const char *name, const char *flags, struct triangulateio *in)
{
  struct triangulateio out;
  char flagbuf[64];
  char path[1024];
  FILE *f;

  memset(&out, 0, sizeof(out));
  strncpy(flagbuf, flags, sizeof(flagbuf) - 1);
  flagbuf[sizeof(flagbuf) - 1] = '\0';

  triangulate(flagbuf, in, &out, NULL);

  snprintf(path, sizeof(path), "%s/%s.txt", g_outdir, name);
  f = fopen(path, "wb");
  if (!f) {
    fprintf(stderr, "golden_runner: cannot write %s\n", path);
    exit(2);
  }
  dump(f, &out);
  fclose(f);
  printf("  %-20s -> %d pts, %d tris\n",
         name, out.numberofpoints, out.numberoftriangles);

  free_output(&out);
}

/* --- Scenarios ----------------------------------------------------------- */

/* A unit square as a PSLG with distinct segment markers. */
static void scenario_pslg_square(void)
{
  static REAL points[8]   = { 0,0,  1,0,  1,1,  0,1 };
  static int  segs[8]     = { 0,1,  1,2,  2,3,  3,0 };
  static int  segmarks[4] = { 11, 12, 13, 14 };
  struct triangulateio in;

  memset(&in, 0, sizeof(in));
  in.numberofpoints = 4;
  in.pointlist = points;
  in.numberofsegments = 4;
  in.segmentlist = segs;
  in.segmentmarkerlist = segmarks;

  /* p: PSLG, n: neighbors, e: edges, z: zero-based, Q: quiet. */
  run("pslg_square", "pnzeQ", &in);

  /* Same input, with quality + a global max-area constraint -> refinement. */
  run("pslg_square_quality", "pq30a0.05nzeQ", &in);
}

/* A 2x1 rectangle split into two regions by a middle segment, with distinct  */
/* region attributes and per-region max-area constraints.                     */
static void scenario_regions(void)
{
  static REAL points[12] = { 0,0,  1,0,  2,0,  2,1,  1,1,  0,1 };
  static int  segs[14]   = { 0,1,  1,2,  2,3,  3,4,  4,5,  5,0,  1,4 };
  static int  segmarks[7]= { 1, 1, 1, 1, 1, 1, 2 };
  /* x, y, attribute, max-area  (4 reals per region) */
  static REAL regions[8] = { 0.5,0.5, 1.0, 0.05,
                             1.5,0.5, 2.0, 0.20 };
  struct triangulateio in;

  memset(&in, 0, sizeof(in));
  in.numberofpoints = 6;
  in.pointlist = points;
  in.numberofsegments = 7;
  in.segmentlist = segs;
  in.segmentmarkerlist = segmarks;
  in.numberofregions = 2;
  in.regionlist = regions;

  /* A: regional attributes, a (no number): use per-region areas from list. */
  run("regions", "pq20AanzeQ", &in);
}

/* A square with a square hole carved out (an annulus). */
static void scenario_hole(void)
{
  static REAL points[16] = { 0,0,  4,0,  4,4,  0,4,      /* outer */
                             1,1,  3,1,  3,3,  1,3 };     /* inner */
  static int  segs[16]   = { 0,1,  1,2,  2,3,  3,0,       /* outer */
                             4,5,  5,6,  6,7,  7,4 };      /* inner */
  static int  segmarks[8]= { 1, 1, 1, 1,  2, 2, 2, 2 };
  static REAL holes[2]   = { 2, 2 };                       /* inside inner */
  struct triangulateio in;

  memset(&in, 0, sizeof(in));
  in.numberofpoints = 8;
  in.pointlist = points;
  in.numberofsegments = 8;
  in.segmentlist = segs;
  in.segmentmarkerlist = segmarks;
  in.numberofholes = 1;
  in.holelist = holes;

  run("hole", "pnzeQ", &in);
}

/* A rectangle enclosing a zig-zag point field, with an interior constraint    */
/* segment (4->5) running horizontally through it.  The zig-zag points          */
/* alternate above and below that line, so the constraint is NOT a Delaunay     */
/* edge and must be recovered by flipping the crossed edges (exercises          */
/* constrainededge / delaunayfixup).                                            */
static void scenario_segment_recovery(void)
{
  static REAL points[22] = { 0,0,  8,0,  8,4,  0,4,     /* enclosing rectangle */
                             1,2,  7,2,                  /* constraint endpoints */
                             2,3,  3,1,  4,3,  5,1,  6,3 };  /* zig-zag interior */
  static int  segs[10]   = { 0,1,  1,2,  2,3,  3,0,      /* boundary */
                             4,5 };                       /* interior constraint */
  static int  segmarks[5]= { 1, 1, 1, 1,  7 };
  struct triangulateio in;

  memset(&in, 0, sizeof(in));
  in.numberofpoints = 11;
  in.pointlist = points;
  in.numberofsegments = 5;
  in.segmentlist = segs;
  in.segmentmarkerlist = segmarks;

  run("segment_recovery", "pnzeQ", &in);
}

/* A square boundary with both diagonals as interior constraints.  The two      */
/* diagonals physically cross at the centre - a point not present in the input  */
/* - so Triangle must compute the intersection and insert a Steiner vertex      */
/* there (exercises segmentintersection), yielding four triangles.              */
static void scenario_segment_intersection(void)
{
  static REAL points[8]   = { 0,0,  4,0,  4,4,  0,4 };
  static int  segs[12]    = { 0,1,  1,2,  2,3,  3,0,     /* boundary */
                              0,2,  1,3 };                /* crossing diagonals */
  static int  segmarks[6] = { 1, 1, 1, 1,  5, 6 };
  struct triangulateio in;

  memset(&in, 0, sizeof(in));
  in.numberofpoints = 4;
  in.pointlist = points;
  in.numberofsegments = 6;
  in.segmentlist = segs;
  in.segmentmarkerlist = segmarks;

  run("segment_intersection", "pnzeQ", &in);
}

/* A concave (L-shaped) domain.  Its boundary is not its convex hull, so        */
/* Triangle meshes the hull and then carves away the triangles in the notch,    */
/* deallocating the subsegments along the way (exercises the non-convex carving */
/* path, including subsegdealloc).  Non-convex domains are the common case for   */
/* real PSLG input, so this also makes the corpus more representative.          */
static void scenario_concave_lshape(void)
{
  static REAL points[12] = { 0,0,  4,0,  4,2,  2,2,  2,4,  0,4 };
  static int  segs[12]   = { 0,1,  1,2,  2,3,  3,4,  4,5,  5,0 };
  static int  segmarks[6]= { 1, 1, 1, 1, 1, 1 };
  struct triangulateio in;

  memset(&in, 0, sizeof(in));
  in.numberofpoints = 6;
  in.pointlist = points;
  in.numberofsegments = 6;
  in.segmentlist = segs;
  in.segmentmarkerlist = segmarks;

  run("concave_lshape", "pnzeQ", &in);
}

int main(int argc, char **argv)
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
    return 1;
  }
  g_outdir = argv[1];

  printf("Generating golden dumps in '%s':\n", g_outdir);
  scenario_pslg_square();
  scenario_regions();
  scenario_hole();
  scenario_segment_recovery();
  scenario_segment_intersection();
  scenario_concave_lshape();
  return 0;
}

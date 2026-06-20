/*****************************************************************************/
/*                                                                           */
/*  scenarios.c                                                              */
/*                                                                           */
/*  The shared test inputs (see scenarios.h).  Each builder zero-initialises */
/*  the triangulateio and fills it from static arrays; the scenarios[] table */
/*  pairs each builder with a name and the triangulate() switch string.      */
/*  Two entries share build_square: a plain constrained mesh and a refined   */
/*  one, to exercise both paths from the same geometry.                       */
/*                                                                           */
/*****************************************************************************/

#include <string.h>
#include "scenarios.h"

/* A unit square as a PSLG with distinct segment markers. */
static void build_square(struct triangulateio *in)
{
  static REAL points[8]   = { 0,0,  1,0,  1,1,  0,1 };
  static int  segs[8]     = { 0,1,  1,2,  2,3,  3,0 };
  static int  segmarks[4] = { 11, 12, 13, 14 };

  memset(in, 0, sizeof(*in));
  in->numberofpoints = 4;
  in->pointlist = points;
  in->numberofsegments = 4;
  in->segmentlist = segs;
  in->segmentmarkerlist = segmarks;
}

/* A 2x1 rectangle split into two regions by a middle segment, with distinct  */
/* region attributes and per-region max-area constraints.                     */
static void build_regions(struct triangulateio *in)
{
  static REAL points[12] = { 0,0,  1,0,  2,0,  2,1,  1,1,  0,1 };
  static int  segs[14]   = { 0,1,  1,2,  2,3,  3,4,  4,5,  5,0,  1,4 };
  static int  segmarks[7]= { 1, 1, 1, 1, 1, 1, 2 };
  static REAL regions[8] = { 0.5,0.5, 1.0, 0.05,
                             1.5,0.5, 2.0, 0.20 };

  memset(in, 0, sizeof(*in));
  in->numberofpoints = 6;
  in->pointlist = points;
  in->numberofsegments = 7;
  in->segmentlist = segs;
  in->segmentmarkerlist = segmarks;
  in->numberofregions = 2;
  in->regionlist = regions;
}

/* A square with a square hole carved out (an annulus). */
static void build_hole(struct triangulateio *in)
{
  static REAL points[16] = { 0,0,  4,0,  4,4,  0,4,      /* outer */
                             1,1,  3,1,  3,3,  1,3 };     /* inner */
  static int  segs[16]   = { 0,1,  1,2,  2,3,  3,0,       /* outer */
                             4,5,  5,6,  6,7,  7,4 };      /* inner */
  static int  segmarks[8]= { 1, 1, 1, 1,  2, 2, 2, 2 };
  static REAL holes[2]   = { 2, 2 };                       /* inside inner */

  memset(in, 0, sizeof(*in));
  in->numberofpoints = 8;
  in->pointlist = points;
  in->numberofsegments = 8;
  in->segmentlist = segs;
  in->segmentmarkerlist = segmarks;
  in->numberofholes = 1;
  in->holelist = holes;
}

/* A rectangle enclosing a zig-zag point field, with an interior constraint   */
/* segment (4->5) the points straddle, so it must be recovered by flips.       */
static void build_segment_recovery(struct triangulateio *in)
{
  static REAL points[22] = { 0,0,  8,0,  8,4,  0,4,
                             1,2,  7,2,
                             2,3,  3,1,  4,3,  5,1,  6,3 };
  static int  segs[10]   = { 0,1,  1,2,  2,3,  3,0,  4,5 };
  static int  segmarks[5]= { 1, 1, 1, 1,  7 };

  memset(in, 0, sizeof(*in));
  in->numberofpoints = 11;
  in->pointlist = points;
  in->numberofsegments = 5;
  in->segmentlist = segs;
  in->segmentmarkerlist = segmarks;
}

/* A square boundary with both diagonals as constraints that cross at the     */
/* centre, forcing a computed intersection vertex.                            */
static void build_segment_intersection(struct triangulateio *in)
{
  static REAL points[8]   = { 0,0,  4,0,  4,4,  0,4 };
  static int  segs[12]    = { 0,1,  1,2,  2,3,  3,0,  0,2,  1,3 };
  static int  segmarks[6] = { 1, 1, 1, 1,  5, 6 };

  memset(in, 0, sizeof(*in));
  in->numberofpoints = 4;
  in->pointlist = points;
  in->numberofsegments = 6;
  in->segmentlist = segs;
  in->segmentmarkerlist = segmarks;
}

/* A concave (L-shaped) domain: boundary is not its convex hull, so the notch */
/* is carved away.                                                            */
static void build_concave_lshape(struct triangulateio *in)
{
  static REAL points[12] = { 0,0,  4,0,  4,2,  2,2,  2,4,  0,4 };
  static int  segs[12]   = { 0,1,  1,2,  2,3,  3,4,  4,5,  5,0 };
  static int  segmarks[6]= { 1, 1, 1, 1, 1, 1 };

  memset(in, 0, sizeof(*in));
  in->numberofpoints = 6;
  in->pointlist = points;
  in->numberofsegments = 6;
  in->segmentlist = segs;
  in->segmentmarkerlist = segmarks;
}

const scenario scenarios[] = {
  { "pslg_square",          "pnzeQ",          build_square },
  { "pslg_square_quality",  "pq30a0.05nzeQ",  build_square },
  { "regions",              "pq20AanzeQ",     build_regions },
  { "hole",                 "pnzeQ",          build_hole },
  { "segment_recovery",     "pnzeQ",          build_segment_recovery },
  { "segment_intersection", "pnzeQ",          build_segment_intersection },
  { "concave_lshape",       "pnzeQ",          build_concave_lshape },
};

const int scenario_count = (int) (sizeof(scenarios) / sizeof(scenarios[0]));

/*****************************************************************************/
/*                                                                           */
/*  contract_validator.c                                                     */
/*                                                                           */
/*  Checks that a mesh satisfies the STRUCTURAL CONTRACT the downstream       */
/*  consumer depends on - not byte-for-byte identity with Triangle.  This is */
/*  the acceptance bar for a reimplementation (e.g. the Java port): produce  */
/*  *a* valid mesh that passes these invariants, rather than reproduce        */
/*  Triangle's exact vertex numbering / triangle order / Steiner placement.  */
/*                                                                           */
/*  Here the validators are run against Triangle's own output for the shared */
/*  scenarios, proving the checks are correct (Triangle must pass) and        */
/*  pinning the invariants as an executable spec.  The same checks can later  */
/*  be ported to validate the Java mesher's output.                          */
/*                                                                           */
/*  Invariants checked (this file):                                          */
/*    1. Topological validity - indices in range, distinct corners, every    */
/*       triangle non-degenerate with consistent orientation, and every      */
/*       undirected edge shared by exactly 1 (boundary) or 2 (interior)      */
/*       triangles (a manifold triangulation).                               */
/*    2. Neighbour-slot semantics - neighbour[i][j] is the triangle across   */
/*       the edge OPPOSITE corner j, adjacency is symmetric, and boundary    */
/*       slots (-1) line up with boundary edges.                             */
/*                                                                           */
/*  Usage:  contract_validator           (exits non-zero if any check fails) */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scenarios.h"

#define CORNER(o, i, k) ((o)->trianglelist[(i) * 3 + (k)])
#define MAX_REPORT 4           /* cap messages per check to avoid a flood */

static int tri_has(struct triangulateio *o, int t, int v)
{
  return CORNER(o, t, 0) == v || CORNER(o, t, 1) == v || CORNER(o, t, 2) == v;
}

/* --- Invariant 1: topological validity ----------------------------------- */

typedef struct { int a, b; } edge;

static int edge_cmp(const void *x, const void *y)
{
  const edge *p = (const edge *) x, *q = (const edge *) y;
  if (p->a != q->a) return p->a - q->a;
  return p->b - q->b;
}

static int validate_topology(const char *name, struct triangulateio *o)
{
  int viol = 0, reported = 0;
  int nt = o->numberoftriangles, np = o->numberofpoints, i, k;
  int orient = 0;                 /* expected signed-area sign, set on first */
  edge *edges;
  int ne, r;

  if (o->numberofcorners != 3) {
    printf("      [topology] %s: numberofcorners=%d (expected 3)\n",
           name, o->numberofcorners);
    return 1;
  }

  for (i = 0; i < nt; i++) {
    int a = CORNER(o, i, 0), b = CORNER(o, i, 1), c = CORNER(o, i, 2);
    REAL ax, ay, bx, by, cx, cy, area2;
    int s;

    if (a < 0 || a >= np || b < 0 || b >= np || c < 0 || c >= np) {
      if (reported++ < MAX_REPORT)
        printf("      [topology] %s: tri %d has out-of-range corner\n", name, i);
      viol++; continue;
    }
    if (a == b || b == c || a == c) {
      if (reported++ < MAX_REPORT)
        printf("      [topology] %s: tri %d has a repeated corner\n", name, i);
      viol++; continue;
    }
    ax = o->pointlist[2*a]; ay = o->pointlist[2*a + 1];
    bx = o->pointlist[2*b]; by = o->pointlist[2*b + 1];
    cx = o->pointlist[2*c]; cy = o->pointlist[2*c + 1];
    area2 = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    s = (area2 > 0) - (area2 < 0);
    if (s == 0) {
      if (reported++ < MAX_REPORT)
        printf("      [topology] %s: tri %d is degenerate (zero area)\n", name, i);
      viol++;
    } else if (orient == 0) {
      orient = s;
    } else if (s != orient) {
      if (reported++ < MAX_REPORT)
        printf("      [topology] %s: tri %d has inconsistent orientation\n", name, i);
      viol++;
    }
  }

  /* Edge-manifold: gather all triangle edges, sort, count run lengths. */
  ne = 3 * nt;
  edges = (edge *) malloc((ne > 0 ? ne : 1) * sizeof(edge));
  for (i = 0; i < nt; i++) {
    for (k = 0; k < 3; k++) {
      int u = CORNER(o, i, k), v = CORNER(o, i, (k + 1) % 3);
      edges[i * 3 + k].a = u < v ? u : v;
      edges[i * 3 + k].b = u < v ? v : u;
    }
  }
  qsort(edges, ne, sizeof(edge), edge_cmp);
  r = 0;
  while (r < ne) {
    int t = r + 1;
    while (t < ne && edges[t].a == edges[r].a && edges[t].b == edges[r].b) t++;
    if ((t - r) != 1 && (t - r) != 2) {
      if (reported++ < MAX_REPORT)
        printf("      [topology] %s: edge (%d,%d) shared by %d triangles\n",
               name, edges[r].a, edges[r].b, t - r);
      viol++;
    }
    r = t;
  }
  free(edges);
  return viol;
}

/* --- Invariant 2: neighbour-slot semantics ------------------------------- */

static int validate_neighbors(const char *name, struct triangulateio *o)
{
  int viol = 0, reported = 0;
  int nt = o->numberoftriangles, i, j, k;

  if (!o->neighborlist) {
    printf("      [neighbors] %s: no neighbour list produced\n", name);
    return 1;
  }

  for (i = 0; i < nt; i++) {
    for (j = 0; j < 3; j++) {
      int N = o->neighborlist[i * 3 + j];
      int u, v, found;

      if (N == -1) continue;                 /* boundary slot */
      if (N < 0 || N >= nt) {
        if (reported++ < MAX_REPORT)
          printf("      [neighbors] %s: tri %d slot %d -> %d out of range\n",
                 name, i, j, N);
        viol++; continue;
      }
      /* Slot j is opposite corner j, i.e. the edge (corner j+1, corner j+2). */
      u = CORNER(o, i, (j + 1) % 3);
      v = CORNER(o, i, (j + 2) % 3);
      if (!tri_has(o, N, u) || !tri_has(o, N, v)) {
        if (reported++ < MAX_REPORT)
          printf("      [neighbors] %s: tri %d slot %d -> %d does not share "
                 "the edge opposite corner %d\n", name, i, j, N, j);
        viol++; continue;
      }
      /* Symmetry: N must list i in exactly one slot, across the same edge. */
      found = 0;
      for (k = 0; k < 3; k++) {
        if (o->neighborlist[N * 3 + k] == i) {
          int u2 = CORNER(o, N, (k + 1) % 3), v2 = CORNER(o, N, (k + 2) % 3);
          found++;
          if (!((u2 == u && v2 == v) || (u2 == v && v2 == u))) {
            if (reported++ < MAX_REPORT)
              printf("      [neighbors] %s: tri %d<->%d adjacency edge "
                     "mismatch\n", name, i, N);
            viol++;
          }
        }
      }
      if (found != 1) {
        if (reported++ < MAX_REPORT)
          printf("      [neighbors] %s: tri %d -> %d not reciprocated "
                 "(found %d times)\n", name, i, N, found);
        viol++;
      }
    }
  }
  return viol;
}

/* --- Driver -------------------------------------------------------------- */

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

int main(void)
{
  int s, failed = 0;

  printf("Contract validation (structural invariants):\n");
  for (s = 0; s < scenario_count; s++) {
    struct triangulateio in, out;
    char flagbuf[64];
    int v;

    scenarios[s].build(&in);
    memset(&out, 0, sizeof(out));
    strncpy(flagbuf, scenarios[s].flags, sizeof(flagbuf) - 1);
    flagbuf[sizeof(flagbuf) - 1] = '\0';

    triangulate(flagbuf, &in, &out, NULL);

    v  = validate_topology(scenarios[s].name, &out);
    v += validate_neighbors(scenarios[s].name, &out);

    if (v == 0) {
      printf("  PASS  %-20s (%d tris)\n", scenarios[s].name,
             out.numberoftriangles);
    } else {
      printf("  FAIL  %-20s (%d violations)\n", scenarios[s].name, v);
      failed = 1;
    }
    free_output(&out);
  }
  return failed;
}

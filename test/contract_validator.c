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
/*  Run here against Triangle's own output for the shared scenarios, which    */
/*  proves the checks correct (Triangle must pass) and pins the invariants    */
/*  as an executable spec to port alongside the Java mesher.                  */
/*                                                                           */
/*  White-box harness: it #includes triangle.c to reach the robust incircle  */
/*  predicate (used by the Delaunay check) and the structs exactinit() needs. */
/*  Therefore it does NOT include scenarios.h (which would re-include          */
/*  triangle.h); the scenario table interface is redeclared locally to match. */
/*                                                                           */
/*  Invariants:                                                              */
/*    1. Topological validity   - manifold mesh, non-degenerate, consistent  */
/*                                orientation, indices in range.             */
/*    2. Neighbour-slot semantics - neighbour[i][j] across the edge opposite */
/*                                corner j; adjacency symmetric.             */
/*    3. Constrained Delaunay   - every interior NON-segment edge is locally */
/*                                Delaunay (empty circumcircle).             */
/*    4. Segment recovery       - every output segment is a real mesh edge.  */
/*    5. Holes & regions        - no triangle covers an input hole point;    */
/*                                region attribute is constant across every  */
/*                                non-segment interior edge.                 */
/*    6. Quality                - min angle >= the requested -q bound.       */
/*                                                                           */
/*  Usage:  contract_validator           (exits non-zero if any check fails) */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define REAL double
#include "triangle.c"

/* Local mirror of scenarios.h (cannot include it: it re-includes triangle.h). */
typedef struct {
  const char *name;
  const char *flags;
  void (*build)(struct triangulateio *in);
} scenario;
extern const scenario scenarios[];
extern const int scenario_count;

#define CORNER(o, i, k) ((o)->trianglelist[(i) * 3 + (k)])
#define MAX_REPORT 4
#define PI 3.14159265358979323846

/* mesh/behavior instances for the robust incircle predicate (sign only). */
static struct mesh vm;
static struct behavior vb;

static int tri_has(struct triangulateio *o, int t, int v)
{
  return CORNER(o, t, 0) == v || CORNER(o, t, 1) == v || CORNER(o, t, 2) == v;
}

/* --- undirected-edge set (sorted, for membership tests) ------------------ */

typedef struct { int a, b; } edge;

static void norm_edge(int u, int v, edge *e)
{
  e->a = u < v ? u : v;
  e->b = u < v ? v : u;
}

static int edge_cmp(const void *x, const void *y)
{
  const edge *p = (const edge *) x, *q = (const edge *) y;
  if (p->a != q->a) return p->a - q->a;
  return p->b - q->b;
}

static int edge_in(const edge *arr, int n, int u, int v)
{
  edge key;
  int lo = 0, hi = n - 1;
  norm_edge(u, v, &key);
  while (lo <= hi) {
    int m = (lo + hi) / 2, c = edge_cmp(&arr[m], &key);
    if (c == 0) return 1;
    if (c < 0) lo = m + 1; else hi = m - 1;
  }
  return 0;
}

static edge *build_segset(struct triangulateio *o, int *n)
{
  int ns = o->numberofsegments, i;
  edge *s = (edge *) malloc((ns > 0 ? ns : 1) * sizeof(edge));
  for (i = 0; i < ns; i++)
    norm_edge(o->segmentlist[2*i], o->segmentlist[2*i + 1], &s[i]);
  qsort(s, ns, sizeof(edge), edge_cmp);
  *n = ns;
  return s;
}

static edge *build_edgeset(struct triangulateio *o, int *n)
{
  int nt = o->numberoftriangles, ne = 3 * nt, i, k, w = 0;
  edge *e = (edge *) malloc((ne > 0 ? ne : 1) * sizeof(edge));
  for (i = 0; i < nt; i++)
    for (k = 0; k < 3; k++)
      norm_edge(CORNER(o, i, k), CORNER(o, i, (k + 1) % 3), &e[w++]);
  qsort(e, ne, sizeof(edge), edge_cmp);
  *n = ne;
  return e;
}

/* The corner of triangle t that is not u and not v (the apex of edge u-v). */
static int apex_of(struct triangulateio *o, int t, int u, int v)
{
  int k;
  for (k = 0; k < 3; k++) {
    int c = CORNER(o, t, k);
    if (c != u && c != v) return c;
  }
  return -1;
}

/* --- 1. topological validity --------------------------------------------- */

static int validate_topology(const char *name, struct triangulateio *o)
{
  int viol = 0, reported = 0;
  int nt = o->numberoftriangles, np = o->numberofpoints, i, k;
  int orient = 0;
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
    ax = o->pointlist[2*a]; ay = o->pointlist[2*a+1];
    bx = o->pointlist[2*b]; by = o->pointlist[2*b+1];
    cx = o->pointlist[2*c]; cy = o->pointlist[2*c+1];
    area2 = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    s = (area2 > 0) - (area2 < 0);
    if (s == 0) {
      if (reported++ < MAX_REPORT)
        printf("      [topology] %s: tri %d is degenerate\n", name, i);
      viol++;
    } else if (orient == 0) {
      orient = s;
    } else if (s != orient) {
      if (reported++ < MAX_REPORT)
        printf("      [topology] %s: tri %d inconsistent orientation\n", name, i);
      viol++;
    }
  }
  ne = 3 * nt;
  edges = (edge *) malloc((ne > 0 ? ne : 1) * sizeof(edge));
  for (i = 0; i < nt; i++)
    for (k = 0; k < 3; k++)
      norm_edge(CORNER(o, i, k), CORNER(o, i, (k + 1) % 3), &edges[i * 3 + k]);
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

/* --- 2. neighbour-slot semantics ----------------------------------------- */

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
      int N = o->neighborlist[i*3 + j], u, v, found;
      if (N == -1) continue;
      if (N < 0 || N >= nt) {
        if (reported++ < MAX_REPORT)
          printf("      [neighbors] %s: tri %d slot %d -> %d out of range\n",
                 name, i, j, N);
        viol++; continue;
      }
      u = CORNER(o, i, (j + 1) % 3);
      v = CORNER(o, i, (j + 2) % 3);
      if (!tri_has(o, N, u) || !tri_has(o, N, v)) {
        if (reported++ < MAX_REPORT)
          printf("      [neighbors] %s: tri %d slot %d -> %d not opposite "
                 "corner %d\n", name, i, j, N, j);
        viol++; continue;
      }
      found = 0;
      for (k = 0; k < 3; k++) {
        if (o->neighborlist[N*3 + k] == i) {
          int u2 = CORNER(o, N, (k + 1) % 3), v2 = CORNER(o, N, (k + 2) % 3);
          found++;
          if (!((u2 == u && v2 == v) || (u2 == v && v2 == u))) {
            if (reported++ < MAX_REPORT)
              printf("      [neighbors] %s: tri %d<->%d edge mismatch\n",
                     name, i, N);
            viol++;
          }
        }
      }
      if (found != 1) {
        if (reported++ < MAX_REPORT)
          printf("      [neighbors] %s: tri %d -> %d not reciprocated (%d)\n",
                 name, i, N, found);
        viol++;
      }
    }
  }
  return viol;
}

/* --- 3. constrained Delaunay (empty circumcircle off segments) ----------- */

static int validate_delaunay(const char *name, struct triangulateio *o,
                             const edge *seg, int nseg)
{
  int viol = 0, reported = 0;
  int nt = o->numberoftriangles, i, j;

  if (!o->neighborlist) return 0;          /* needs adjacency */
  for (i = 0; i < nt; i++) {
    for (j = 0; j < 3; j++) {
      int N = o->neighborlist[i*3 + j], u, v, ap;
      REAL *pa, *pb, *pc, *pd;
      if (N == -1 || N < i) continue;       /* each interior edge once */
      u = CORNER(o, i, (j + 1) % 3);
      v = CORNER(o, i, (j + 2) % 3);
      if (edge_in(seg, nseg, u, v)) continue;  /* constrained edge: exempt */
      ap = apex_of(o, N, u, v);
      if (ap < 0) continue;
      pa = &o->pointlist[2 * CORNER(o, i, 0)];
      pb = &o->pointlist[2 * CORNER(o, i, 1)];
      pc = &o->pointlist[2 * CORNER(o, i, 2)];
      pd = &o->pointlist[2 * ap];
      /* triangle i is CCW (checked in topology); >0 means ap strictly inside */
      if (incircle(&vm, &vb, pa, pb, pc, pd) > 0.0) {
        if (reported++ < MAX_REPORT)
          printf("      [delaunay] %s: edge (%d,%d) not locally Delaunay "
                 "(apex %d inside tri %d)\n", name, u, v, ap, i);
        viol++;
      }
    }
  }
  return viol;
}

/* --- 4. segment recovery (each output segment is a mesh edge) ------------- */

static int validate_segments(const char *name, struct triangulateio *o,
                             const edge *meshedges, int nedge)
{
  int viol = 0, reported = 0, i;
  if (!o->segmentlist) return 0;
  for (i = 0; i < o->numberofsegments; i++) {
    int u = o->segmentlist[2*i], v = o->segmentlist[2*i + 1];
    if (!edge_in(meshedges, nedge, u, v)) {
      if (reported++ < MAX_REPORT)
        printf("      [segments] %s: segment (%d,%d) is not a mesh edge\n",
               name, u, v);
      viol++;
    }
  }
  return viol;
}

/* --- 4b. segment coverage (each input segment covered by a chain) -------- */

/* Parameter t of P along segment a+t*(b-a) if P lies on it; returns 0 if not. */
static int on_segment_param(REAL ax, REAL ay, REAL dx, REAL dy, REAL len2,
                            REAL px, REAL py, double *t)
{
  REAL rx = px - ax, ry = py - ay;
  REAL cross = dx * ry - dy * rx;
  REAL tol = 1e-6;
  REAL tt;
  if (cross * cross > tol * tol * len2 * len2) {
    return 0;                                  /* off the line */
  }
  tt = (rx * dx + ry * dy) / len2;
  if (tt < -tol || tt > 1.0 + tol) {
    return 0;
  }
  *t = tt;
  return 1;
}

static int covers_unit_interval(double *lo, double *hi, int n)
{
  int i, j;
  double tol = 1e-6, reach = 0.0;
  if (n == 0) {
    return 0;
  }
  for (i = 1; i < n; i++) {                    /* insertion sort by lo */
    double kl = lo[i], kh = hi[i];
    for (j = i - 1; j >= 0 && lo[j] > kl; j--) {
      lo[j + 1] = lo[j];
      hi[j + 1] = hi[j];
    }
    lo[j + 1] = kl;
    hi[j + 1] = kh;
  }
  if (lo[0] > tol) {
    return 0;                                  /* gap at the start */
  }
  for (i = 0; i < n; i++) {
    if (lo[i] > reach + tol) {
      return 0;                                /* gap in the middle */
    }
    if (hi[i] > reach) {
      reach = hi[i];
    }
  }
  return reach >= 1.0 - tol;                    /* reaches the end */
}

#define MAX_PIECES 1024

static int validate_segment_coverage(const char *name, struct triangulateio *in,
                                     struct triangulateio *o)
{
  int viol = 0, reported = 0, s, i;
  for (s = 0; s < in->numberofsegments; s++) {
    int a = in->segmentlist[2 * s], b = in->segmentlist[2 * s + 1];
    double lo[MAX_PIECES], hi[MAX_PIECES];
    int n = 0;
    REAL ax = o->pointlist[2 * a], ay = o->pointlist[2 * a + 1];
    REAL dx = o->pointlist[2 * b] - ax, dy = o->pointlist[2 * b + 1] - ay;
    REAL len2 = dx * dx + dy * dy;
    if (len2 <= 0) {
      continue;
    }
    for (i = 0; i < o->numberofsegments && n < MAX_PIECES; i++) {
      int u = o->segmentlist[2 * i], w = o->segmentlist[2 * i + 1];
      double tu, tw;
      if (on_segment_param(ax, ay, dx, dy, len2,
                           o->pointlist[2 * u], o->pointlist[2 * u + 1], &tu)
          && on_segment_param(ax, ay, dx, dy, len2,
                              o->pointlist[2 * w], o->pointlist[2 * w + 1], &tw)) {
        lo[n] = tu < tw ? tu : tw;
        hi[n] = tu < tw ? tw : tu;
        n++;
      }
    }
    if (!covers_unit_interval(lo, hi, n)) {
      if (reported++ < MAX_REPORT)
        printf("      [coverage] %s: input segment (%d,%d) not covered by "
               "output subsegments\n", name, a, b);
      viol++;
    }
  }
  return viol;
}

/* --- 5a. holes (no triangle covers an input hole point) ------------------ */

static int orient_sign(REAL ax, REAL ay, REAL bx, REAL by, REAL px, REAL py)
{
  REAL d = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
  return (d > 0) - (d < 0);
}

static int validate_holes(const char *name, struct triangulateio *in,
                          struct triangulateio *o)
{
  int viol = 0, reported = 0, h, i;
  for (h = 0; h < in->numberofholes; h++) {
    REAL hx = in->holelist[2*h], hy = in->holelist[2*h + 1];
    for (i = 0; i < o->numberoftriangles; i++) {
      REAL *A = &o->pointlist[2 * CORNER(o, i, 0)];
      REAL *B = &o->pointlist[2 * CORNER(o, i, 1)];
      REAL *C = &o->pointlist[2 * CORNER(o, i, 2)];
      int s1 = orient_sign(A[0], A[1], B[0], B[1], hx, hy);
      int s2 = orient_sign(B[0], B[1], C[0], C[1], hx, hy);
      int s3 = orient_sign(C[0], C[1], A[0], A[1], hx, hy);
      if (s1 != 0 && s1 == s2 && s2 == s3) {       /* strictly inside */
        if (reported++ < MAX_REPORT)
          printf("      [holes] %s: hole point (%.6g,%.6g) is inside tri %d\n",
                 name, hx, hy, i);
        viol++;
      }
    }
  }
  return viol;
}

/* --- 5b. regions (attribute constant across non-segment interior edges) -- */

static int validate_regions(const char *name, struct triangulateio *o,
                            const edge *seg, int nseg)
{
  int viol = 0, reported = 0;
  int nt = o->numberoftriangles, na = o->numberoftriangleattributes, i, j, t;

  if (na < 1 || !o->triangleattributelist || !o->neighborlist) return 0;
  for (i = 0; i < nt; i++) {
    for (j = 0; j < 3; j++) {
      int N = o->neighborlist[i*3 + j], u, v;
      if (N == -1 || N < i) continue;
      u = CORNER(o, i, (j + 1) % 3);
      v = CORNER(o, i, (j + 2) % 3);
      if (edge_in(seg, nseg, u, v)) continue;   /* region boundary: may differ */
      for (t = 0; t < na; t++) {
        if (o->triangleattributelist[i*na + t] !=
            o->triangleattributelist[N*na + t]) {
          if (reported++ < MAX_REPORT)
            printf("      [regions] %s: attr differs across non-segment edge "
                   "(tri %d vs %d)\n", name, i, N);
          viol++;
          break;
        }
      }
    }
  }
  return viol;
}

/* --- 6. quality (min angle >= requested -q bound) ------------------------ */

static double tri_min_angle(REAL *A, REAL *B, REAL *C)
{
  REAL px[3], py[3];
  double best = 180.0;
  int k;
  px[0]=A[0]; py[0]=A[1]; px[1]=B[0]; py[1]=B[1]; px[2]=C[0]; py[2]=C[1];
  for (k = 0; k < 3; k++) {
    int q = (k + 1) % 3, r = (k + 2) % 3;
    double ux = px[q]-px[k], uy = py[q]-py[k];
    double vx = px[r]-px[k], vy = py[r]-py[k];
    double lu = sqrt(ux*ux + uy*uy), lv = sqrt(vx*vx + vy*vy);
    double cs, ang;
    if (lu == 0.0 || lv == 0.0) return 0.0;
    cs = (ux*vx + uy*vy) / (lu * lv);
    if (cs > 1.0) cs = 1.0; else if (cs < -1.0) cs = -1.0;
    ang = acos(cs) * 180.0 / PI;
    if (ang < best) best = ang;
  }
  return best;
}

static int validate_quality(const char *name, struct triangulateio *o,
                            const char *flags)
{
  int viol = 0, reported = 0, i;
  const char *q = strchr(flags, 'q');
  double bound, tol = 0.05;
  if (!q) return 0;                           /* no quality requested */
  q++;
  bound = ((*q >= '0' && *q <= '9') || *q == '.') ? strtod(q, NULL) : 20.0;
  for (i = 0; i < o->numberoftriangles; i++) {
    double a = tri_min_angle(&o->pointlist[2 * CORNER(o, i, 0)],
                             &o->pointlist[2 * CORNER(o, i, 1)],
                             &o->pointlist[2 * CORNER(o, i, 2)]);
    if (a < bound - tol) {
      if (reported++ < MAX_REPORT)
        printf("      [quality] %s: tri %d min angle %.3f < %.3f\n",
               name, i, a, bound);
      viol++;
    }
  }
  return viol;
}

/* --- driver -------------------------------------------------------------- */

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

  memset(&vm, 0, sizeof(vm));
  memset(&vb, 0, sizeof(vb));
  vb.noexact = 0;
  exactinit();

  printf("Contract validation (structural invariants):\n");
  for (s = 0; s < scenario_count; s++) {
    struct triangulateio in, out;
    char flagbuf[64];
    edge *seg, *meshedges;
    int nseg, nedge, v;

    scenarios[s].build(&in);
    memset(&out, 0, sizeof(out));
    strncpy(flagbuf, scenarios[s].flags, sizeof(flagbuf) - 1);
    flagbuf[sizeof(flagbuf) - 1] = '\0';
    triangulate(flagbuf, &in, &out, NULL);

    seg = build_segset(&out, &nseg);
    meshedges = build_edgeset(&out, &nedge);

    v  = validate_topology(scenarios[s].name, &out);
    v += validate_neighbors(scenarios[s].name, &out);
    v += validate_delaunay(scenarios[s].name, &out, seg, nseg);
    v += validate_segments(scenarios[s].name, &out, meshedges, nedge);
    v += validate_segment_coverage(scenarios[s].name, &in, &out);
    v += validate_holes(scenarios[s].name, &in, &out);
    v += validate_regions(scenarios[s].name, &out, seg, nseg);
    v += validate_quality(scenarios[s].name, &out, scenarios[s].flags);

    if (v == 0) {
      printf("  PASS  %-20s (%d tris)\n", scenarios[s].name,
             out.numberoftriangles);
    } else {
      printf("  FAIL  %-20s (%d violations)\n", scenarios[s].name, v);
      failed = 1;
    }
    free(seg);
    free(meshedges);
    free_output(&out);
  }
  return failed;
}

/*****************************************************************************/
/*                                                                           */
/*  phase_runner.c                                                           */
/*                                                                           */
/*  Emits Triangle's mesh at intermediate PHASE boundaries, so a port can be */
/*  validated phase by phase instead of only on the final mesh.  For each    */
/*  shared scenario it writes:                                               */
/*                                                                           */
/*    <name>.delaunay.txt  - Delaunay triangulation of the input points only */
/*                           (no segments): switches "nzQ".                  */
/*    <name>.cdt.txt        - constrained Delaunay with holes carved and      */
/*                           regions attributed, but NOT refined: "p[A]nzQ".  */
/*                                                                           */
/*  The final (refined) phase is the existing golden corpus.                 */
/*                                                                           */
/*  These are reference meshes for the port: each must satisfy the contract  */
/*  invariants appropriate to its phase (the Java side checks this).  Because */
/*  a reimplementation produces a different valid mesh, they are validated    */
/*  structurally, not matched byte-for-byte.                                  */
/*                                                                           */
/*  The dump format mirrors golden_runner.c so the same parser reads both.   */
/*                                                                           */
/*  Usage:  phase_runner <output-directory>                                  */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scenarios.h"

/* Deterministic text dump (mirrors golden_runner.c's dump). */
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
              io->neighborlist[i * 3], io->neighborlist[i * 3 + 1],
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

static void run(const char *outdir, const char *name, const char *suffix,
                const char *flags, struct triangulateio *in)
{
  struct triangulateio out;
  char flagbuf[64], path[1024];
  FILE *f;

  memset(&out, 0, sizeof(out));
  strncpy(flagbuf, flags, sizeof(flagbuf) - 1);
  flagbuf[sizeof(flagbuf) - 1] = '\0';
  triangulate(flagbuf, in, &out, NULL);

  snprintf(path, sizeof(path), "%s/%s.%s.txt", outdir, name, suffix);
  f = fopen(path, "wb");
  if (!f) {
    fprintf(stderr, "phase_runner: cannot write %s\n", path);
    exit(2);
  }
  dump(f, &out);
  fclose(f);
  printf("  %-26s -> %d pts, %d tris\n", path, out.numberofpoints,
         out.numberoftriangles);
  free_output(&out);
}

int main(int argc, char **argv)
{
  int s;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
    return 1;
  }

  printf("Generating phase meshes in '%s':\n", argv[1]);
  for (s = 0; s < scenario_count; s++) {
    struct triangulateio in;
    char cdtflags[16];

    /* Delaunay phase: the input points alone (segments/holes/regions cleared). */
    scenarios[s].build(&in);
    in.numberofsegments = 0;
    in.segmentlist = NULL;
    in.segmentmarkerlist = NULL;
    in.numberofholes = 0;
    in.holelist = NULL;
    in.numberofregions = 0;
    in.regionlist = NULL;
    run(argv[1], scenarios[s].name, "delaunay", "nzQ", &in);

    /* CDT phase: full PSLG, carved, regions attributed, but not refined. */
    scenarios[s].build(&in);
    strcpy(cdtflags, in.numberofregions > 0 ? "pAnzQ" : "pnzQ");
    run(argv[1], scenarios[s].name, "cdt", cdtflags, &in);
  }
  return 0;
}

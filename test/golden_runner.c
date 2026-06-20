/*****************************************************************************/
/*                                                                           */
/*  golden_runner.c                                                          */
/*                                                                           */
/*  Characterization ("golden") harness for Triangle.  For each shared       */
/*  scenario (see scenarios.c) it runs triangulate() through the library API */
/*  and writes a deterministic text dump of the output mesh to               */
/*  <outdir>/<name>.txt.                                                     */
/*                                                                           */
/*  The dumps captured from the current, known-good build are the baseline   */
/*  ("golden") files.  Re-running after a code change and diffing against     */
/*  the baseline catches ANY change in output - the tripwire for refactoring */
/*  or stripping.  Triangle is deterministic for a given input and switch     */
/*  set, so "identical output" is a meaningful, strict contract.             */
/*                                                                           */
/*  Usage:  golden_runner <output-directory>                                 */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scenarios.h"

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

int main(int argc, char **argv)
{
  int s;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
    return 1;
  }

  printf("Generating golden dumps in '%s':\n", argv[1]);
  for (s = 0; s < scenario_count; s++) {
    struct triangulateio in, out;
    char flagbuf[64];
    char path[1024];
    FILE *f;

    scenarios[s].build(&in);
    memset(&out, 0, sizeof(out));
    strncpy(flagbuf, scenarios[s].flags, sizeof(flagbuf) - 1);
    flagbuf[sizeof(flagbuf) - 1] = '\0';

    triangulate(flagbuf, &in, &out, NULL);

    snprintf(path, sizeof(path), "%s/%s.txt", argv[1], scenarios[s].name);
    f = fopen(path, "wb");
    if (!f) {
      fprintf(stderr, "golden_runner: cannot write %s\n", path);
      return 2;
    }
    dump(f, &out);
    fclose(f);
    printf("  %-20s -> %d pts, %d tris\n",
           scenarios[s].name, out.numberofpoints, out.numberoftriangles);

    free_output(&out);
  }
  return 0;
}

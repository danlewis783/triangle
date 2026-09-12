/* Exercise the Microsoft import library, DLL ABI, and DLL-owned allocation. */
#include <stdio.h>
#define REAL double
#define VOID void
#define ANSI_DECLARATORS
#include "triangle.h"

int main(void)
{
  struct triangulateio input = {0}, output = {0};
  double points[] = {0.0, 0.0, 1.0, 0.0, 0.0, 1.0};
  char switches[] = "zQ";
  int valid;
  input.pointlist = points;
  input.numberofpoints = 3;
  triangulate(switches, &input, &output, NULL);
  valid = output.numberofpoints == 3 && output.numberoftriangles == 1 &&
          output.numberofcorners == 3 && output.trianglelist != NULL;
  if (valid) {
    int a = output.trianglelist[0], b = output.trianglelist[1],
        c = output.trianglelist[2];
    valid = a >= 0 && a < 3 && b >= 0 && b < 3 && c >= 0 && c < 3 &&
            a != b && b != c && c != a;
  }
  trifree(output.pointlist);
  trifree(output.pointmarkerlist);
  trifree(output.trianglelist);
  if (!valid) {
    fprintf(stderr, "DLL returned an invalid triangle.\n");
    return 1;
  }
  puts("PASS: DLL triangulation and trifree.");
  return 0;
}

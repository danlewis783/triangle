/*****************************************************************************/
/*                                                                           */
/*  test_triangle.c                                                          */
/*                                                                           */
/*  Unit / characterization tests for Triangle, driven through its public    */
/*  library API (triangulate()).  Built against triangle.c compiled with     */
/*  -DTRILIBRARY.  Uses the Unity test framework (test/unity).               */
/*                                                                           */
/*  These are integration-style tests: Triangle is a monolithic file whose   */
/*  internal routines are not exposed, so we exercise it the intended way -   */
/*  feed a known input to triangulate() and assert on the output mesh.       */
/*                                                                           */
/*****************************************************************************/

#include <string.h>
#include <stdlib.h>

#include "unity.h"

/* Must match the precision Triangle was compiled with (double by default). */
#define REAL double
#include "triangle.h"

/* --- Unity per-test fixtures (like JUnit @BeforeEach / @AfterEach) -------- */

void setUp(void)    {}
void tearDown(void) {}

/* --- Helpers ------------------------------------------------------------- */

/* Wrap a caller-owned point array as a Triangle input.  memset guarantees   */
/* every other field (attributes, segments, holes, regions) is zero/NULL.    */
static void make_input(struct triangulateio *in, REAL *points, int npoints)
{
  memset(in, 0, sizeof(*in));
  in->numberofpoints = npoints;
  in->pointlist = points;
}

/* A fresh output struct: all pointers NULL so triangulate() allocates them. */
static void make_output(struct triangulateio *out)
{
  memset(out, 0, sizeof(*out));
}

/* Release the arrays triangulate() allocated in an output struct. */
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

/* --- Tests --------------------------------------------------------------- */

/* The Delaunay triangulation of the 4 corners of a unit square is two        */
/* triangles over the original 4 vertices (no Steiner points).                */
void test_square_delaunay_gives_two_triangles(void)
{
  REAL square[8] = { 0,0,  1,0,  1,1,  0,1 };
  struct triangulateio in, out;

  make_input(&in, square, 4);
  make_output(&out);

  triangulate("zQ", &in, &out, NULL);   /* z: index from 0, Q: quiet */

  TEST_ASSERT_EQUAL_INT(4, out.numberofpoints);
  TEST_ASSERT_EQUAL_INT(2, out.numberoftriangles);
  TEST_ASSERT_EQUAL_INT(3, out.numberofcorners); /* linear triangles */

  free_output(&out);
}

/* A triangulated square has 5 edges (4 boundary + 1 diagonal), and the mesh  */
/* must satisfy Euler's formula V - E + F = 2 (F counts the outer face).      */
void test_square_edges_satisfy_euler(void)
{
  REAL square[8] = { 0,0,  1,0,  1,1,  0,1 };
  struct triangulateio in, out;
  int V, E, F;

  make_input(&in, square, 4);
  make_output(&out);

  triangulate("zeQ", &in, &out, NULL);  /* e: produce an edge list */

  TEST_ASSERT_EQUAL_INT(5, out.numberofedges);

  V = out.numberofpoints;
  E = out.numberofedges;
  F = out.numberoftriangles + 1;        /* + the unbounded outer face */
  TEST_ASSERT_EQUAL_INT(2, V - E + F);

  free_output(&out);
}

/* Adding a point at the centre of the square yields a 4-triangle fan with    */
/* 8 edges (4 boundary + 4 spokes) over 5 vertices.                           */
void test_square_with_centre_gives_four_triangles(void)
{
  REAL pts[10] = { 0,0,  1,0,  1,1,  0,1,  0.5,0.5 };
  struct triangulateio in, out;

  make_input(&in, pts, 5);
  make_output(&out);

  triangulate("zeQ", &in, &out, NULL);

  TEST_ASSERT_EQUAL_INT(5, out.numberofpoints);
  TEST_ASSERT_EQUAL_INT(4, out.numberoftriangles);
  TEST_ASSERT_EQUAL_INT(8, out.numberofedges);

  free_output(&out);
}

/* A maximum-area constraint must force refinement: Triangle inserts Steiner  */
/* points, so the output has strictly more vertices and triangles than the    */
/* 4-corner / 2-triangle input.  (Exact counts are algorithm-dependent, so    */
/* we assert the qualitative property rather than a brittle magic number.)    */
void test_area_constraint_adds_steiner_points(void)
{
  REAL square[8] = { 0,0,  1,0,  1,1,  0,1 };
  struct triangulateio in, out;

  make_input(&in, square, 4);
  make_output(&out);

  triangulate("zq30a0.05Q", &in, &out, NULL); /* q: quality, a: max area */

  TEST_ASSERT_GREATER_THAN_INT(4, out.numberofpoints);
  TEST_ASSERT_GREATER_THAN_INT(2, out.numberoftriangles);

  free_output(&out);
}

/* --- Runner -------------------------------------------------------------- */

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_square_delaunay_gives_two_triangles);
  RUN_TEST(test_square_edges_satisfy_euler);
  RUN_TEST(test_square_with_centre_gives_four_triangles);
  RUN_TEST(test_area_constraint_adds_steiner_points);
  return UNITY_END();
}

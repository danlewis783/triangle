#ifndef TRIANGLE_SCENARIOS_H
#define TRIANGLE_SCENARIOS_H

/*****************************************************************************/
/*                                                                           */
/*  scenarios.h                                                              */
/*                                                                           */
/*  Shared input scenarios exercised by BOTH test harnesses:                 */
/*    - golden_runner.c     (byte-for-byte characterization of the output)   */
/*    - contract_validator.c (structural-invariant checks on the output)     */
/*                                                                           */
/*  Defining the inputs once means the two harnesses can never drift onto    */
/*  different geometry.  triangle.h has no include guard and needs REAL       */
/*  defined first, so this header does both; includers should include this   */
/*  header and NOT triangle.h directly.                                       */
/*                                                                           */
/*****************************************************************************/

#define REAL double
#include "triangle.h"

typedef struct {
  const char *name;
  const char *flags;                          /* triangulate() switch string */
  void (*build)(struct triangulateio *in);    /* zero-init then populate `in' */
} scenario;

extern const scenario scenarios[];
extern const int scenario_count;

#endif /* TRIANGLE_SCENARIOS_H */

// RUN: %dxc -T lib_6_4 %s | FileCheck %s

// This tests intrinsic overloads using parameter combinations that should fail, but didn't

CHECK: error: use of undeclared identifier 'fma'

export
double MismatchedIntrins(double d1, double d2, float f) {
  return fma(d1, d2, f);
}

CHECK: error: use of undeclared identifier 'fma'

export
double InvalidAfterValidIntrins(double d1, double d2, double d3,
                                float f1, float f2, float f3) {
  // This is valid and would let the next, invalid call slip through
  double ret = fma(d1, d2, d3);
  ret += fma(f1, f2, f3);
  return ret;
}

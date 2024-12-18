// RUN: %dxc -Tlib_6_9 -verify %s

export
vector<double, 3> doit(vector<double, 5> vec5) {
  vec5.x = 1; // expected-error {{Invalid swizzle 'x' on vector of over 4 elements.}}
  return vec5.xyw; // expected-error {{Invalid swizzle 'xyw' on vector of over 4 elements.}}
}

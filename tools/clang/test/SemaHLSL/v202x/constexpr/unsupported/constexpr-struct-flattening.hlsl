// RUN: %dxc -T lib_6_3 -HV 202x -Wno-c++14-extensions -verify %s

// These expressions are currently unsupported in constant evaluation but could
// be supported in the future.

struct Components {
  int2 first;
  int2 second;
};

constexpr int flattened_struct_cast() { // expected-error{{constexpr function never produces a constant expression}}
  Components value = { int2(1, 2), int2(3, 4) };
  return ((int4)value).w; // expected-note{{subexpression not valid in a constant expression}}
}

static_assert(flattened_struct_cast() == 4, "flattened struct-to-vector cast"); // expected-error{{static_assert expression is not an integral constant expression}}

// RUN: %dxc -T vs_6_0 %s | FileCheck %s

// Make sure the option for disabling the warning is printed.
// At the time of this writing, these may only be used with the pragma:
// #pragma dxc diagnostic ignored "-Wparentheses-equality"

float4 foo;

// This function has no output semantic on purpose in order to produce an error,
// otherwise, the warnings will not be captured in the output for FileCheck.
float main() {
  float4 x = foo;

// CHECK: equality comparison with extraneous parentheses
// CHECK: -Wparentheses-equality
  if ((x.y == 0))
  {

// CHECK: implicit truncation of vector type
// CHECK: -Wconversion
    return x;

  }
  return x.y;

}

// CHECK: error: Semantic must be defined

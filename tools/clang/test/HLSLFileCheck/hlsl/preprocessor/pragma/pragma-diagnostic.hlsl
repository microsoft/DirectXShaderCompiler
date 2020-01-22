// RUN: %dxc -T vs_6_0 %s | FileCheck %s

float4 foo;

// This function has no output semantic on purpose in order to produce an error,
// otherwise, the warnings will not be captured in the output for FileCheck.
float main() : OUT {
  float4 x = foo;

#pragma dxc diagnostic push

// CHECK-NOT: equality comparison with extraneous parentheses
#pragma dxc diagnostic ignored "-Wparentheses-equality"
  if ((x.y == 0))
  {

// CHECK: error: implicit truncation of vector type
#pragma dxc diagnostic error "-Wconversion"
    return x;

  }
  return x.y;

#pragma dxc diagnostic pop

}

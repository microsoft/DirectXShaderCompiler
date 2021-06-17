// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// Make sure 1-component vector in conditional compiles
// CHECK: = select i1

float1 A[2];
float B;

float main() : OUT {
  float1 foo = (A[0] > 0.0f) ? A[0] : B;
  return foo;
}

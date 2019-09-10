// RUN: %dxc -T lib_6_3 -exports main %s | FileCheck %s

// Regression test for GitHub #1943, where recursive struct member functions
// would crash the compiler.

// The SCCP pass replaces the recursive call with an undef value,
// which is why validation fails with a non-obvious error.

// CHECK: validation errors
// CHECK: Instructions should not read uninitialized value

struct S
{
  int func() { return func(); }
};

int main() : OUT
{
  S s;
  return s.func();
}
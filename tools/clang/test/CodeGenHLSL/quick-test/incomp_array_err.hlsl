// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Verify error on on incomplete array in a struct or class

typedef const int inta[];

static inta s_test1 = {1, 2, 3};

static int s_test2[] = { 4, 5, 6 };

// CHECK: incomp_array_err.hlsl:14:3: error: array dimensions of struct/class members must be explicit
struct foo1 {
  float4 member;
  inta a;
};

// CHECK: incomp_array_err.hlsl:19:3: error: array dimensions of struct/class members must be explicit
struct foo2 {
  int a[];
  float4 member;
};

// CHECK: incomp_array_err.hlsl:26:3: error: array dimensions of struct/class members must be explicit
class foo3 {
  float4 member;
  inta a;
};

// CHECK: incomp_array_err.hlsl:32:3: error: array dimensions of struct/class members must be explicit
class foo4 {
  float4 member;
  int a[];
};

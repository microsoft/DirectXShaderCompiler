// RUN: %dxc -T ps_6_0 -E main -HV 202x -spirv %s | FileCheck %s

static_assert(1 == 1, "translation unit");
static_assert(sizeof(float) == 4);

namespace N {
static_assert(2 + 2 == 4, "namespace");
}

struct S {
  static_assert(sizeof(float) == 4, "record");
  float Value;
};

float main() : SV_Target {
  static_assert(sizeof(S) == 4, "function");
  static_assert(1 < 2);
  return 0;
}

// CHECK: OpEntryPoint Fragment %main "main"

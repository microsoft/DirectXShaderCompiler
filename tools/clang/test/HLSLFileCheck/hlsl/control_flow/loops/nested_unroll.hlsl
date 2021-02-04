// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: @main

[RootSignature("")]
float main(float foo : FOO) : SV_Target {
  float result = 0;
  [unroll]
  for (uint i = 0; i < 2; i++) {
    [unroll]
    for (uint j = 0; j <= i; j++) {
      result += sin(j * i * foo);
    }
  }
  return result;
}


// RUN: %dxc -T ps_6_0 -E main -HV 202x -spirv -fcgl %s | FileCheck %s

// CHECK-DAG: OpDecorate [[FIRST:%[^ ]+]] Binding 3
// CHECK-DAG: OpDecorate [[SECOND:%[^ ]+]] Binding 4
// CHECK-DAG: [[FIRST]] = OpVariable {{%[^ ]+}} Uniform
// CHECK-DAG: [[SECOND]] = OpVariable {{%[^ ]+}} Uniform

namespace First {
cbuffer SceneConstants : register(b3) {
  float Value;
}
}

namespace Second {
cbuffer SceneConstants : register(b4) {
  float Value;
}
}

float4 main() : SV_Target {
  return First::Value + Second::Value;
}

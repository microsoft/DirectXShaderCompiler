// RUN: %dxc -T ps_6_0 -E main -HV 202x %s | FileCheck %s

// CHECK: ; cbuffer SceneConstants
// CHECK: ; cbuffer SceneConstants
// CHECK: ; Resource Bindings:
// CHECK-DAG: ; SceneConstants{{ +}}cbuffer{{ +}}NA{{ +}}NA{{ +}}CB0{{ +}}cb3{{ +}}1
// CHECK-DAG: ; SceneConstants{{ +}}cbuffer{{ +}}NA{{ +}}NA{{ +}}CB1{{ +}}cb4{{ +}}1

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

// RUN: %dxc -E main -HV 202x -T vs_6_2 %s | FileCheck %s

union s1 {
  int1x1 a;
  float4x2 b;
};

// CHECK: @dx.op.storeOutput.f32
float4x2 main() : OUT {
  s1 s;
  float4x2 mat = { 0.0f, 0.1f, 0.2f, 0.3f,
               0.4f, 0.5f, 0.6f, 0.7f };
  s.b = mat;
  return s.b;
}

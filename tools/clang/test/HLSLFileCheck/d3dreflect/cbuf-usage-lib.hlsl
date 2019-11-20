// RUN: %dxc -auto-binding-space 13 -T lib_6_3 -Vd -validator-version 0.0 %s | %D3DReflect %s | FileCheck %s

// Make sure usage flag is set properly for cbuffers used in libraries

// CHECK-NOT: CBufUnused

// CHECK: D3D12_SHADER_BUFFER_DESC: Name: CBuf1
// CHECK: Num Variables: 1
// CHECK: D3D12_SHADER_VARIABLE_DESC: Name: CBuf1
// CHECK: uFlags: 0x2
// CHECK: CBuffer: CBuf1

// CHECK: D3D12_SHADER_BUFFER_DESC: Name: CBuf0
// CHECK: Num Variables: 2
// CHECK: D3D12_SHADER_VARIABLE_DESC: Name: i1
// CHECK: uFlags: 0
// CHECK: D3D12_SHADER_VARIABLE_DESC: Name: f1
// CHECK: uFlags: 0x2
// CHECK: CBuffer: CBuf0

// CHECK: D3D12_SHADER_BUFFER_DESC: Name: CBuf2
// CHECK: Num Variables: 1
// CHECK: D3D12_SHADER_VARIABLE_DESC: Name: CBuf2
// CHECK: uFlags: 0x2
// CHECK: CBuffer: CBuf2

cbuffer CBuf0 {
  int i1;
  float f1;
}

struct CBStruct {
  int i2;
  float f2;
};

ConstantBuffer<CBStruct> CBuf1;
ConstantBuffer<CBStruct> CBufUnused;
ConstantBuffer<CBStruct> CBuf2[];

float unused_func() {
  return CBufUnused.i2;
}

export float foo() {
  return CBuf1.i2;
}

[shader("vertex")]
float main(int idx : IDX) : OUT {
  return f1 * CBuf2[idx].f2;
}

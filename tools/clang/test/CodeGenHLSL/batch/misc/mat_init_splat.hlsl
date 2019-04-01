// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 1.000000e+00)

struct MyStruct {
  float3x3 mat;
};

float main() : OUT {
  MyStruct st = (MyStruct)1;
  return st.mat[0].x;
}

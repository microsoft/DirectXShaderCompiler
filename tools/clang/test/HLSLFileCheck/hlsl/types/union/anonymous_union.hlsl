// RUN: %dxc -E main -HV 202x -T vs_6_2 %s | FileCheck %s

struct vec {
  union {
    struct {
      float x;
      float y;
      float z;
      float w;
    };
    struct {
      float r;
      float g;
      float b;
      float a;
    };
    float f[4];
  };
};

// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 2.000000e+00)
float main() : OUT {
  vec s;
  s.y = 1.0f;
  s.b = 1.0f;
  return s.y + s.b;
}

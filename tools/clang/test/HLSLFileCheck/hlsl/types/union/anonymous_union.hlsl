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

// CHECK: @dx.op.storeOutput.i32
float main() : OUT {
  vec s;
  s.x = 1.0f;
  s.b = 1.0f;
  return s.x + s.b;
}

// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// Make sure phi/select of handle in lib.
// CHECK-DAG: phi %dx.types.Handle
// CHECK: phi %dx.types.Handle
// CHECK: select i1 %{{[^,]+}}, %dx.types.Handle
// CHECK-DAG: phi %dx.types.Handle
// CHECK: phi %dx.types.Handle
// CHECK: select i1 %{{[^,]+}}, %dx.types.Handle
// CHECK-DAG: phi %dx.types.Handle
// CHECK: phi %dx.types.Handle

// Make sure get dimensions returns 24
// CHECK: ret i32 24

struct MyStruct {
  float2 a;
  int b;
  float3 c;
};

RWStructuredBuffer<MyStruct> a;
RWStructuredBuffer<MyStruct> b;
RWStructuredBuffer<MyStruct> c;

uint test(int i, int j, int m) {
  RWStructuredBuffer<MyStruct> buf = c;
  while (i > 9) {
     while (j < 4) {
        if (i < m)
          buf = b;
        buf[j].b = i;
        j++;
     }
     if (m > j)
       buf = a;
     buf[m].b = i;
     i--;
  }
  buf[i].b = j;
  uint dim = 0;
  uint stride = 0;
  buf.GetDimensions(dim, stride);
  return stride;
}
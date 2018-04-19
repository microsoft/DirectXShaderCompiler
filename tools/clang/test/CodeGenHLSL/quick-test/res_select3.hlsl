// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure phi of resource in lib.
// CHECK: phi %"class.RWStructuredBuffer

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
// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure no phi of resource.
// CHECK-NOT: phi %class.RWBuffer
// CHECK: phi %dx.types.Handle

RWBuffer<float4> a;
RWBuffer<float4> b;
RWBuffer<float4> c;

float4 test(int i, int j, int m) {
  RWBuffer<float4> buf = c;
  while (i > 9) {
     while (j < 4) {
        if (i < m)
          buf = b;
        buf[j] = i;
        j++;
     }
     if (m > j)
       buf = a;
     buf[m] = i;
     i--;
  }
  buf[i] = j;
  return j;
}
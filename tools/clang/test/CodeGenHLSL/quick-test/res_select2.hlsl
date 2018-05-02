// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// Make sure phi of resource for lib.
// CHECK: phi %"class.RWBuffer

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
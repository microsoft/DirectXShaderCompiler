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
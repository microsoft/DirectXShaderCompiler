// RUN: %dxc /Tps_6_2 -enable-16bit-types /Emain > %s | FileCheck %s
// CHECK: define void @main()
// CHECK: entry
// CHECK: %div.i = sdiv i32 %{{[0-9]+}}, %rem

int foo(int v0, int v1) { return v0 / v1; }
uint foo(uint v0, uint v1) { return v0 * v1; }
min16int foo(min16int v0, min16int v1) { return v0 + v1; }
min12int foo(min12int v0, min12int v1) { return v0 - v1; }
min16uint foo(min16uint v0, min16uint v1) { return v0 << v1; }

int main(int vi
            : A, uint vui
            : B, min16int vm16i
            : C, min12int vm12i
            : D, min16uint vm16ui
            : E) : SV_Target {
  return foo(vi, vi % 10);
}
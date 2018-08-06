// RUN: %dxc /Tps_6_0 /Emain > %s | FileCheck %s
// CHECK: define void @main()
// CHECK: entry
float main(float a : A) : SV_Target {
  return exp(a)[0];
}
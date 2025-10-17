// RUN: %dxc -E main -T ps_6_0 -HV 202x %s | FileCheck %s

// CHECK-NOT: "MyCBArray"

union MyStruct {
int a;
};
ConstantBuffer<MyStruct> MyCBArray[5] : register(b2, space5);
float4 main() : SV_Target {
  return 0;
}

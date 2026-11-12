// RUN: %dxc -T cs_6_5 -E CSMain -O0 -Zi -enable-16bit-types %s | FileCheck %s

// CHECK-LABEL:  @CSMain()
// CHECK:  phi float [ %{{.+}}, %{{.+}} ], [ %{{.+}}, %{{.+}} ], !dbg !{{[0-9]+}} ; line:15 col:12

RWBuffer<float> u0 : register(u0);
RWBuffer<float> u1 : register(u1);

[RootSignature("DescriptorTable(UAV(u0,numDescriptors=2))")]
[numthreads(64,1,1)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
  float my_var = u0[dtid.x];
  bool cond = my_var > 10;
  if (cond) {
    my_var *= 2;
  }
  u1[dtid.x] = my_var;
}

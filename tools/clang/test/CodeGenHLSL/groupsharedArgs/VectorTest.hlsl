// RUN: %dxc -E main -T cs_6_3 -HV 202x -fcgl %s | FileCheck %s

groupshared float4 SharedData;

// CHECK-LABEL: fn1
// CHECK: [[Tmp:%.*]] = alloca <1 x float>, align 4
// CHECK: store <1 x float> <float 5.000000e+00>, <1 x float>* [[Tmp]]
// CHECK: [[Z:%.*]] = load <1 x float>, <1 x float>* [[Tmp]]
// CHECK: [[Y:%.*]] = shufflevector <1 x float> [[Z]], <1 x float> undef, <4 x i32> zeroinitializer
// CHECK: store <4 x float> [[Y]], <4 x float> addrspace(3)* %Sh, align 4
void fn1(groupshared float4 Sh) {
  Sh = 5.0.xxxx;
}

[numthreads(4, 1, 1)]
// call void @"\01?fn1@@YAXAAV?$vector@M$03@@@Z"(<4 x float> addrspace(3)* dereferenceable(16) @"\01?SharedData@@3V?$vector@M$03@@A"), !dbg !25 ; line:13 col:3
void main(uint3 TID : SV_GroupThreadID) {
  fn1(SharedData);
}

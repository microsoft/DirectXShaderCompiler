// RUN: %dxc -T ps_6_0 -E main -HV 2021 -fcgl %s -spirv | FileCheck %s

struct MyData {
  uint a : 16;
  uint b : 16;
  uint c;
};

ConstantBuffer<MyData> input;

uint main() : SV_Target {
  // CHECK:      [[SOURCE:%[0-9]+]] = OpLoad {{%[^ ]+}} %input
  // CHECK-NEXT: [[BITFIELDS:%[0-9]+]] = OpCompositeExtract %uint [[SOURCE]] 0
  // CHECK-NEXT: [[C:%[0-9]+]] = OpCompositeExtract %uint [[SOURCE]] 1
  // CHECK-NEXT: [[VALUE:%[0-9]+]] = OpCompositeConstruct {{%[^ ]+}} [[BITFIELDS]] [[C]]
  // CHECK-NEXT: OpStore %local [[VALUE]]
  MyData local = input;

  // CHECK: [[C_PTR:%[0-9]+]] = OpAccessChain %_ptr_Function_uint %local %int_1
  // CHECK: [[C_VALUE:%[0-9]+]] = OpLoad %uint [[C_PTR]]
  // CHECK: OpReturnValue [[C_VALUE]]
  return local.c;
}

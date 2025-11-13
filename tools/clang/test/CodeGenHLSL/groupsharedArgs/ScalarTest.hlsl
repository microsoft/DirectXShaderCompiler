// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -HV 202x -fcgl %s | FileCheck %s

groupshared uint16_t SharedData;

// CHECK-LABEL: fn1
// CHECK: store i16 5, i16 addrspace(3)* %Sh, align 4
void fn1(groupshared uint16_t Sh) {
  Sh = 5;
}

[numthreads(4, 1, 1)]
// call void @"\01?fn1@@YAXAAG@Z"(i16 addrspace(3)* dereferenceable(2) @"\01?SharedData@@3GA")
void main(uint3 TID : SV_GroupThreadID) {
  fn1(SharedData);
}

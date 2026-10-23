// RUN: %dxc -T cs_6_0 -E main -HV 2021 -fcgl %s -spirv | FileCheck %s

// Flat conversions between a struct with bitfields and a vector, and splats of
// a scalar into such a struct, treat each bitfield as one element. Bits a and b
// share one SPIR-V member, so each must be extracted from or inserted into that
// member at its own offset and width.

struct Bits {
  uint a : 16;
  uint b : 16;
  uint c;
};

struct SignedBits {
  int a : 4;
  int b : 28;
};

RWStructuredBuffer<uint3> Out;
RWStructuredBuffer<Bits> BitsOut;
RWStructuredBuffer<int2> SignedOut;

[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
  Bits s;
  SignedBits sb;

// CHECK: [[s:%[0-9]+]] = OpLoad %Bits_0 %s
// CHECK: [[w0:%[0-9]+]] = OpCompositeExtract %uint [[s]] 0
// CHECK: [[a:%[0-9]+]] = OpBitFieldUExtract %uint [[w0]] %uint_0 %uint_16
// CHECK: [[w1:%[0-9]+]] = OpCompositeExtract %uint [[s]] 0
// CHECK: [[b:%[0-9]+]] = OpBitFieldUExtract %uint [[w1]] %uint_16 %uint_16
// CHECK: [[c:%[0-9]+]] = OpCompositeExtract %uint [[s]] 1
// CHECK:                 OpCompositeConstruct %v3uint [[a]] [[b]] [[c]]
  Out[0] = (uint3)s;

// CHECK: [[id:%[0-9]+]] = OpLoad %v3uint %id
// CHECK: [[x:%[0-9]+]] = OpCompositeExtract %uint [[id]] 0
// CHECK: [[y:%[0-9]+]] = OpCompositeExtract %uint [[id]] 1
// CHECK: [[z:%[0-9]+]] = OpCompositeExtract %uint [[id]] 2
// CHECK: [[xy:%[0-9]+]] = OpBitFieldInsert %uint [[x]] [[y]] %uint_16 %uint_16
// CHECK:                  OpCompositeConstruct %Bits_0 [[xy]] [[z]]
  BitsOut[0] = (Bits)id;

// CHECK: [[px:%[0-9]+]] = OpAccessChain %_ptr_Function_uint %id %int_0
// CHECK: [[x_0:%[0-9]+]] = OpLoad %uint [[px]]
// CHECK: [[xx:%[0-9]+]] = OpBitFieldInsert %uint [[x_0]] [[x_0]] %uint_16 %uint_16
// CHECK:                  OpCompositeConstruct %Bits_0 [[xx]] [[x_0]]
  BitsOut[1] = (Bits)id.x;

// CHECK: [[sb:%[0-9]+]] = OpLoad %SignedBits %sb
// CHECK: [[sw0:%[0-9]+]] = OpCompositeExtract %int [[sb]] 0
// CHECK: [[sa:%[0-9]+]] = OpBitFieldSExtract %int [[sw0]] %uint_0 %uint_4
// CHECK: [[sw1:%[0-9]+]] = OpCompositeExtract %int [[sb]] 0
// CHECK: [[sbb:%[0-9]+]] = OpBitFieldSExtract %int [[sw1]] %uint_4 %uint_28
// CHECK:                   OpCompositeConstruct %v2int [[sa]] [[sbb]]
  SignedOut[0] = (int2)sb;
}

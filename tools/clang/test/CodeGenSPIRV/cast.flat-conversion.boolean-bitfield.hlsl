// RUN: %dxc -T cs_6_0 -E main -HV 2021 -fcgl %s -spirv | FileCheck %s
// RUN: %dxc -T cs_6_0 -E main -HV 2021 -Od %s -spirv

// Boolean bitfields in external buffers have integer storage. Converting the
// containing struct to a scalar or to another struct must extract the bit as an
// integer before converting its value. The second run also validates the
// generated module.

struct Flag {
  bool value : 1;
};

struct PlainFlag {
  bool value;
};

StructuredBuffer<Flag> Flags : register(t0);
RWStructuredBuffer<uint3> Out : register(u0);

cbuffer Constants : register(b0) {
  Flag constantFlag;
};

[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
  // CHECK: [[BUFFER_WORD:%[0-9]+]] = OpCompositeExtract %uint {{%[0-9]+}} 0
  // CHECK: {{%[0-9]+}} = OpBitFieldUExtract %uint [[BUFFER_WORD]] %uint_0 %uint_1
  // CHECK: OpStore %fromBuffer
  uint fromBuffer = (uint)Flags[id.x];

  // CHECK: [[CONSTANT_WORD:%[0-9]+]] = OpCompositeExtract %uint {{%[0-9]+}} 0
  // CHECK: {{%[0-9]+}} = OpBitFieldUExtract %uint [[CONSTANT_WORD]] %uint_0 %uint_1
  // CHECK: OpStore %fromConstant
  uint fromConstant = (uint)constantFlag;

  // A cast to a struct with the same field types splits the source in
  // InitListHandler, which must also extract the bit as an integer.
  // CHECK: [[COPY_WORD:%[0-9]+]] = OpCompositeExtract %uint {{%[0-9]+}} 0
  // CHECK: {{%[0-9]+}} = OpBitFieldUExtract %uint [[COPY_WORD]] %uint_0 %uint_1
  // CHECK: OpStore %copy
  PlainFlag copy = (PlainFlag)constantFlag;

  Out[id.x] = uint3(fromBuffer, fromConstant, copy.value);
}

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

struct FlagPair {
  bool a : 1;
  bool b : 1;
};

StructuredBuffer<Flag> Flags : register(t0);
StructuredBuffer<FlagPair> FlagPairs : register(t1);
ByteAddressBuffer PackedFlags : register(t2);
RWStructuredBuffer<uint3> Out : register(u0);
RWStructuredBuffer<uint2> PairOut : register(u1);
RWStructuredBuffer<FlagPair> StoredPairs : register(u2);
RWByteAddressBuffer PackedOut : register(u3);
RWStructuredBuffer<FlagPair> UpdatedPairs : register(u4);
ConstantBuffer<FlagPair> PairConstant : register(b1);

// Sema accepts a local struct that holds a boolean bitfield struct as a member.
struct Holder {
  FlagPair pair;
};

cbuffer Constants : register(b0) {
  Flag constantFlag;
  FlagPair constantPair;
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

  // CHECK: [[PAIR_WORD_A:%[0-9]+]] = OpCompositeExtract %uint {{%[0-9]+}} 0
  // CHECK: [[PAIR_BIT_A:%[0-9]+]] = OpBitFieldUExtract %uint [[PAIR_WORD_A]] %uint_0 %uint_1
  // CHECK-NEXT: [[PAIR_BOOL_A:%[0-9]+]] = OpINotEqual %bool [[PAIR_BIT_A]] %uint_0
  // CHECK: [[PAIR_WORD_B:%[0-9]+]] = OpCompositeExtract %uint {{%[0-9]+}} 0
  // CHECK: [[PAIR_BIT_B:%[0-9]+]] = OpBitFieldUExtract %uint [[PAIR_WORD_B]] %uint_1 %uint_1
  // CHECK-NEXT: [[PAIR_BOOL_B:%[0-9]+]] = OpINotEqual %bool [[PAIR_BIT_B]] %uint_0
  // CHECK: [[PAIR_UINT_A:%[0-9]+]] = OpSelect %uint [[PAIR_BOOL_A]] %uint_1 %uint_0
  // CHECK: [[PAIR_UINT_B:%[0-9]+]] = OpSelect %uint [[PAIR_BOOL_B]] %uint_1 %uint_0
  // CHECK: {{%[0-9]+}} = OpCompositeConstruct %v2uint [[PAIR_UINT_A]] [[PAIR_UINT_B]]
  // CHECK: OpStore %fromPair
  uint2 fromPair = (uint2)FlagPairs[id.x];
  PairOut[id.x * 4] = fromPair;

  // CHECK: OpBitFieldUExtract %uint {{%[0-9]+}} %uint_0 %uint_1
  // CHECK-NEXT: [[BUFFER_BOOL_A:%[0-9]+]] = OpINotEqual %bool {{%[0-9]+}} %uint_0
  // CHECK: OpBitFieldUExtract %uint {{%[0-9]+}} %uint_1 %uint_1
  // CHECK-NEXT: [[BUFFER_BOOL_B:%[0-9]+]] = OpINotEqual %bool {{%[0-9]+}} %uint_0
  // CHECK: {{%[0-9]+}} = OpCompositeConstruct %v2bool [[BUFFER_BOOL_A]] [[BUFFER_BOOL_B]]
  // CHECK: OpStore %boolsFromPair
  bool2 boolsFromPair = (bool2)FlagPairs[id.x];
  PairOut[id.x * 4 + 1] = uint2(boolsFromPair);

  // CHECK: OpBitFieldUExtract %uint {{%[0-9]+}} %uint_0 %uint_1
  // CHECK-NEXT: [[CONSTANT_BOOL_A:%[0-9]+]] = OpINotEqual %bool {{%[0-9]+}} %uint_0
  // CHECK: OpBitFieldUExtract %uint {{%[0-9]+}} %uint_1 %uint_1
  // CHECK-NEXT: [[CONSTANT_BOOL_B:%[0-9]+]] = OpINotEqual %bool {{%[0-9]+}} %uint_0
  // CHECK: OpSelect %uint [[CONSTANT_BOOL_A]] %uint_1 %uint_0
  // CHECK: OpSelect %uint [[CONSTANT_BOOL_B]] %uint_1 %uint_0
  // CHECK: OpStore %fromConstantPair
  uint2 fromConstantPair = (uint2)constantPair;
  PairOut[id.x * 4 + 2] = fromConstantPair;

  // CHECK: OpBitFieldUExtract %uint {{%[0-9]+}} %uint_0 %uint_1
  // CHECK-NEXT: [[CBUFFER_BOOL_A:%[0-9]+]] = OpINotEqual %bool {{%[0-9]+}} %uint_0
  // CHECK: OpBitFieldUExtract %uint {{%[0-9]+}} %uint_1 %uint_1
  // CHECK-NEXT: [[CBUFFER_BOOL_B:%[0-9]+]] = OpINotEqual %bool {{%[0-9]+}} %uint_0
  // CHECK: {{%[0-9]+}} = OpCompositeConstruct %v2bool [[CBUFFER_BOOL_A]] [[CBUFFER_BOOL_B]]
  // CHECK: OpStore %boolsFromConstantPair
  bool2 boolsFromConstantPair = (bool2)constantPair;
  PairOut[id.x * 4 + 3] = uint2(boolsFromConstantPair);

  // Sema rejects local variables of these types, so the copies go directly
  // between buffers. Each copy moves the whole container as an integer.
  // CHECK: [[COPY_PAIR_WORD:%[0-9]+]] = OpCompositeExtract %uint {{%[0-9]+}} 0
  // CHECK-NOT: OpINotEqual
  // CHECK: OpCompositeConstruct {{%[^ ]+}} [[COPY_PAIR_WORD]]
  StoredPairs[id.x * 2] = constantPair;

  // CHECK: OpCompositeConstruct {{%[^ ]+}} {{%[0-9]+}}
  // CHECK-NOT: OpINotEqual
  // CHECK: OpStore
  StoredPairs[id.x * 2 + 1] = PackedFlags.Load<FlagPair>(id.x * 4);

  // CHECK: [[RAW_STORE:%[0-9]+]] = OpCompositeExtract %uint {{%[0-9]+}} 0
  // CHECK-NOT: OpSelect
  // CHECK: OpStore {{%[0-9]+}} [[RAW_STORE]]
  PackedOut.Store<FlagPair>(id.x * 4, FlagPairs[id.x]);

  // Reading one member compares the extracted bit with zero exactly once.
  // CHECK: [[DIRECT_BIT:%[0-9]+]] = OpBitFieldUExtract %uint {{%[0-9]+}} %uint_1 %uint_1
  // CHECK-NEXT: [[DIRECT_BOOL:%[0-9]+]] = OpINotEqual %bool [[DIRECT_BIT]] %uint_0
  // CHECK-NOT: OpINotEqual
  // CHECK: OpStore %directB [[DIRECT_BOOL]]
  bool directB = FlagPairs[id.x].b;

  // Writing one member inserts its integer value into the shared container.
  // CHECK: [[UPDATE_BOOL:%[0-9]+]] = OpLoad %bool %directB
  // CHECK: [[UPDATE_VALUE:%[0-9]+]] = OpSelect %uint [[UPDATE_BOOL]] %uint_1 %uint_0
  // CHECK: [[UPDATE_OLD:%[0-9]+]] = OpLoad %uint [[UPDATE_PTR:%[0-9]+]]
  // CHECK: [[UPDATE_NEW:%[0-9]+]] = OpBitFieldInsert %uint [[UPDATE_OLD]] [[UPDATE_VALUE]] %uint_0 %uint_1
  // CHECK: OpStore [[UPDATE_PTR]] [[UPDATE_NEW]]
  UpdatedPairs[id.x].a = directB;

  // Copies between local and buffer values move the whole container as an
  // integer in both directions.
  Holder holder;
  // CHECK: [[TO_LOCAL:%[0-9]+]] = OpCompositeExtract %uint {{%[0-9]+}} 0
  // CHECK-NOT: OpINotEqual
  // CHECK: OpCompositeConstruct {{%[^ ]+}} [[TO_LOCAL]]
  // CHECK: OpStore
  holder.pair = FlagPairs[id.x];

  // CHECK: [[TO_BUFFER:%[0-9]+]] = OpCompositeExtract %uint {{%[0-9]+}} 0
  // CHECK-NOT: OpINotEqual
  // CHECK: OpCompositeConstruct {{%[^ ]+}} [[TO_BUFFER]]
  // CHECK: OpStore
  UpdatedPairs[id.x + 1] = holder.pair;

  // A same-type conversion copies the container whole instead of splatting it
  // into each bitfield.
  // CHECK: [[SAME_WORD:%[0-9]+]] = OpCompositeExtract %uint {{%[0-9]+}} 0
  // CHECK-NOT: OpBitField
  // CHECK: OpCompositeConstruct {{%[^ ]+}} [[SAME_WORD]]
  // CHECK: OpStore
  UpdatedPairs[id.x + 2] = (FlagPair)PairConstant;
}

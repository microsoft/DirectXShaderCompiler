// RUN: %dxc -T cs_6_0 -E main -HV 2021 -fcgl %s -spirv | FileCheck %s
// RUN: %dxc -T cs_6_0 -E main -HV 2021 -Od %s -spirv
// RUN: %dxc -T cs_6_0 -E main -HV 2021 -fspv-debug=vulkan-with-source %s -spirv

// Boolean bitfields in values without an external layout use a uint container.
// Sema rejects local variables, parameters and static globals whose type
// declares a boolean bitfield directly, but temporaries, return values and
// structs nested in a local variable still reach code generation. Merged fields
// keep independent values through flat conversions, splats, initializer lists
// and member writes. The last two runs validate the module, the second one with
// debug information for the bitfield members.

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

// A boolean bitfield has a uint container, so it merges with a preceding uint
// bitfield.
struct Mixed {
  uint count : 3;
  bool flag : 1;
};

struct Outer {
  FlagPair pair;
  Mixed mixed;
};

StructuredBuffer<uint2> Inputs : register(t0);
RWStructuredBuffer<uint4> Out : register(u0);

FlagPair makePair(uint2 value) { return (FlagPair)value; }

[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
  uint2 input = Inputs[id.x];

  // Vector to struct, then struct to vector, through a temporary.
  // CHECK: [[VECTOR_A:%[0-9]+]] = OpSelect %uint {{%[0-9]+}} %uint_1 %uint_0
  // CHECK: [[VECTOR_B:%[0-9]+]] = OpSelect %uint {{%[0-9]+}} %uint_1 %uint_0
  // CHECK: [[VECTOR_WORD:%[0-9]+]] = OpBitFieldInsert %uint [[VECTOR_A]] [[VECTOR_B]] %uint_1 %uint_1
  // CHECK: [[VECTOR_PAIR:%[0-9]+]] = OpCompositeConstruct %FlagPair [[VECTOR_WORD]]
  // CHECK: OpCompositeExtract %uint [[VECTOR_PAIR]] 0
  // CHECK: OpBitFieldUExtract %uint {{%[0-9]+}} %uint_0 %uint_1
  // CHECK-NEXT: [[VECTOR_BOOL_A:%[0-9]+]] = OpINotEqual %bool {{%[0-9]+}} %uint_0
  // CHECK: OpBitFieldUExtract %uint {{%[0-9]+}} %uint_1 %uint_1
  // CHECK-NEXT: [[VECTOR_BOOL_B:%[0-9]+]] = OpINotEqual %bool {{%[0-9]+}} %uint_0
  // CHECK: [[VECTOR_UINT_A:%[0-9]+]] = OpSelect %uint [[VECTOR_BOOL_A]] %uint_1 %uint_0
  // CHECK: [[VECTOR_UINT_B:%[0-9]+]] = OpSelect %uint [[VECTOR_BOOL_B]] %uint_1 %uint_0
  // CHECK: OpCompositeConstruct %v2uint [[VECTOR_UINT_A]] [[VECTOR_UINT_B]]
  // CHECK: OpStore %fromVector
  uint2 fromVector = (uint2)(FlagPair)input;

  // CHECK: [[SPLAT_A:%[0-9]+]] = OpSelect %uint {{%[0-9]+}} %uint_1 %uint_0
  // CHECK: [[SPLAT_B:%[0-9]+]] = OpSelect %uint {{%[0-9]+}} %uint_1 %uint_0
  // CHECK: [[SPLAT_WORD:%[0-9]+]] = OpBitFieldInsert %uint [[SPLAT_A]] [[SPLAT_B]] %uint_1 %uint_1
  // CHECK: OpCompositeConstruct %FlagPair [[SPLAT_WORD]]
  // CHECK: OpStore %splat
  uint2 splat = (uint2)(FlagPair)input.x;

  // CHECK: [[PLAIN_BIT:%[0-9]+]] = OpBitFieldUExtract %uint {{%[0-9]+}} %uint_0 %uint_1
  // CHECK-NEXT: [[PLAIN_BOOL:%[0-9]+]] = OpINotEqual %bool [[PLAIN_BIT]] %uint_0
  // CHECK: OpCompositeConstruct %PlainFlag [[PLAIN_BOOL]]
  // CHECK: OpStore %plain
  PlainFlag plain = (PlainFlag)(Flag)input.x;

  // CHECK: [[OUTER_A:%[0-9]+]] = OpSelect %uint {{%[0-9]+}} %uint_1 %uint_0
  // CHECK: [[OUTER_B:%[0-9]+]] = OpSelect %uint {{%[0-9]+}} %uint_1 %uint_0
  // CHECK: [[OUTER_PAIR_WORD:%[0-9]+]] = OpBitFieldInsert %uint [[OUTER_A]] [[OUTER_B]] %uint_1 %uint_1
  // CHECK: [[OUTER_PAIR:%[0-9]+]] = OpCompositeConstruct %FlagPair [[OUTER_PAIR_WORD]]
  // CHECK: [[OUTER_FLAG:%[0-9]+]] = OpSelect %uint {{%[0-9]+}} %uint_1 %uint_0
  // CHECK: [[OUTER_MIXED_WORD:%[0-9]+]] = OpBitFieldInsert %uint {{%[0-9]+}} [[OUTER_FLAG]] %uint_3 %uint_1
  // CHECK: [[OUTER_MIXED:%[0-9]+]] = OpCompositeConstruct %Mixed [[OUTER_MIXED_WORD]]
  // CHECK: OpCompositeConstruct %Outer [[OUTER_PAIR]] [[OUTER_MIXED]]
  // CHECK: OpStore %outer
  Outer outer = {input.x != 0, input.x == 0, input.y, input.y != 0};

  // CHECK: [[WRITE_A:%[0-9]+]] = OpSelect %uint {{%[0-9]+}} %uint_1 %uint_0
  // CHECK: [[OLD_A:%[0-9]+]] = OpLoad %uint [[PTR_A:%[0-9]+]]
  // CHECK: [[NEW_A:%[0-9]+]] = OpBitFieldInsert %uint [[OLD_A]] [[WRITE_A]] %uint_0 %uint_1
  // CHECK: OpStore [[PTR_A]] [[NEW_A]]
  outer.pair.a = input.y != 0;

  // CHECK: [[WRITE_B:%[0-9]+]] = OpSelect %uint {{%[0-9]+}} %uint_1 %uint_0
  // CHECK: [[OLD_B:%[0-9]+]] = OpLoad %uint [[PTR_B:%[0-9]+]]
  // CHECK: [[NEW_B:%[0-9]+]] = OpBitFieldInsert %uint [[OLD_B]] [[WRITE_B]] %uint_1 %uint_1
  // CHECK: OpStore [[PTR_B]] [[NEW_B]]
  outer.pair.b = input.y == 0;

  // Reading one member compares the extracted bit with zero exactly once.
  // CHECK: [[READ_WORD:%[0-9]+]] = OpLoad %uint {{%[0-9]+}}
  // CHECK: [[READ_BIT:%[0-9]+]] = OpBitFieldUExtract %uint [[READ_WORD]] %uint_1 %uint_1
  // CHECK-NEXT: [[READ_BOOL:%[0-9]+]] = OpINotEqual %bool [[READ_BIT]] %uint_0
  // CHECK-NOT: OpINotEqual
  // CHECK: OpStore %memberB [[READ_BOOL]]
  bool memberB = outer.pair.b;

  // CHECK: [[MIXED_WORD:%[0-9]+]] = OpLoad %uint {{%[0-9]+}}
  // CHECK: [[MIXED_BIT:%[0-9]+]] = OpBitFieldUExtract %uint [[MIXED_WORD]] %uint_3 %uint_1
  // CHECK-NEXT: [[MIXED_BOOL:%[0-9]+]] = OpINotEqual %bool [[MIXED_BIT]] %uint_0
  // CHECK-NOT: OpINotEqual
  // CHECK: OpStore %mixedFlag [[MIXED_BOOL]]
  bool mixedFlag = outer.mixed.flag;

  // CHECK: OpBitFieldUExtract %uint {{%[0-9]+}} %uint_0 %uint_1
  // CHECK: OpBitFieldUExtract %uint {{%[0-9]+}} %uint_1 %uint_1
  // CHECK: OpBitFieldUExtract %uint {{%[0-9]+}} %uint_0 %uint_3
  // CHECK: [[OUTER_FLAG_BIT:%[0-9]+]] = OpBitFieldUExtract %uint {{%[0-9]+}} %uint_3 %uint_1
  // CHECK-NEXT: [[OUTER_FLAG_BOOL:%[0-9]+]] = OpINotEqual %bool [[OUTER_FLAG_BIT]] %uint_0
  // CHECK: OpSelect %uint [[OUTER_FLAG_BOOL]] %uint_1 %uint_0
  // CHECK: OpStore %outerValues
  uint4 outerValues = (uint4)outer;

  // CHECK: OpFunctionCall %FlagPair %makePair
  // CHECK: OpStore %returned
  uint2 returned = (uint2)makePair(input.yx);

  Outer copy = outer;
  copy.pair.b = input.x == 0;

  Out[id.x * 4] = uint4(fromVector, splat);
  Out[id.x * 4 + 1] = uint4(plain.value, memberB, mixedFlag, outer.mixed.count);
  Out[id.x * 4 + 2] = outerValues;
  Out[id.x * 4 + 3] = uint4(returned, (uint2)copy.pair);
}

// CHECK: %makePair = OpFunction %FlagPair
// CHECK: OpBitFieldInsert %uint {{%[0-9]+}} {{%[0-9]+}} %uint_1 %uint_1
// CHECK: OpReturnValue

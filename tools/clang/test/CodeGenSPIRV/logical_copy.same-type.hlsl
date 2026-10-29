// RUN: %dxc %s -spirv -fcgl -T cs_6_0 -E main -fspv-target-env=vulkan1.3 | FileCheck %s --implicit-check-not=OpCopyLogical
// RUN: %dxc %s -spirv -T cs_6_0 -E main -fspv-target-env=vulkan1.3 | FileCheck %s --check-prefix=OPT
// RUN: %dxc %s -spirv -T cs_6_0 -E main -fspv-target-env=vulkan1.1 | FileCheck %s --check-prefix=OPT

struct Foo {
  float4 a;
};

struct Padded {
  float a;
  float4 b;
};

struct Nested {
  Foo a;
  float4 b;
};

struct ArrayData {
  float4 values[2];
};

cbuffer Inputs {
  Foo foo;
  Padded padded;
  Nested nested;
  float4 values[2];
};

RWStructuredBuffer<Foo> outFoo : register(u0);
RWStructuredBuffer<Padded> outPadded : register(u1);
RWStructuredBuffer<Nested> outNested : register(u2);
RWStructuredBuffer<ArrayData> outArray : register(u3);

// OPT: OpEntryPoint GLCompute %main "main"

[numthreads(1, 1, 1)]
void main() {
  // The std140 and std430 layouts lower to identical SPIR-V types.
  // CHECK: [[FOO:%[0-9]+]] = OpLoad %Foo
  // CHECK: [[FOO_COPY:%[0-9]+]] = OpCopyObject %Foo [[FOO]]
  // CHECK: OpStore {{%[0-9]+}} [[FOO_COPY]]
  outFoo[0] = foo;

  // CHECK: [[PADDED:%[0-9]+]] = OpLoad %Padded
  // CHECK: [[PADDED_COPY:%[0-9]+]] = OpCopyObject %Padded [[PADDED]]
  // CHECK: OpStore {{%[0-9]+}} [[PADDED_COPY]]
  outPadded[0] = padded;

  // CHECK: [[NESTED:%[0-9]+]] = OpLoad %Nested
  // CHECK: [[NESTED_COPY:%[0-9]+]] = OpCopyObject %Nested [[NESTED]]
  // CHECK: OpStore {{%[0-9]+}} [[NESTED_COPY]]
  outNested[0] = nested;

  // CHECK: [[ARRAY:%[0-9]+]] = OpLoad %_arr_v4float_uint_2
  // CHECK: [[ARRAY_COPY:%[0-9]+]] = OpCopyObject %_arr_v4float_uint_2 [[ARRAY]]
  // CHECK: OpStore {{%[0-9]+}} [[ARRAY_COPY]]
  outArray[0].values = values;
}

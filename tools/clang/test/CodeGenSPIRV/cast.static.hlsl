// RUN: %dxc -T cs_6_0 -E numeric_casts -HV 202x -spirv %s | FileCheck %s -check-prefix=NUMERIC
// RUN: %dxc -T cs_6_0 -E matrix_casts -HV 202x -spirv %s | FileCheck %s -check-prefix=MATRIX
// RUN: %dxc -T cs_6_0 -E shape_casts -HV 202x -spirv %s | FileCheck %s -check-prefix=SHAPE
// RUN: %dxc -T cs_6_0 -E struct_casts -HV 202x -spirv %s | FileCheck %s -check-prefix=STRUCT
// RUN: %dxc -T cs_6_0 -E void_cast -HV 202x -spirv %s | FileCheck %s -check-prefix=VOID

template <typename T, typename U>
T convert(U value) {
  return static_cast<T>(value);
}

struct Base {
  float value;
};

struct Derived : Base {
  float extra;
};

RWStructuredBuffer<float4> float_output : register(u0);
RWStructuredBuffer<int4> int_output : register(u1);
RWStructuredBuffer<float2x2> matrix_output : register(u2);
RWStructuredBuffer<uint> uint_output : register(u3);
RWStructuredBuffer<Derived> derived_output : register(u4);
RWStructuredBuffer<Base> base_output : register(u5);

// NUMERIC: [[INPUT:%[0-9]+]] = OpLoad %v4float
// NUMERIC: [[INTS:%[0-9]+]] = OpConvertFToS %v4int [[INPUT]]
// NUMERIC: OpStore {{%[0-9]+}} [[INTS]]
// NUMERIC: [[UINT:%[0-9]+]] = OpConvertFToU %uint
// NUMERIC: [[FLOAT:%[0-9]+]] = OpConvertUToF %float [[UINT]]
// NUMERIC: [[BOOL:%[0-9]+]] = OpFOrdNotEqual %bool
// NUMERIC: [[BOOL_UINT:%[0-9]+]] = OpSelect %uint [[BOOL]] %uint_1 %uint_0
// NUMERIC: OpStore {{%[0-9]+}} [[BOOL_UINT]]
[numthreads(1, 1, 1)]
void numeric_casts(uint3 dispatch_id : SV_DispatchThreadID) {
  float4 input = float_output[dispatch_id.x];
  int4 integers = convert<int4>(input);
  int_output[dispatch_id.x] = integers;

  uint integer = static_cast<uint>(input.x);
  float_output[dispatch_id.x].x = static_cast<float>(integer);

  bool boolean = static_cast<bool>(input.y);
  uint_output[dispatch_id.x] = static_cast<uint>(boolean);
}

// MATRIX: OpConvertFToS %v2int
// MATRIX: OpConvertFToS %v2int
// MATRIX: OpConvertSToF %v2float
// MATRIX: OpConvertSToF %v2float
// MATRIX: OpStore
[numthreads(1, 1, 1)]
void matrix_casts(uint3 dispatch_id : SV_DispatchThreadID) {
  float2x2 input = matrix_output[dispatch_id.x];
  int2x2 integers = static_cast<int2x2>(input);
  matrix_output[dispatch_id.x] = static_cast<float2x2>(integers);
}

// SHAPE: [[INPUT:%[0-9]+]] = OpLoad %v4float
// SHAPE: [[X:%[0-9]+]] = OpCompositeExtract %float [[INPUT]] 0
// SHAPE: [[Y:%[0-9]+]] = OpCompositeExtract %float [[INPUT]] 1
// SHAPE: [[RESULT:%[0-9]+]] = OpCompositeConstruct %v4float [[X]] [[Y]] [[X]] [[X]]
// SHAPE: OpStore {{%[0-9]+}} [[RESULT]]
[numthreads(1, 1, 1)]
void shape_casts(uint3 dispatch_id : SV_DispatchThreadID) {
  float4 input = float_output[dispatch_id.x];
  float2 truncated = static_cast<float2>(input);
  float4 splat = static_cast<float4>(input.x);
  float_output[dispatch_id.x] = float4(truncated, splat.zw);
}

// STRUCT: [[INDEX:%[0-9]+]] = OpCompositeExtract %uint
// STRUCT: [[DERIVED_PTR:%[0-9]+]] = OpAccessChain %_ptr_Uniform_Derived %derived_output %int_0 [[INDEX]]
// STRUCT: [[BASE_PTR:%[0-9]+]] = OpAccessChain %_ptr_Uniform_Base [[DERIVED_PTR]] %uint_0
// STRUCT: [[VALUE_PTR:%[0-9]+]] = OpAccessChain %_ptr_Uniform_float [[BASE_PTR]] %uint_0
// STRUCT: [[VALUE:%[0-9]+]] = OpLoad %float [[VALUE_PTR]]
// STRUCT: [[BASE:%[0-9]+]] = OpCompositeConstruct %Base [[VALUE]]
// STRUCT: OpStore {{%[0-9]+}} [[BASE]]
[numthreads(1, 1, 1)]
void struct_casts(uint3 dispatch_id : SV_DispatchThreadID) {
  Derived derived = derived_output[dispatch_id.x];
  Base base = static_cast<Base>(derived);
  base_output[dispatch_id.x] = static_cast<Base>(base);
}

uint increment() {
  uint original;
  InterlockedAdd(uint_output[0], 1, original);
  return original;
}

// VOID: OpAtomicIAdd
[numthreads(1, 1, 1)]
void void_cast() {
  static_cast<void>(increment());
}

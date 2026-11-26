// RUN: %dxc -T ps_6_0 -E main -HV 202x %s | FileCheck %s -check-prefix=SIGNED
// RUN: %dxc -T ps_6_0 -E matrix_cast -HV 202x %s | FileCheck %s -check-prefix=MATRIX
// RUN: %dxc -T ps_6_0 -E unsigned_cast -HV 202x %s | FileCheck %s -check-prefix=UNSIGNED
// RUN: %dxc -T ps_6_0 -E boolean_cast -HV 202x %s | FileCheck %s -check-prefix=BOOL
// RUN: %dxc -T ps_6_0 -E shape_cast -HV 202x %s | FileCheck %s -check-prefix=SHAPE
// RUN: %dxc -T ps_6_0 -E struct_cast -HV 202x %s | FileCheck %s -check-prefix=STRUCT
// RUN: %dxc -T ps_6_0 -E void_cast -HV 202x %s | FileCheck %s -check-prefix=VOID

template <typename T, typename U>
T convert(U value) {
  return static_cast<T>(value);
}

// SIGNED: [[X:%.*]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0,
// SIGNED: [[Y:%.*]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1,
// SIGNED: [[Z:%.*]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2,
// SIGNED: [[W:%.*]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3,
float4 main(float4 value : VALUE) : SV_Target {
  // SIGNED: [[IX:%.*]] = fptosi float [[X]] to i32
  // SIGNED: [[IY:%.*]] = fptosi float [[Y]] to i32
  // SIGNED: [[IZ:%.*]] = fptosi float [[Z]] to i32
  // SIGNED: [[IW:%.*]] = fptosi float [[W]] to i32
  int4 integers = convert<int4>(value);
  // SIGNED: [[FX:%.*]] = sitofp i32 [[IX]] to float
  // SIGNED: [[FY:%.*]] = sitofp i32 [[IY]] to float
  // SIGNED: [[FZ:%.*]] = sitofp i32 [[IZ]] to float
  // SIGNED: [[FW:%.*]] = sitofp i32 [[IW]] to float
  // SIGNED: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float [[FX]])
  // SIGNED: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float [[FY]])
  // SIGNED: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float [[FZ]])
  // SIGNED: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float [[FW]])
  return static_cast<float4>(integers);
}

float4 matrix_cast(float4 value : VALUE) : SV_Target {
  // MATRIX: [[X:%.*]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0,
  // MATRIX: [[Y:%.*]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1,
  // MATRIX: [[Z:%.*]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2,
  // MATRIX: [[W:%.*]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3,
  float2x2 matrix = float2x2(value);
  // MATRIX: [[IX:%.*]] = fptosi float [[X]] to i32
  // MATRIX: [[IY:%.*]] = fptosi float [[Y]] to i32
  // MATRIX: [[IZ:%.*]] = fptosi float [[Z]] to i32
  // MATRIX: [[IW:%.*]] = fptosi float [[W]] to i32
  int2x2 integers = static_cast<int2x2>(matrix);
  // MATRIX: [[FX:%.*]] = sitofp i32 [[IX]] to float
  // MATRIX: [[FY:%.*]] = sitofp i32 [[IY]] to float
  // MATRIX: [[FZ:%.*]] = sitofp i32 [[IZ]] to float
  // MATRIX: [[FW:%.*]] = sitofp i32 [[IW]] to float
  float2x2 result = static_cast<float2x2>(integers);
  // MATRIX: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float [[FX]])
  // MATRIX: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float [[FY]])
  // MATRIX: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float [[FZ]])
  // MATRIX: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float [[FW]])
  return float4(result._11, result._12, result._21, result._22);
}

struct S {
  float value;
};

struct Derived : S {
  float extra;
};

// STRUCT: [[X:%.*]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0,
// STRUCT-NOT: call float @dx.op.loadInput.f32
// STRUCT: [[I:%.*]] = fptosi float [[X]] to i32
// STRUCT: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 [[I]])
int struct_cast(float2 value : VALUE) : SV_Target {
  Derived derived;
  derived.value = value.x;
  derived.extra = value.y;
  S base = static_cast<S>(derived);
  S copy = static_cast<S>(base);
  return static_cast<int>(copy.value);
}

// UNSIGNED: [[X:%.*]] = call float @dx.op.loadInput.f32
// UNSIGNED: [[I:%.*]] = fptoui float [[X]] to i32
// UNSIGNED: [[F:%.*]] = uitofp i32 [[I]] to float
// UNSIGNED: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float [[F]])
float unsigned_cast(float value : VALUE) : SV_Target {
  uint integer = static_cast<uint>(value);
  return static_cast<float>(integer);
}

// BOOL: [[X:%.*]] = call float @dx.op.loadInput.f32
// BOOL: [[B:%.*]] = fcmp fast une float [[X]], 0.000000e+00
// BOOL: [[I:%.*]] = zext i1 [[B]] to i32
// BOOL: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 [[I]])
uint boolean_cast(float value : VALUE) : SV_Target {
  bool boolean = static_cast<bool>(value);
  return static_cast<uint>(boolean);
}

// SHAPE: [[X:%.*]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0,
// SHAPE: [[Y:%.*]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1,
// SHAPE-NOT: call float @dx.op.loadInput.f32
// SHAPE: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float [[X]])
// SHAPE: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float [[Y]])
// SHAPE: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float [[X]])
// SHAPE: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float [[X]])
float4 shape_cast(float4 value : VALUE) : SV_Target {
  float2 truncated = static_cast<float2>(value);
  float4 splat = static_cast<float4>(value.x);
  return float4(truncated, splat.zw);
}

RWStructuredBuffer<uint> counter : register(u0);

uint increment() {
  uint original;
  InterlockedAdd(counter[0], 1, original);
  return original;
}

// VOID: call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle {{%.*}}, i32 0, i32 0, i32 0, i32 undef, i32 1)
// VOID: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 0)
uint void_cast() : SV_Target {
  static_cast<void>(increment());
  return 0;
}

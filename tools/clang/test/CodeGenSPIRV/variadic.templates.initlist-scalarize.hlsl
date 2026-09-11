// RUN: %dxc -E main -T ps_6_0 -HV 202x -fcgl %s -spirv | FileCheck %s

// Verify that pack expansions and equivalent explicit initializers produce
// the expected SPIR-V composite structure.

struct PairOf2 {
  float2 lo;
  float2 hi;
};

template <typename... Ts>
float4 PackVectorMixed(Ts... vals) {
  float4 v = { vals... };
  return v;
}
float4 ManualVectorMixed(float2 a, float2 b) {
  float4 v = { a, b };
  return v;
}

template <typename... Ts>
float2 PackArrayOverflow(Ts... vals) {
  float2 arr[2] = { vals... };
  return arr[0] + arr[1];
}
float2 ManualArrayOverflow(float a, float b, float c, float d) {
  float2 arr[2] = { a, b, c, d };
  return arr[0] + arr[1];
}

template <typename... Ts>
PairOf2 PackStruct(Ts... vals) {
  PairOf2 s = { vals... };
  return s;
}
PairOf2 ManualStruct(float2 a, float2 b) {
  PairOf2 s = { a, b };
  return s;
}

// CHECK-LABEL: %PackVectorMixed = OpFunction %v4float
// CHECK: [[PACK_V0:%[0-9]+]] = OpCompositeExtract %float {{%[0-9]+}} 0
// CHECK: [[PACK_V1:%[0-9]+]] = OpCompositeExtract %float {{%[0-9]+}} 1
// CHECK: [[PACK_V2:%[0-9]+]] = OpCompositeExtract %float {{%[0-9]+}} 0
// CHECK: [[PACK_V3:%[0-9]+]] = OpCompositeExtract %float {{%[0-9]+}} 1
// CHECK: OpCompositeConstruct %v4float [[PACK_V0]] [[PACK_V1]]
// CHECK-SAME: [[PACK_V2]] [[PACK_V3]]
// CHECK-LABEL: %ManualVectorMixed = OpFunction %v4float
// CHECK: [[MANUAL_V0:%[0-9]+]] = OpCompositeExtract %float {{%[0-9]+}} 0
// CHECK: [[MANUAL_V1:%[0-9]+]] = OpCompositeExtract %float {{%[0-9]+}} 1
// CHECK: [[MANUAL_V2:%[0-9]+]] = OpCompositeExtract %float {{%[0-9]+}} 0
// CHECK: [[MANUAL_V3:%[0-9]+]] = OpCompositeExtract %float {{%[0-9]+}} 1
// CHECK: OpCompositeConstruct %v4float [[MANUAL_V0]] [[MANUAL_V1]]
// CHECK-SAME: [[MANUAL_V2]] [[MANUAL_V3]]

// CHECK-LABEL: %PackArrayOverflow = OpFunction %v2float
// CHECK: [[PACK_A0:%[0-9]+]] = OpCompositeConstruct %v2float
// CHECK: [[PACK_A1:%[0-9]+]] = OpCompositeConstruct %v2float
// CHECK: OpCompositeConstruct %_arr_v2float_uint_2 [[PACK_A0]] [[PACK_A1]]
// CHECK-LABEL: %ManualArrayOverflow = OpFunction %v2float
// CHECK: [[MANUAL_A0:%[0-9]+]] = OpCompositeConstruct %v2float
// CHECK: [[MANUAL_A1:%[0-9]+]] = OpCompositeConstruct %v2float
// CHECK: OpCompositeConstruct %_arr_v2float_uint_2 [[MANUAL_A0]] [[MANUAL_A1]]

// CHECK-LABEL: %PackStruct = OpFunction %PairOf2
// CHECK: [[PACK_S0:%[0-9]+]] = OpLoad %v2float
// CHECK: [[PACK_S1:%[0-9]+]] = OpLoad %v2float
// CHECK: OpCompositeConstruct %PairOf2 [[PACK_S0]] [[PACK_S1]]
// CHECK-LABEL: %ManualStruct = OpFunction %PairOf2
// CHECK: [[MANUAL_S0:%[0-9]+]] = OpLoad %v2float
// CHECK: [[MANUAL_S1:%[0-9]+]] = OpLoad %v2float
// CHECK: OpCompositeConstruct %PairOf2 [[MANUAL_S0]] [[MANUAL_S1]]
float4 main(float4 inp : A) : SV_Target {
  float2 lo = inp.xy;
  float2 hi = inp.zw;

  float4 vp = PackVectorMixed(lo, hi);
  float4 vm = ManualVectorMixed(lo, hi);

  float2 op = PackArrayOverflow(inp.x, inp.y, inp.z, inp.w);
  float2 om = ManualArrayOverflow(inp.x, inp.y, inp.z, inp.w);

  PairOf2 sp = PackStruct(lo, hi);
  PairOf2 sm = ManualStruct(lo, hi);

  return vp + vm + float4(op + om, 0, 0) +
         float4(sp.lo + sm.lo, sp.hi + sm.hi);
}

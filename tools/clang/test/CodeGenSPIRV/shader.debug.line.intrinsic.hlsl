// RUN: %dxc -T ps_6_0 -E main -fspv-debug=vulkan -no-warnings

// CHECK:      [[file:%\d+]] = OpString
// CHECK:      [[src:%\d+]] = OpExtInst %void %2 DebugSource [[file]]

static int dest_i;

void main() {
  float2 v2f;
  uint4 v4i;
  float2x2 m2x2f;

// CHECK:                     DebugLine [[src]] %uint_16 %uint_22 %uint_3 %uint_14
// CHECK-NEXT: [[mod:%\d+]] = OpExtInst %ModfStructType {{%\d+}} ModfStruct {{%\d+}}
// CHECK-NEXT:     {{%\d+}} = OpCompositeExtract %v2float [[mod]] 1
  modf(v2f,
// CHECK:      [[mod:%\d+]] = OpConvertFToU %v2uint {{%\d+}}
// CHECK-NEXT:                DebugLine [[src]] %uint_22 %uint_22 %uint_8 %uint_8
// CHECK-NEXT: [[v4i:%\d+]] = OpLoad %v4uint %v4i
// CHECK-NEXT: [[v4i:%\d+]] = OpVectorShuffle %v4uint [[v4i]] [[mod]] 4 5 2 3
// CHECK-NEXT:                OpStore %v4i [[v4i]]
       v4i.xy);

// CHECK:                      DebugLine [[src]] %uint_29 %uint_29 %uint_9 %uint_9
// CHECK-NEXT: [[v4ix:%\d+]] = OpCompositeExtract %uint {{%\d+}} 0
// CHECK-NEXT:      {{%\d+}} = OpShiftLeftLogical %uint [[v4ix]] %uint_8
// CHECK-NEXT:      {{%\d+}} = OpShiftLeftLogical %uint [[v4ix]] %uint_16
// CHECK-NEXT:      {{%\d+}} = OpShiftLeftLogical %uint [[v4ix]] %uint_24
  v4i = msad4(v4i.x, v4i.xy, v4i);
// CHECK:      DebugLine [[src]] %uint_29 %uint_29 %uint_3 %uint_33
// CHECK-NEXT: OpStore %v4i {{%\d+}}

// CHECK:      DebugLine [[src]] %uint_35 %uint_35 %uint_23 %uint_67
// CHECK:      OpExtInst %v2float {{%\d+}} Fma
  /* comment */ v4i = mad(m2x2f, float2x2(v4i), float2x2(v2f, v2f));
// CHECK:      DebugLine [[src]] %uint_35 %uint_35 %uint_17 %uint_67
// CHECK-NEXT: OpStore %v4i

// CHECK:                 DebugLine [[src]] %uint_41 %uint_41 %uint_9 %uint_23
// CHECK-NEXT: {{%\d+}} = OpMatrixTimesVector %v2float
  v2f = mul(v2f, m2x2f);

// CHECK:                 DebugLine [[src]] %uint_45 %uint_45 %uint_11 %uint_26
// CHECK-NEXT: {{%\d+}} = OpDot %float
  v2f.x = dot(v4i.xy, v2f);

// CHECK:                 DebugLine [[src]] %uint_49 %uint_49 %uint_11 %uint_25
// CHECK-NEXT: {{%\d+}} = OpExtInst %v2float {{%\d+}} UnpackHalf2x16
  v2f.x = f16tof32(v4i.x);

// CHECK:                 DebugLine [[src]] %uint_53 %uint_53 %uint_11 %uint_20
// CHECK-NEXT: {{%\d+}} = OpDPdx %v2float
  m2x2f = ddx(m2x2f);

// CHECK:                       DebugLine [[src]] %uint_60 %uint_60 %uint_11 %uint_36
// CHECK-NEXT: [[fmod0:%\d+]] = OpFRem %v2float {{%\d+}} {{%\d+}}
// CHECK:                       DebugLine [[src]] %uint_60 %uint_60 %uint_11 %uint_36
// CHECK-NEXT: [[fmod1:%\d+]] = OpFRem %v2float {{%\d+}} {{%\d+}}
// CHECK-NEXT:       {{%\d+}} = OpCompositeConstruct %mat2v2float [[fmod0]] [[fmod1]]
  m2x2f = fmod(m2x2f, float2x2(v4i));

// CHECK:                     DebugLine [[src]] %uint_65 %uint_65 %uint_7 %uint_7
// CHECK-NEXT: [[v2f:%\d+]] = OpFOrdNotEqual %v2bool
// CHECK:          {{%\d+}} = OpAll %bool [[v2f]]
  if (all(v2f))
// CHECK:                      DebugLine [[src]] %uint_72 %uint_72 %uint_5 %uint_31
// CHECK:       [[sin:%\d+]] = OpExtInst %float {{%\d+}} Sin {{%\d+}}
// CHECK-NEXT:                 DebugLine [[src]] %uint_72 %uint_72 %uint_19 %uint_23
// CHECK-NEXT: [[v2fx:%\d+]] = OpAccessChain %_ptr_Function_float %v2f %int_1
// CHECK-NEXT:                 DebugLine [[src]] %uint_72 %uint_72 %uint_5 %uint_31
// CHECK-NEXT:                 OpStore [[v2fx]] [[sin]]
    sincos(v2f.x, v2f.y, v2f.x);

// CHECK:                 DebugLine [[src]] %uint_76 %uint_76 %uint_9 %uint_21
// CHECK-NEXT: {{%\d+}} = OpExtInst %v2float {{%\d+}} FClamp
  v2f = saturate(v2f);

// CHECK: DebugLine [[src]] %uint_80 %uint_80 %uint_26 %uint_33
// CHECK: OpAny
  /* comment */ dest_i = any(v4i);

// CHECK:                     DebugLine [[src]] %uint_87 %uint_87 %uint_35 %uint_47
// CHECK-NEXT: [[idx:%\d+]] = OpIAdd %uint
// CHECK:                     DebugLine [[src]] %uint_87 %uint_87 %uint_3 %uint_48
// CHECK-NEXT: [[v4i:%\d+]] = OpAccessChain %_ptr_Function_uint %v4i %int_0
// CHECK-NEXT:                OpStore [[v4i]] {{%\d+}}
  v4i.x = NonUniformResourceIndex(v4i.y + v4i.z);

// CHECK:      DebugLine [[src]] %uint_93 %uint_93 %uint_11 %uint_39
// CHECK-NEXT: OpImageSparseTexelsResident %bool
// CHECK:      DebugLine [[src]] %uint_93 %uint_93 %uint_3 %uint_39
// CHECK-NEXT: OpAccessChain %_ptr_Function_uint %v4i %int_2
  v4i.z = CheckAccessFullyMapped(v4i.w);

// CHECK:                     DebugLine [[src]] %uint_101 %uint_101 %uint_19 %uint_36
// CHECK-NEXT: [[add:%\d+]] = OpFAdd %v2float
// CHECK-NEXT:                DebugLine [[src]] %uint_101 %uint_101 %uint_12 %uint_39
// CHECK-NEXT:                OpBitcast %v2uint [[add]]
// CHECK-NEXT:                DebugLine [[src]] %uint_101 %uint_101 %uint_3 %uint_39
// CHECK-NEXT:                OpLoad %v4uint %v4i
  v4i.xy = asuint(m2x2f._m00_m11 + v2f);

// CHECK:      DebugLine [[src]] %uint_107 %uint_107 %uint_8 %uint_23
// CHECK-NEXT: OpFMul %v2float
// CHECK-NEXT: DebugLine [[src]] %uint_107 %uint_107 %uint_3 %uint_31
// CHECK-NEXT: OpFOrdLessThan %v2bool
  clip(v4i.yz * m2x2f._m00_m11);

  float4 v4f;

// CHECK:      DebugLine [[src]] %uint_115 %uint_115 %uint_9 %uint_37
// CHECK:      OpFMul %float
// CHECK-NEXT: OpCompositeConstruct %v4float
// CHECK-NEXT: OpConvertFToU %v4uint
  v4i = dst(v4f + 3 * v4f, v4f - v4f);

// CHECK:      DebugLine [[src]] %uint_121 %uint_121 %uint_17 %uint_43
// CHECK-NEXT: OpExtInst %float {{%\d+}} Exp2
// CHECK:      DebugLine [[src]] %uint_121 %uint_121 %uint_11 %uint_44
// CHECK-NEXT: OpBitcast %int
  v4i.x = asint(ldexp(v4f.x + v4f.y, v4f.w));

// CHECK:      DebugLine [[src]] %uint_129 %uint_129 %uint_19 %uint_31
// CHECK-NEXT: OpFAdd %float
// CHECK-NEXT: DebugLine [[src]] %uint_129 %uint_129 %uint_34 %uint_38
// CHECK-NEXT: OpAccessChain %_ptr_Function_float %v4f %int_3
// CHECK-NEXT: DebugLine [[src]] %uint_129 %uint_129 %uint_13 %uint_39
// CHECK-NEXT: OpExtInst %FrexpStructType {{%\d+}} FrexpStruct
  v4f = lit(frexp(v4f.x + v4f.y, v4f.w),
// CHECK:                     DebugLine [[src]] %uint_133 %uint_133 %uint_13 %uint_17
// CHECK-NEXT: [[v4f:%\d+]] = OpAccessChain %_ptr_Function_float %v4f %int_2
// CHECK-NEXT:                OpLoad %float [[v4f]]
            v4f.z,
// CHECK:                       DebugLine [[src]] %uint_140 %uint_140 %uint_13 %uint_58
// CHECK-NEXT: [[clamp:%\d+]] = OpExtInst %uint {{%\d+}} UClamp
// CHECK-NEXT:                  OpConvertUToF %float [[clamp]]
// CHECK-NEXT:                  DebugLine [[src]] %uint_129 %uint_140 %uint_9 %uint_59
// CHECK-NEXT:                  OpExtInst %float {{%\d+}} FMax %float_0
// CHECK-NEXT:                  OpExtInst %float {{%\d+}} FMin
            clamp(v4i.x + v4i.y, 2 * v4i.z, v4i.w - v4i.z));

// CHECK:                      DebugLine [[src]] %uint_146 %uint_146 %uint_33 %uint_59
// CHECK-NEXT: [[sign:%\d+]] = OpExtInst %v3float {{%\d+}} FSign
// CHECK-NEXT:                 DebugLine [[src]] %uint_146 %uint_146 %uint_38 %uint_38
// CHECK-NEXT:                 OpConvertFToS %v3int [[sign]]
  v4i = D3DCOLORtoUBYTE4(float4(sign(v4f.xyz - 2 * v4f.xyz),
// CHECK:      DebugLine [[src]] %uint_149 %uint_149 %uint_33 %uint_43
// CHECK-NEXT: OpExtInst %float {{%\d+}} FSign
                                sign(v4f.w)));
// CHECK:                     DebugLine [[src]] %uint_146 %uint_149 %uint_9 %uint_45
// CHECK-NEXT: [[arg:%\d+]] = OpVectorShuffle %v4float {{%\d+}} {{%\d+}} 2 1 0 3
// CHECK-NEXT:                OpVectorTimesScalar %v4float [[arg]]

// CHECK:      DebugLine [[src]] %uint_156 %uint_156 %uint_7 %uint_19
// CHECK-NEXT: OpIsNan %v4bool
  if (isfinite(v4f).x)
// CHECK:                     DebugLine [[src]] %uint_161 %uint_161 %uint_15 %uint_30
// CHECK-NEXT: [[rcp:%\d+]] = OpFDiv %v4float
// CHECK-NEXT:                DebugLine [[src]] %uint_161 %uint_161 %uint_11 %uint_31
// CHECK-NEXT:                OpExtInst %v4float {{%\d+}} Sin [[rcp]]
    v4f = sin(rcp(v4f / v4i.x));

// CHECK:                     DebugLine [[src]] %uint_168 %uint_168 %uint_20 %uint_47
// CHECK-NEXT:                OpExtInst %float {{%\d+}} Log2
// CHECK:                     DebugLine [[src]] %uint_168 %uint_168 %uint_11 %uint_48
// CHECK-NEXT: [[arg:%\d+]] = OpCompositeConstruct %v2float
// CHECK-NEXT:                OpExtInst %uint {{%\d+}} PackHalf2x16 [[arg]]
  v4i.x = f32tof16(log10(v2f.x * v2f.y + v4f.x));

// CHECK:      DebugLine [[src]] %uint_172 %uint_172 %uint_3 %uint_26
// CHECK-NEXT: OpTranspose %mat2v2float
  transpose(m2x2f + m2x2f);

// CHECK:                     DebugLine [[src]] %uint_180 %uint_180 %uint_25 %uint_42
// CHECK-NEXT: [[abs:%\d+]] = OpExtInst %float {{%\d+}} FAbs
// CHECK-NEXT:                DebugLine [[src]] %uint_180 %uint_180 %uint_20 %uint_43
// CHECK-NEXT:                OpExtInst %float {{%\d+}} Sqrt [[abs]]
// CHECK:      DebugLine [[src]] %uint_180 %uint_180 %uint_7 %uint_52
// CHECK-NEXT: OpExtInst %uint {{%\d+}} FindUMsb
  max(firstbithigh(sqrt(abs(v2f.x * v4f.w)) + v4i.x),
// CHECK:      DebugLine [[src]] %uint_183 %uint_183 %uint_7 %uint_16
// CHECK-NEXT: OpExtInst %float {{%\d+}} Cos
      cos(v4f.x));
// CHECK:      DebugLine [[src]] %uint_180 %uint_183 %uint_3 %uint_17
// CHECK-NEXT: OpExtInst %float {{%\d+}} FMax
}

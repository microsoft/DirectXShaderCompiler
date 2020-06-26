// Run: %dxc -T ps_6_0 -E main -Zi

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.intrinsic.hlsl

groupshared int dest_i;

void main() {
  float2 v2f;
  uint4 v4i;
  float2x2 m2x2f;

// CHECK:                     OpLine [[file]] 16 3
// CHECK-NEXT: [[mod:%\d+]] = OpExtInst %ModfStructType {{%\d+}} ModfStruct {{%\d+}}
// CHECK-NEXT:     {{%\d+}} = OpCompositeExtract %v2float [[mod]] 1
  modf(v2f,
// CHECK:                     OpLine [[file]] 22 8
// CHECK-NEXT: [[mod:%\d+]] = OpConvertFToU %v2uint {{%\d+}}
// CHECK-NEXT: [[v4i:%\d+]] = OpLoad %v4uint %v4i
// CHECK-NEXT: [[v4i:%\d+]] = OpVectorShuffle %v4uint [[v4i]] [[mod]] 4 5 2 3
// CHECK-NEXT:                OpStore %v4i [[v4i]]
       v4i.xy);

// CHECK:                      OpLine [[file]] 29 9
// CHECK-NEXT: [[v4ix:%\d+]] = OpCompositeExtract %uint {{%\d+}} 0
// CHECK-NEXT:      {{%\d+}} = OpShiftLeftLogical %uint [[v4ix]] %uint_8
// CHECK-NEXT:      {{%\d+}} = OpShiftLeftLogical %uint [[v4ix]] %uint_16
// CHECK-NEXT:      {{%\d+}} = OpShiftLeftLogical %uint [[v4ix]] %uint_24
  v4i = msad4(v4i.x, v4i.xy, v4i);
// CHECK:      OpLine [[file]] 29 3
// CHECK-NEXT: OpStore %v4i {{%\d+}}

// CHECK:      OpLine [[file]] 35 23
// CHECK-NEXT: OpExtInst %v2float {{%\d+}} Fma
  /* comment */ v4i = mad(m2x2f, float2x2(v4i), float2x2(v2f, v2f));
// CHECK:      OpLine [[file]] 35 17
// CHECK-NEXT: OpStore %v4i

// CHECK:                 OpLine [[file]] 41 9
// CHECK-NEXT: {{%\d+}} = OpMatrixTimesVector %v2float
  v2f = mul(v2f, m2x2f);

// CHECK:                 OpLine [[file]] 45 11
// CHECK-NEXT: {{%\d+}} = OpDot %float
  v2f.x = dot(v4i.xy, v2f);

// CHECK:                 OpLine [[file]] 49 11
// CHECK-NEXT: {{%\d+}} = OpExtInst %v2float {{%\d+}} UnpackHalf2x16
  v2f.x = f16tof32(v4i.x);

// CHECK:                 OpLine [[file]] 53 11
// CHECK-NEXT: {{%\d+}} = OpDPdx %v2float
  m2x2f = ddx(m2x2f);

// CHECK:                       OpLine [[file]] 60 11
// CHECK-NEXT: [[fmod0:%\d+]] = OpFRem %v2float {{%\d+}} {{%\d+}}
// CHECK:                       OpLine [[file]] 60 11
// CHECK-NEXT: [[fmod1:%\d+]] = OpFRem %v2float {{%\d+}} {{%\d+}}
// CHECK-NEXT:       {{%\d+}} = OpCompositeConstruct %mat2v2float [[fmod0]] [[fmod1]]
  m2x2f = fmod(m2x2f, float2x2(v4i));

// CHECK:                     OpLine [[file]] 65 7
// CHECK-NEXT: [[v2f:%\d+]] = OpFOrdNotEqual %v2bool
// CHECK-NEXT:     {{%\d+}} = OpAll %bool [[v2f]]
  if (all(v2f))
// CHECK:                      OpLine [[file]] 72 5
// CHECK:       [[sin:%\d+]] = OpExtInst %float {{%\d+}} Sin {{%\d+}}
// CHECK-NEXT:                 OpLine [[file]] 72 19
// CHECK-NEXT: [[v2fx:%\d+]] = OpAccessChain %_ptr_Function_float %v2f %int_1
// CHECK-NEXT:                 OpLine [[file]] 72 5
// CHECK-NEXT:                 OpStore [[v2fx]] [[sin]]
    sincos(v2f.x, v2f.y, v2f.x);

// CHECK:                 OpLine [[file]] 76 9
// CHECK-NEXT: {{%\d+}} = OpExtInst %v2float {{%\d+}} FClamp
  v2f = saturate(v2f);

// CHECK:      OpLine [[file]] 80 17
// CHECK-NEXT: OpAtomicCompareExchange %int %dest_i %uint_1 %uint_0 %uint_0
  /* comment */ InterlockedCompareStore(dest_i, v4i.x, v4i.y);

// CHECK:                     OpLine [[file]] 87 41
// CHECK-NEXT: [[idx:%\d+]] = OpIAdd %uint
// CHECK:                     OpLine [[file]] 87 3
// CHECK-NEXT: [[v4i:%\d+]] = OpAccessChain %_ptr_Function_uint %v4i %int_0
// CHECK-NEXT:                OpStore [[v4i]] {{%\d+}}
  v4i.x = NonUniformResourceIndex(v4i.y + v4i.z);

// CHECK:      OpLine [[file]] 93 11
// CHECK-NEXT: OpImageSparseTexelsResident %bool
// CHECK:      OpLine [[file]] 93 3
// CHECK-NEXT: OpAccessChain %_ptr_Function_uint %v4i %int_2
  v4i.z = CheckAccessFullyMapped(v4i.w);

// CHECK:                     OpLine [[file]] 101 34
// CHECK-NEXT: [[add:%\d+]] = OpFAdd %v2float
// CHECK-NEXT:                OpLine [[file]] 101 12
// CHECK-NEXT:                OpBitcast %v2uint [[add]]
// CHECK-NEXT:                OpLine [[file]] 101 3
// CHECK-NEXT:                OpLoad %v4uint %v4i
  v4i.xy = asuint(m2x2f._m00_m11 + v2f);

// CHECK:      OpLine [[file]] 107 15
// CHECK-NEXT: OpFMul %v2float
// CHECK-NEXT: OpLine [[file]] 107 3
// CHECK-NEXT: OpFOrdLessThan %v2bool
  clip(v4i.yz * m2x2f._m00_m11);

  float4 v4f;

// CHECK:      OpLine [[file]] 115 37
// CHECK-NEXT: OpFMul %float
// CHECK:      OpLine [[file]] 115 9
// CHECK-NEXT: OpConvertFToU %v4uint
  v4i = dst(v4f + 3 * v4f, v4f - v4f);

// CHECK:      OpLine [[file]] 121 17
// CHECK-NEXT: OpExtInst %float {{%\d+}} Exp2
// CHECK:      OpLine [[file]] 121 11
// CHECK-NEXT: OpBitcast %int
  v4i.x = asint(ldexp(v4f.x + v4f.y, v4f.w));

// CHECK:      OpLine [[file]] 129 25
// CHECK-NEXT: OpFAdd %float
// CHECK-NEXT: OpLine [[file]] 129 34
// CHECK-NEXT: OpAccessChain %_ptr_Function_float %v4f %int_3
// CHECK-NEXT: OpLine [[file]] 129 13
// CHECK-NEXT: OpExtInst %FrexpStructType {{%\d+}} FrexpStruct
  v4f = lit(frexp(v4f.x + v4f.y, v4f.w),
// CHECK:                     OpLine [[file]] 133 13
// CHECK-NEXT: [[v4f:%\d+]] = OpAccessChain %_ptr_Function_float %v4f %int_2
// CHECK-NEXT:                OpLoad %float [[v4f]]
            v4f.z,
// CHECK:                       OpLine [[file]] 140 13
// CHECK-NEXT: [[clamp:%\d+]] = OpExtInst %uint {{%\d+}} UClamp
// CHECK-NEXT:                  OpConvertUToF %float [[clamp]]
// CHECK-NEXT:                  OpLine [[file]] 129 9
// CHECK-NEXT:                  OpExtInst %float {{%\d+}} FMax %float_0
// CHECK-NEXT:                  OpExtInst %float {{%\d+}} FMin
            clamp(v4i.x + v4i.y, 2 * v4i.z, v4i.w - v4i.z));

// CHECK:                      OpLine [[file]] 146 33
// CHECK-NEXT: [[sign:%\d+]] = OpExtInst %v3float {{%\d+}} FSign
// CHECK-NEXT:                 OpLine [[file]] 146 38
// CHECK-NEXT:                 OpConvertFToS %v3int [[sign]]
  v4i = D3DCOLORtoUBYTE4(float4(sign(v4f.xyz - 2 * v4f.xyz),
// CHECK:      OpLine [[file]] 149 33
// CHECK-NEXT: OpExtInst %float {{%\d+}} FSign
                                sign(v4f.w)));
// CHECK:                     OpLine [[file]] 146 9
// CHECK-NEXT: [[arg:%\d+]] = OpVectorShuffle %v4float {{%\d+}} {{%\d+}} 2 1 0 3
// CHECK-NEXT:                OpVectorTimesScalar %v4float [[arg]]

// CHECK:      OpLine [[file]] 156 7
// CHECK-NEXT: OpIsNan %v4bool
  if (isfinite(v4f).x)
// CHECK:                     OpLine [[file]] 161 15
// CHECK-NEXT: [[rcp:%\d+]] = OpFDiv %v4float
// CHECK-NEXT:                OpLine [[file]] 161 11
// CHECK-NEXT:                OpExtInst %v4float {{%\d+}} Sin [[rcp]]
    v4f = sin(rcp(v4f / v4i.x));

// CHECK:                     OpLine [[file]] 168 20
// CHECK-NEXT:                OpExtInst %float {{%\d+}} Log2
// CHECK:                     OpLine [[file]] 168 11
// CHECK-NEXT: [[arg:%\d+]] = OpCompositeConstruct %v2float
// CHECK-NEXT:                OpExtInst %uint {{%\d+}} PackHalf2x16 [[arg]]
  v4i.x = f32tof16(log10(v2f.x * v2f.y + v4f.x));

// CHECK:      OpLine [[file]] 172 3
// CHECK-NEXT: OpTranspose %mat2v2float
  transpose(m2x2f + m2x2f);

// CHECK:                     OpLine [[file]] 180 25
// CHECK-NEXT: [[abs:%\d+]] = OpExtInst %float {{%\d+}} FAbs
// CHECK-NEXT:                OpLine [[file]] 180 20
// CHECK-NEXT:                OpExtInst %float {{%\d+}} Sqrt [[abs]]
// CHECK:      OpLine [[file]] 180 7
// CHECK-NEXT: OpExtInst %uint {{%\d+}} FindUMsb
  max(firstbithigh(sqrt(abs(v2f.x * v4f.w)) + v4i.x),
// CHECK:      OpLine [[file]] 183 7
// CHECK-NEXT: OpExtInst %float {{%\d+}} Cos
      cos(v4f.x));
// CHECK:      OpLine [[file]] 180 3
// CHECK-NEXT: OpExtInst %float {{%\d+}} FMax
}

// Run: %dxc -T ps_6_0 -E main -Zi

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.intrinsic.hlsl

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.intrinsic.hlsl

ConsumeStructuredBuffer<bool2> consume_v2bool;
AppendStructuredBuffer<float2> append_v2float;
ByteAddressBuffer byte_addr;
RWTexture2D<int3> rw_tex;
SamplerState sam;
Texture2D<float4> t2f4;
RWByteAddressBuffer rw_byte;

// Note that preprocessor prepends a "#line 1 ..." line to the whole file and
// the compliation sees line numbers incremented by 1.

void main() {
  float2 v2f;
  uint4 v4i;
  float2x2 m2x2f;

// CHECK:                     OpLine [[file]] 30 18
// CHECK-NEXT: [[cnt:%\d+]] = OpAccessChain %_ptr_Uniform_int %counter_var_append_v2float %uint_0
// CHECK:                     OpLine [[file]] 30 3
// CHECK-NEXT: [[app:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %append_v2float %uint_0
  append_v2float.Append(
// CHECK:                     OpLine [[file]] 35 22
// CHECK-NEXT: [[cnt:%\d+]] = OpAccessChain %_ptr_Uniform_int %counter_var_consume_v2bool %uint_0
// CHECK:                     OpLine [[file]] 35 7
// CHECK-NEXT: [[app:%\d+]] = OpAccessChain %_ptr_Uniform_v2uint %consume_v2bool %uint_0
      consume_v2bool.Consume());
// CHECK:      OpLine [[file]] 30 18
// CHECK-NEXT: OpStore

// CHECK:                     OpLine [[file]] 42 3
// CHECK-NEXT: [[mod:%\d+]] = OpExtInst %ModfStructType {{%\d+}} ModfStruct {{%\d+}}
// CHECK-NEXT:     {{%\d+}} = OpCompositeExtract %v2float [[mod]] 1
  modf(v2f,
// CHECK:                     OpLine [[file]] 48 8
// CHECK-NEXT: [[mod:%\d+]] = OpConvertFToU %v2uint {{%\d+}}
// CHECK-NEXT: [[v4i:%\d+]] = OpLoad %v4uint %v4i
// CHECK-NEXT: [[v4i:%\d+]] = OpVectorShuffle %v4uint [[v4i]] [[mod]] 4 5 2 3
// CHECK-NEXT:                OpStore %v4i [[v4i]]
       v4i.xy);

// CHECK:                    OpLine [[file]] 54 13
// CHECK-NEXT: [[ba:%\d+]] = OpAccessChain %_ptr_Uniform_uint %byte_addr %uint_0 {{%\d+}}
// CHECK-NEXT:               OpLine [[file]] 54 23
// CHECK-NEXT:    {{%\d+}} = OpLoad %uint [[ba]]
  v4i.xyz = byte_addr.Load3(v4i.x);
// CHECK:                 OpLine [[file]] 54 13
// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %v3uint

// CHECK:                      OpLine [[file]] 60 10
// CHECK-NEXT: [[size:%\d+]] = OpImageQuerySize %v2uint {{%\d+}}
  rw_tex.GetDimensions(
// CHECK-NEXT:                 OpLine [[file]] 63 7
// CHECK-NEXT:    [[w:%\d+]] = OpCompositeExtract %uint [[size]] 0
// CHECK-NEXT: [[v4ix:%\d+]] = OpAccessChain %_ptr_Function_uint %v4i %int_0
// CHECK-NEXT:                 OpStore [[v4ix]] [[w]]
      v4i.x, v4i.y);

// CHECK:                     OpLine [[file]] 70 3
// CHECK-NEXT: [[len:%\d+]] = OpArrayLength %uint %byte_addr 0
// CHECK-NEXT:     {{%\d+}} = OpIMul %uint [[len]] %uint_4
  byte_addr.GetDimensions(v4i.z);

// CHECK:                     OpLine [[file]] 75 14
// CHECK-NEXT: [[sam:%\d+]] = OpSampledImage %type_sampled_image {{%\d+}} {{%\d+}}
// CHECK-NEXT:     {{%\d+}} = OpImageGather %v4float [[sam]] {{%\d+}} %int_0 Offset
  v4i = t2f4.GatherRed(sam, v2f, v4i.xy);

// CHECK:                     OpLine [[file]] 82 9
// CHECK-NEXT: [[v4ix:%\d]] = OpCompositeExtract %uint {{%\d+}} 0
// CHECK-NEXT:     {{%\d+}} = OpShiftLeftLogical %uint [[v4ix]] %uint_8
// CHECK-NEXT:     {{%\d+}} = OpShiftLeftLogical %uint [[v4ix]] %uint_16
// CHECK-NEXT:     {{%\d+}} = OpShiftLeftLogical %uint [[v4ix]] %uint_24
  v4i = msad4(v4i.x, v4i.xy, v4i);
// CHECK:                 OpLine [[file]] 82 33
// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %v4uint

// CHECK:                 OpLine [[file]] 88 9
// CHECK-NEXT: {{%\d+}} = OpExtInst %v2float {{%\d+}} Fma
  v4i = mad(m2x2f, float2x2(v4i), float2x2(v2f, v2f));
// CHECK:      OpLine [[file]] 88 3
// CHECK-NEXT: OpStore %v4i

// TODO(jaebaek): Add "OpLine 94 9" here.
// CHECK: {{%\d+}} = OpMatrixTimesVector %v2float
  v2f = mul(v2f, m2x2f);

// TODO(jaebaek): Add "OpLine 98 11" here.
// CHECK: {{%\d+}} = OpDot %float
  v2f.x = dot(v4i.xy, v2f);

// CHECK:                 OpLine [[file]] 102 12
// CHECK-NEXT: {{%\d+}} = OpExtInst %v2float {{%\d+}} UnpackHalf2x16
  half h = f16tof32(v4i.x);

// CHECK:                 OpLine [[file]] 106 11
// CHECK-NEXT: {{%\d+}} = OpDPdx %v2float
  m2x2f = ddx(m2x2f);

// CHECK:                      OpLine [[file]] 113 23
// CHECK-NEXT:  [[rhs:%\d+]] = OpCompositeExtract %v2float {{%\d+}} 1
// CHECK-NEXT:                 OpLine [[file]] 113 11
// CHECK-NEXT: [[fmod:%\d+]] = OpFMod %v2float {{%\d+}} [[rhs]]
// CHECK-NEXT:      {{%\d+}} = OpCompositeConstruct %mat2v2float {{%\d+}} [[fmod]]
  m2x2f = fmod(m2x2f, float2x2(v4i));

// CHECK:                     OpLine [[file]] 120 11
// CHECK-NEXT: [[add:%\d+]] = OpAtomicIAdd %uint {{%\d+}} %uint_1 %uint_0 %uint_42
// CHECK-NEXT:                OpLine [[file]] 120 34
// CHECK-NEXT: [[v4i:%\d+]] = OpAccessChain %_ptr_Function_uint %v4i %int_0
// CHECK-NEXT:                OpStore [[v4i]] [[add]]
  rw_byte.InterlockedAdd(16, 42, v4i.x);

// CHECK:                 OpLine [[file]] 125 7
// TODO(jaebaek): Add "OpFOrdNotEqual %v2bool .." here
// CHECK-NEXT: {{%\d+}} = OpAll %bool {{%\d+}}
  if (all(v2f))
// CHECK:                      OpLine [[file]] 132 5
// CHECK-NEXT:  [[sin:%\d+]] = OpExtInst %float {{%\d+}} Sin {{%\d+}}
// CHECK-NEXT:                 OpLine [[file]] 132 19
// CHECK-NEXT: [[v2fx:%\d+]] = OpAccessChain %_ptr_Function_float %v2f %int_1
// CHECK-NEXT:                 OpLine [[file]] 132 5
// CHECK-NEXT:                 OpStore [[v2fx]] [[sin]]
    sincos(v2f.x, v2f.y, v2f.x);

// CHECK:                     OpLine [[file]] 138 18
// CHECK-NEXT: [[v2f:%\d+]] = OpLoad %v2float %v2f
// CHECK-NEXT:                OpLine [[file]] 138 9
// CHECK-NEXT:     {{%\d+}} = OpExtInst %v2float {{%\d+}} FClamp [[v2f]]
  v2f = saturate(v2f);
}

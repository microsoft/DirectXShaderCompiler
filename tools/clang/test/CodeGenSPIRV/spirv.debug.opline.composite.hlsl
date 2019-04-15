// Run: %dxc -T ps_6_0 -E main -Zi

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.composite.hlsl

struct int4_bool_float3 {
  int4 a;
  bool b;
  float3 c;
};

int4_bool_float3 test_struct() {
  int4_bool_float3 x;
  return x;
}

cbuffer CONSTANTS {
  int4_bool_float3 y;
};

RWTexture2D<int3> z;

// Note that preprocessor prepends a "#line 1 ..." line to the whole file,
// the compliation sees line numbers incremented by 1.

void main() {
  int4 a = {
      float2(1, 0),
// CHECK:                   OpConvertFToS %int %float_0
// CHECK-NEXT:              OpLine [[file]] 34 7
// CHECK-NEXT: [[z:%\d+]] = OpCompositeExtract %float {{%\d+}} 0
// CHECK-NEXT: [[x:%\d+]] = OpCompositeExtract %float {{%\d+}} 1
      test_struct().c.zx
// CHECK-NEXT:              OpLine [[file]] 28 12
// CHECK-NEXT: [[z:%\d+]] = OpConvertFToS %int [[z]]
// CHECK-NEXT: [[x:%\d+]] = OpConvertFToS %int [[x]]
// CHECK-NEXT:   {{%\d+}} = OpCompositeConstruct %v4int {{%\d+}} {{%\d+}} [[z]] [[x]]
  };

// CHECK:                        OpFDiv %float {{%\d+}} %float_2
// CHECK-NEXT:                   OpLine [[file]] 46 25
// CHECK-NEXT:  [[first:%\d+]] = OpCompositeConstruct %v2float {{%\d+}} {{%\d+}}
// CHECK-NEXT: [[second:%\d+]] = OpCompositeConstruct %v2float {{%\d+}} {{%\d+}}
// CHECK-NEXT:        {{%\d+}} = OpCompositeConstruct %mat2v2float [[first]] [[second]]
  float2x2 b = float2x2(a.x, b._m00, 2 + a.y, b._m11 / 2);

// CHECK:                   OpLine [[file]] 51 12
// CHECK-NEXT: [[y:%\d+]] = OpAccessChain %_ptr_Uniform_int4_bool_float3 %CONSTANTS %int_0
// CHECK-NEXT:   {{%\d+}} = OpAccessChain %_ptr_Uniform_v4int [[y]] %int_0
  int4 c = y.a;

// CHECK:                   OpLine [[file]] 58 3
// CHECK-NEXT: [[z:%\d+]] = OpLoad %type_2d_image %z
// CHECK-NEXT: [[z:%\d+]] = OpImageRead %v4int [[z]] {{%\d+}} None
// CHECK-NEXT: [[z:%\d+]] = OpVectorShuffle %v3int [[z]] [[z]] 0 1 2
// CHECK-NEXT:   {{%\d+}} = OpCompositeInsert %v3int %int_16 [[z]] 0
  z[uint2(2, 3)].x = 16;

// TODO(jaebaek): Update InitListHandler to properly emit debug info.
  b = float2x2(c);
  c = int4(b);
}

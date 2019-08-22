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

struct S {
  int a;
  void inc() { a++; }
};

S getS() {
  S a;
  return a;
}

struct init {
  int first;
  float second;
};

// Note that preprocessor prepends a "#line 1 ..." line to the whole file,
// the compliation sees line numbers incremented by 1.

void main() {
  S foo;

  init bar;

  int4 a = {
      float2(1, 0),
// CHECK: OpLine [[file]] 50 7
// CHECK: OpFunctionCall %int4_bool_float3_0 %test_struct
      test_struct().c.zx
// CHECK:      OpLine [[file]] 46 12
// CHECK:      OpCompositeExtract %float {{%\d+}} 0
// CHECK-NEXT: OpCompositeExtract %float {{%\d+}} 1
// CHECK-NEXT: OpConvertFToS %int
// CHECK-NEXT: OpConvertFToS %int
// CHECK-NEXT: OpCompositeConstruct %v4int
  };

// CHECK:                        OpFDiv %float {{%\d+}} %float_2
// CHECK-NEXT:                   OpLine [[file]] 64 25
// CHECK-NEXT:  [[first:%\d+]] = OpCompositeConstruct %v2float {{%\d+}} {{%\d+}}
// CHECK-NEXT: [[second:%\d+]] = OpCompositeConstruct %v2float {{%\d+}} {{%\d+}}
// CHECK-NEXT:        {{%\d+}} = OpCompositeConstruct %mat2v2float [[first]] [[second]]
  float2x2 b = float2x2(a.x, b._m00, 2 + a.y, b._m11 / 2);

// CHECK:                   OpLine [[file]] 69 12
// CHECK-NEXT: [[y:%\d+]] = OpAccessChain %_ptr_Uniform_int4_bool_float3 %CONSTANTS %int_0
// CHECK-NEXT:   {{%\d+}} = OpAccessChain %_ptr_Uniform_v4int [[y]] %int_0
  int4 c = y.a;

// CHECK:                   OpLine [[file]] 76 3
// CHECK-NEXT: [[z:%\d+]] = OpLoad %type_2d_image %z
// CHECK-NEXT: [[z:%\d+]] = OpImageRead %v4int [[z]] {{%\d+}} None
// CHECK-NEXT: [[z:%\d+]] = OpVectorShuffle %v3int [[z]] [[z]] 0 1 2
// CHECK-NEXT:   {{%\d+}} = OpCompositeInsert %v3int %int_16 [[z]] 0
  z[uint2(2, 3)].x = 16;

// CHECK:      OpLine [[file]] 82 3
// CHECK-NEXT: OpLoad %mat2v2float %b
// CHECK:      OpLine [[file]] 82 4
// CHECK-NEXT: OpFSub %v2float
  b--;

  int2x2 d;
// CHECK:      OpLine [[file]] 91 8
// CHECK-NEXT: OpLoad %mat2v2float %b
// CHECK-NEXT: OpLine [[file]] 91 3
// CHECK-NEXT: OpCompositeExtract %v2float
// CHECK:      OpLine [[file]] 91 11
// CHECK:      OpStore %d
  modf(b, d);

// CHECK:      OpLine [[file]] 95 7
// CHECK-NEXT: OpFunctionCall %void %S_inc %foo
  foo.inc();

// CHECK:      OpLine [[file]] 99 10
// CHECK-NEXT: OpFunctionCall %void %S_inc %temp_var_S
  getS().inc();

// CHECK:      OpLine [[file]] 105 19
// CHECK-NEXT: OpLoad %init %bar
// CHECK:      OpLine [[file]] 105 12
// CHECK-NEXT: OpConvertFToS %int
  int4 e = {1, 2, bar};

// CHECK:      OpLine [[file]] 111 16
// CHECK-NEXT: OpCompositeConstruct %v2float %float_1 %float_2
// CHECK-NEXT: OpLine [[file]] 111 22
// CHECK-NEXT: OpCompositeExtract %int
  b = float2x2(1, 2, bar);
// CHECK:      OpLine [[file]] 111 3
// CHECK-NEXT: OpStore %b

// TODO(jaebaek): Update InitListHandler to properly emit debug info.
  b = float2x2(c);
  c = int4(b);
}

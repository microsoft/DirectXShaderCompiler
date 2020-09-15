// Run: %dxc -T ps_6_0 -E main

void main() {
  float2x3 m = { {1,2,3} , {4,5,6} };

// CHECK:      [[m:%\d+]] = OpLoad %mat2v3float %m
// CHECK-NEXT:   {{%\d+}} = OpTranspose %mat3v2float [[m]]
  float3x2 n = transpose(m);

// CHECK:        [[p:%\d+]] = OpLoad %_arr_v3int_uint_2 %p
// CHECK-NEXT: [[p00:%\d+]] = OpCompositeExtract %int [[p]] 0 0
// CHECK-NEXT: [[p01:%\d+]] = OpCompositeExtract %int [[p]] 0 1
// CHECK-NEXT: [[p02:%\d+]] = OpCompositeExtract %int [[p]] 0 2
// CHECK-NEXT: [[p10:%\d+]] = OpCompositeExtract %int [[p]] 1 0
// CHECK-NEXT: [[p11:%\d+]] = OpCompositeExtract %int [[p]] 1 1
// CHECK-NEXT: [[p12:%\d+]] = OpCompositeExtract %int [[p]] 1 2
// CHECK-NEXT: [[pt0:%\d+]] = OpCompositeConstruct %v2int [[p00]] [[p10]]
// CHECK-NEXT: [[pt1:%\d+]] = OpCompositeConstruct %v2int [[p01]] [[p11]]
// CHECK-NEXT: [[pt2:%\d+]] = OpCompositeConstruct %v2int [[p02]] [[p12]]
// CHECK-NEXT:  [[pt:%\d+]] = OpCompositeConstruct %_arr_v2int_uint_3 [[pt0]] [[pt1]] [[pt2]]
// CHECK-NEXT:                OpStore %pt [[pt]]
  int2x3 p;
  int3x2 pt = transpose(p);

// CHECK:        [[q:%\d+]] = OpLoad %_arr_v2bool_uint_3 %q
// CHECK-NEXT: [[q00:%\d+]] = OpCompositeExtract %bool [[q]] 0 0
// CHECK-NEXT: [[q01:%\d+]] = OpCompositeExtract %bool [[q]] 0 1
// CHECK-NEXT: [[q10:%\d+]] = OpCompositeExtract %bool [[q]] 1 0
// CHECK-NEXT: [[q11:%\d+]] = OpCompositeExtract %bool [[q]] 1 1
// CHECK-NEXT: [[q20:%\d+]] = OpCompositeExtract %bool [[q]] 2 0
// CHECK-NEXT: [[q21:%\d+]] = OpCompositeExtract %bool [[q]] 2 1
// CHECK-NEXT: [[qt0:%\d+]] = OpCompositeConstruct %v3bool [[q00]] [[q10]] [[q20]]
// CHECK-NEXT: [[qt1:%\d+]] = OpCompositeConstruct %v3bool [[q01]] [[q11]] [[q21]]
// CHECK-NEXT:  [[qt:%\d+]] = OpCompositeConstruct %_arr_v3bool_uint_2 [[qt0]] [[qt1]]
// CHECK-NEXT:                OpStore %qt [[qt]]
  bool3x2 q;
  bool2x3 qt = transpose(q);

// CHECK:         [[r:%\d+]] = OpLoad %_arr_v4uint_uint_4 %r
// CHECK-NEXT:  [[r00:%\d+]] = OpCompositeExtract %uint [[r]] 0 0
// CHECK-NEXT:  [[r01:%\d+]] = OpCompositeExtract %uint [[r]] 0 1
// CHECK-NEXT:  [[r02:%\d+]] = OpCompositeExtract %uint [[r]] 0 2
// CHECK-NEXT:  [[r03:%\d+]] = OpCompositeExtract %uint [[r]] 0 3
// CHECK-NEXT:  [[r10:%\d+]] = OpCompositeExtract %uint [[r]] 1 0
// CHECK-NEXT:  [[r11:%\d+]] = OpCompositeExtract %uint [[r]] 1 1
// CHECK-NEXT:  [[r12:%\d+]] = OpCompositeExtract %uint [[r]] 1 2
// CHECK-NEXT:  [[r13:%\d+]] = OpCompositeExtract %uint [[r]] 1 3
// CHECK-NEXT:  [[r20:%\d+]] = OpCompositeExtract %uint [[r]] 2 0
// CHECK-NEXT:  [[r21:%\d+]] = OpCompositeExtract %uint [[r]] 2 1
// CHECK-NEXT:  [[r22:%\d+]] = OpCompositeExtract %uint [[r]] 2 2
// CHECK-NEXT:  [[r23:%\d+]] = OpCompositeExtract %uint [[r]] 2 3
// CHECK-NEXT:  [[r30:%\d+]] = OpCompositeExtract %uint [[r]] 3 0
// CHECK-NEXT:  [[r31:%\d+]] = OpCompositeExtract %uint [[r]] 3 1
// CHECK-NEXT:  [[r32:%\d+]] = OpCompositeExtract %uint [[r]] 3 2
// CHECK-NEXT:  [[r33:%\d+]] = OpCompositeExtract %uint [[r]] 3 3
// CHECK-NEXT:  [[rt0:%\d+]] = OpCompositeConstruct %v4uint [[r00]] [[r10]] [[r20]] [[r30]]
// CHECK-NEXT:  [[rt1:%\d+]] = OpCompositeConstruct %v4uint [[r01]] [[r11]] [[r21]] [[r31]]
// CHECK-NEXT:  [[rt2:%\d+]] = OpCompositeConstruct %v4uint [[r02]] [[r12]] [[r22]] [[r32]]
// CHECK-NEXT:  [[rt3:%\d+]] = OpCompositeConstruct %v4uint [[r03]] [[r13]] [[r23]] [[r33]]
// CHECK-NEXT:   [[rt:%\d+]] = OpCompositeConstruct %_arr_v4uint_uint_4 [[rt0]] [[rt1]] [[rt2]] [[rt3]]
// CHECK-NEXT:                 OpStore %rt [[rt]]
  uint4x4 r;
  uint4x4 rt = transpose(r);

// A 1-D matrix is in fact a vector, and its transpose is the vector itself.
//
// CHECK:      [[s:%\d+]] = OpLoad %v4float %s
// CHECK-NEXT:              OpStore %st [[s]]
  float1x4 s;
  float4x1 st = transpose(s);

// CHECK:      [[t:%\d+]] = OpLoad %float %t
// CHECK-NEXT:              OpStore %tt [[t]]
  float1x1 t;
  float1x1 tt = transpose(t);
}

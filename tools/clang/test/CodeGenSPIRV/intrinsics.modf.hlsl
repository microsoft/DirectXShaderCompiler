// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'modf' function can only operate on scalar/vector/matrix of float/int.

// CHECK: [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

// CHECK: OpName %ModfStructType "ModfStructType"
// CHECK: OpMemberName %ModfStructType 0 "frac"
// CHECK: OpMemberName %ModfStructType 1 "ip"
// CHECK: OpName %ModfStructType_0 "ModfStructType"
// CHECK: OpMemberName %ModfStructType_0 0 "frac"
// CHECK: OpMemberName %ModfStructType_0 1 "ip"
// CHECK: OpName %ModfStructType_1 "ModfStructType"
// CHECK: OpMemberName %ModfStructType_1 0 "frac"
// CHECK: OpMemberName %ModfStructType_1 1 "ip"

// Note: Even though we have used non-float types below,
// DXC has casted the input of modf() into float. Therefore
// all struct types have float members.
// CHECK:   %ModfStructType = OpTypeStruct %float %float
// CHECK: %ModfStructType_0 = OpTypeStruct %v4float %v4float
// CHECK: %ModfStructType_1 = OpTypeStruct %v3float %v3float

void main() {
  uint     a, ip_a, frac_a;
  int4     b, ip_b, frac_b;
  float2x3 c, ip_c, frac_c;

// CHECK:                 [[a:%\d+]] = OpLoad %uint %a
// CHECK-NEXT:           [[af:%\d+]] = OpConvertUToF %float [[a]]
// CHECK-NEXT:[[modf_struct_a:%\d+]] = OpExtInst %ModfStructType [[glsl]] ModfStruct [[af]]
// CHECK-NEXT:         [[ip_a:%\d+]] = OpCompositeExtract %float [[modf_struct_a]] 1
// CHECK-NEXT:    [[uint_ip_a:%\d+]] = OpConvertFToU %uint [[ip_a]]
// CHECK-NEXT:                         OpStore %ip_a [[uint_ip_a]]
// CHECK-NEXT:       [[frac_a:%\d+]] = OpCompositeExtract %float [[modf_struct_a]] 0
// CHECK-NEXT:  [[uint_frac_a:%\d+]] = OpConvertFToU %uint [[frac_a]]
// CHECK-NEXT:                         OpStore %frac_a [[uint_frac_a]]
  frac_a = modf(a, ip_a);

// CHECK:                 [[b:%\d+]] = OpLoad %v4int %b
// CHECK-NEXT:           [[bf:%\d+]] = OpConvertSToF %v4float [[b]]
// CHECK-NEXT:[[modf_struct_b:%\d+]] = OpExtInst %ModfStructType_0 [[glsl]] ModfStruct [[bf]]
// CHECK-NEXT:         [[ip_b:%\d+]] = OpCompositeExtract %v4float [[modf_struct_b]] 1
// CHECK-NEXT:    [[int4_ip_b:%\d+]] = OpConvertFToS %v4int [[ip_b]]
// CHECK-NEXT:                         OpStore %ip_b [[int4_ip_b]]
// CHECK-NEXT:       [[frac_b:%\d+]] = OpCompositeExtract %v4float [[modf_struct_b]] 0
// CHECK-NEXT:  [[int4_frac_b:%\d+]] = OpConvertFToS %v4int [[frac_b]]
// CHECK-NEXT:                         OpStore %frac_b [[int4_frac_b]]
  frac_b = modf(b, ip_b);

// CHECK:                      [[c:%\d+]] = OpLoad %mat2v3float %c
// CHECK-NEXT:            [[c_row0:%\d+]] = OpCompositeExtract %v3float [[c]] 0
// CHECK-NEXT:[[modf_struct_c_row0:%\d+]] = OpExtInst %ModfStructType_1 [[glsl]] ModfStruct [[c_row0]]
// CHECK-NEXT:         [[ip_c_row0:%\d+]] = OpCompositeExtract %v3float [[modf_struct_c_row0]] 1
// CHECK-NEXT:       [[frac_c_row0:%\d+]] = OpCompositeExtract %v3float [[modf_struct_c_row0]] 0
// CHECK-NEXT:            [[c_row1:%\d+]] = OpCompositeExtract %v3float [[c]] 1
// CHECK-NEXT:[[modf_struct_c_row1:%\d+]] = OpExtInst %ModfStructType_1 [[glsl]] ModfStruct [[c_row1]]
// CHECK-NEXT:         [[ip_c_row1:%\d+]] = OpCompositeExtract %v3float [[modf_struct_c_row1]] 1
// CHECK-NEXT:       [[frac_c_row1:%\d+]] = OpCompositeExtract %v3float [[modf_struct_c_row1]] 0
// CHECK-NEXT:              [[ip_c:%\d+]] = OpCompositeConstruct %mat2v3float [[ip_c_row0]] [[ip_c_row1]]
// CHECK-NEXT:                              OpStore %ip_c [[ip_c]]
// CHECK-NEXT:            [[frac_c:%\d+]] = OpCompositeConstruct %mat2v3float [[frac_c_row0]] [[frac_c_row1]]
// CHECK-NEXT:                              OpStore %frac_c [[frac_c]]
  frac_c = modf(c, ip_c);
}

// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s

struct S {
  float2   a;
  float    b;
  double2  c;
  double   d;
  int64_t  e;
  uint64_t f;
};

void main() {

// CHECK:              [[inf:%[0-9]+]] = OpFDiv %float %float_1 %float_0
// CHECK-NEXT:        [[inf2:%[0-9]+]] = OpCompositeConstruct %v2float [[inf]] [[inf]]
// CHECK-NEXT:  [[inf_double:%[0-9]+]] = OpFConvert %double [[inf]]
// CHECK-NEXT: [[inf2_double:%[0-9]+]] = OpCompositeConstruct %v2double [[inf_double]] [[inf_double]] 
// CHECK-NEXT:  [[inf_double_0:%[0-9]+]] = OpFConvert %double [[inf]]
// CHECK-NEXT: [[inf_double_:%[0-9]+]] = OpFConvert %double [[inf]]
// CHECK-NEXT:   [[inf_int64:%[0-9]+]] = OpConvertFToS %long [[inf_double_]]
// CHECK-NEXT: [[inf_double__0:%[0-9]+]] = OpFConvert %double [[inf]]
// CHECK-NEXT:  [[inf_uint64:%[0-9]+]] = OpConvertFToU %ulong [[inf_double__0]]
// CHECK-NEXT:             {{%[0-9]+}} = OpCompositeConstruct %S [[inf2]] [[inf]] [[inf2_double]] [[inf_double_0]] [[inf_int64]] [[inf_uint64]]
  S s3 = (S)(1.0 / 0.0);

// CHECK:              [[b:%[0-9]+]] = OpLoad %float %b
// CHECK-NEXT:  [[b2_float:%[0-9]+]] = OpCompositeConstruct %v2float [[b]] [[b]]
// CHECK-NEXT:  [[b_double:%[0-9]+]] = OpFConvert %double [[b]]
// CHECK-NEXT: [[b2_double:%[0-9]+]] = OpCompositeConstruct %v2double [[b_double]] [[b_double]]
// CHECK-NEXT:  [[b_double_0:%[0-9]+]] = OpFConvert %double [[b]]
// CHECK-NEXT: [[b_double_:%[0-9]+]] = OpFConvert %double [[b]]
// CHECK-NEXT:   [[b_int64:%[0-9]+]] = OpConvertFToS %long [[b_double_]]
// CHECK-NEXT: [[b_double__0:%[0-9]+]] = OpFConvert %double [[b]]
// CHECK-NEXT:  [[b_uint64:%[0-9]+]] = OpConvertFToU %ulong [[b_double__0]]
// CHECK-NEXT:           {{%[0-9]+}} = OpCompositeConstruct %S [[b2_float]] [[b]] [[b2_double]] [[b_double_0]] [[b_int64]] [[b_uint64]]
  float b;
  S s2 = (S)(b);


// CHECK:              [[a:%[0-9]+]] = OpLoad %double %a
// CHECK-NEXT:   [[a_float:%[0-9]+]] = OpFConvert %float [[a]]
// CHECK-NEXT:  [[a2_float:%[0-9]+]] = OpCompositeConstruct %v2float [[a_float]] [[a_float]]
// CHECK-NEXT:   [[a_float_0:%[0-9]+]] = OpFConvert %float [[a]]
// CHECK-NEXT: [[a2_double:%[0-9]+]] = OpCompositeConstruct %v2double [[a]] [[a]]
// CHECK-NEXT:   [[a_int64:%[0-9]+]] = OpConvertFToS %long [[a]]
// CHECK-NEXT:  [[a_uint64:%[0-9]+]] = OpConvertFToU %ulong [[a]]
// CHECK-NEXT:           {{%[0-9]+}} = OpCompositeConstruct %S [[a2_float]] [[a_float_0]] [[a2_double]] [[a]] [[a_int64]] [[a_uint64]]
  double a;
  S s1 = (S)(a);
}

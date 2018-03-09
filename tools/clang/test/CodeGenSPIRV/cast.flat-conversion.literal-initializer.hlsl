// Run: %dxc -T ps_6_0 -E main

struct S {
  float2   a;
  float    b;
  double2  c;
  double   d;
  int64_t  e;
  uint64_t f;
};

void main() {

// CHECK:              [[inf:%\d+]] = OpFDiv %float %float_1 %float_0
// CHECK-NEXT:        [[inf2:%\d+]] = OpCompositeConstruct %v2float [[inf]] [[inf]]
// CHECK-NEXT:  [[inf_double:%\d+]] = OpFConvert %double [[inf]]
// CHECK-NEXT: [[inf2_double:%\d+]] = OpCompositeConstruct %v2double [[inf_double]] [[inf_double]] 
// CHECK-NEXT:  [[inf_double:%\d+]] = OpFConvert %double [[inf]]
// CHECK-NEXT: [[inf_double_:%\d+]] = OpFConvert %double [[inf]]
// CHECK-NEXT:   [[inf_int64:%\d+]] = OpConvertFToS %long [[inf_double_]]
// CHECK-NEXT: [[inf_double_:%\d+]] = OpFConvert %double [[inf]]
// CHECK-NEXT:  [[inf_uint64:%\d+]] = OpConvertFToU %ulong [[inf_double_]]
// CHECK-NEXT:             {{%\d+}} = OpCompositeConstruct %S [[inf2]] [[inf]] [[inf2_double]] [[inf_double]] [[inf_int64]] [[inf_uint64]]
  S s3 = (S)(1.0 / 0.0);

// CHECK:              [[b:%\d+]] = OpLoad %float %b
// CHECK-NEXT:  [[b2_float:%\d+]] = OpCompositeConstruct %v2float [[b]] [[b]]
// CHECK-NEXT:  [[b_double:%\d+]] = OpFConvert %double [[b]]
// CHECK-NEXT: [[b2_double:%\d+]] = OpCompositeConstruct %v2double [[b_double]] [[b_double]]
// CHECK-NEXT:  [[b_double:%\d+]] = OpFConvert %double [[b]]
// CHECK-NEXT: [[b_double_:%\d+]] = OpFConvert %double [[b]]
// CHECK-NEXT:   [[b_int64:%\d+]] = OpConvertFToS %long [[b_double_]]
// CHECK-NEXT: [[b_double_:%\d+]] = OpFConvert %double [[b]]
// CHECK-NEXT:  [[b_uint64:%\d+]] = OpConvertFToU %ulong [[b_double_]]
// CHECK-NEXT:           {{%\d+}} = OpCompositeConstruct %S [[b2_float]] [[b]] [[b2_double]] [[b_double]] [[b_int64]] [[b_uint64]]
  float b;
  S s2 = (S)(b);


// CHECK:              [[a:%\d+]] = OpLoad %double %a
// CHECK-NEXT:   [[a_float:%\d+]] = OpFConvert %float [[a]]
// CHECK-NEXT:  [[a2_float:%\d+]] = OpCompositeConstruct %v2float [[a_float]] [[a_float]]
// CHECK-NEXT:   [[a_float:%\d+]] = OpFConvert %float [[a]]
// CHECK-NEXT: [[a2_double:%\d+]] = OpCompositeConstruct %v2double [[a]] [[a]]
// CHECK-NEXT:   [[a_int64:%\d+]] = OpConvertFToS %long [[a]]
// CHECK-NEXT:  [[a_uint64:%\d+]] = OpConvertFToU %ulong [[a]]
// CHECK-NEXT:           {{%\d+}} = OpCompositeConstruct %S [[a2_float]] [[a_float]] [[a2_double]] [[a]] [[a_int64]] [[a_uint64]]
  double a;
  S s1 = (S)(a);
}

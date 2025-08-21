// RUN: %dxc -T lib_6_9 -enable-16bit-types -DFUNC=isnan %s | FileCheck %s
// RUN: %dxc -T lib_6_9 -enable-16bit-types -DFUNC=isinf %s | FileCheck %s
// RUN: %dxc -T lib_6_9 -enable-16bit-types -DFUNC=isfinite %s | FileCheck %s

// CHECK-LABEL: test_func
// CHECK:  call i1 @dx.op.isSpecialFloat.f16(i32 [[OP:[0-9]*]], half
export bool test_func(half h) {
  return FUNC(h);
}

// CHECK-LABEL: test_func2
// CHECK:  call <2 x i1> @dx.op.isSpecialFloat.v2f16(i32 [[OP:[0-9]*]], <2 x half>
export bool2 test_func2(half2 h) {
  return FUNC(h);
}

// CHECK-LABEL: test_func3
// CHECK:  call <3 x i1> @dx.op.isSpecialFloat.v3f16(i32 [[OP:[0-9]*]], <3 x half>
export bool3 test_func3(half3 h) {
  return FUNC(h);
}

// CHECK-LABEL: test_func4
// CHECK:  call <4 x i1> @dx.op.isSpecialFloat.v4f16(i32 [[OP:[0-9]*]], <4 x half>
export bool4 test_func4(half4 h) {
  return FUNC(h);
}

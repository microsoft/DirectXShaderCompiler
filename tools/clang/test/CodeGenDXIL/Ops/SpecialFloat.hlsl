// RUN: %dxc -T lib_6_3 -enable-16bit-types %s | FileCheck %s

// CHECK-LABEL: test_isinf
// CHECK:  call i1 @dx.op.isSpecialFloat.f16(i32 9, half
export bool test_isinf(half h) {
  return isinf(h);
}

// CHECK-LABEL: test_isnan
// CHECK:  call i1 @dx.op.isSpecialFloat.f16(i32 8, half

export bool test_isnan(half h) {
  return isnan(h);
}

// CHECK-LABEL: test_isfinite
// CHECK:  call i1 @dx.op.isSpecialFloat.f16(i32 10, half

export bool test_isfinite(half h) {
  return isfinite(h);
}

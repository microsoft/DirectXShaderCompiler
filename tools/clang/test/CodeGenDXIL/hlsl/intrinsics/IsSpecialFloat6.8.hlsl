// RUN: %dxc -T lib_6_8 -enable-16bit-types %s | FileCheck %s

// 31744 = 0x7c00, which corresponds to positive infinity
// it is also used as a mask to get the exp bits of a half
// -1024 = 0xfc00, which corresponds to negative infinity
// 1023 = 0x3ff, and is used to mask the significand of a half

// CHECK-LABEL: test_isinf
// CHECK: [[V1:%.*]] = bitcast half {{.*}} to i16
// CHECK: [[V2:%.*]] = icmp eq i16 [[V1]], 31744
// CHECK: [[V3:%.*]] = icmp eq i16 [[V1]], -1024
// CHECK: [[V4:%.*]] = or i1 [[V2]], [[V3]]
// CHECK: ret i1 [[V4]]
export bool test_isinf(half h) {
  return isinf(h);
}

// CHECK-LABEL: test_isnan
// CHECK: [[V1:%.*]] = bitcast half {{.*}} to i16
// CHECK: [[V2:%.*]] = and i16 [[V1]], 31744 
// CHECK: [[V3:%.*]] = icmp eq i16 [[V2]], 31744
// CHECK: [[V4:%.*]] = and i16 [[V1]], 1023
// CHECK: [[V5:%.*]] = icmp ne i16 [[V4]], 0
// CHECK: [[V6:%.*]] = and i1 [[V3]], [[V5]]
// CHECK: ret i1 [[V6]]
export bool test_isnan(half h) {
  return isnan(h);
}

// CHECK-LABEL: test_isfinite
// CHECK: [[V1:%.*]] = bitcast half {{.*}} to i16
// CHECK: [[V2:%.*]] = and i16 [[V1]], 31744
// CHECK: [[V3:%.*]] = icmp ne i16 [[V2]], 31744
// CHECK: ret i1 [[V3]]
export bool test_isfinite(half h) {
  return isfinite(h);
}

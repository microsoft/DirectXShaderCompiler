// RUN: %dxc -T lib_6_3 %s -fcgl | FileCheck %s

int shl32(int V, int S) {
  return V << S;
}

// CHECK: define internal i32 @"\01?shl32
// CHECK-DAG:  %[[Masked:[0-9]+]] = and i32 %{{[0-9]+}}, 31
// CHECK-DAG:  %{{[0-9]+}} = shl i32 %{{[0-9]+}}, %[[Masked]]

int shr32(int V, int S) {
  return V >> S;
}

// CHECK: define internal i32 @"\01?shr32
// CHECK-DAG:  %[[Masked:[0-9]+]] = and i32 %{{[0-9]+}}, 31
// CHECK-DAG:  %{{[0-9]+}} = ashr i32 %{{[0-9]+}}, %[[Masked]]

int64_t shl64(int64_t V, int64_t S) {
  return V << S;
}

// CHECK define internal i64 @"\01?shl64
// CHECK-DAG:  %[[Masked:[0-9]+]] = and i64 %{{[0-9]+}}, 63
// CHECK-DAG:  %{{[0-9]+}} = shl i64 %{{[0-9]+}}, %[[Masked]]

int64_t shr64(int64_t V, int64_t S) {
  return V >> S;
}

// CHECK: define internal i64 @"\01?shr64@@YA_J_J0@Z"(i64 %V, i64 %S) #0 {
// CHECK-DAG:  %[[Masked:[0-9]+]] = and i64 %{{[0-9]+}}, 63
// CHECK-DAG:  %{{[0-9]+}} = ashr i64 %{{[0-9]+}}, %[[Masked]]

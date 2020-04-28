// RUN: %dxc -T lib_6_3 -auto-binding-space 11 -default-linkage external %s | FileCheck %s

// Make sure calls with empty struct params are well-behaved

// CHECK: define float @"\01?test2@@YAMUT@@@Z"(%struct.T* %[[t:.*]]) #0 {
// CHECK-NOT:memcpy
// Copy from t is a no-op, no code should be generated
// CHECK: call float @"\01?test@@YAMUT@@@Z"(%struct.T* %[[t]])

struct T {
};

float test(T t);

float test2(T t): SV_Target {
  return test(t);
}
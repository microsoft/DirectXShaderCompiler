// RUN: %dxc -T lib_6_3 -auto-binding-space 11 -default-linkage external %s | FileCheck %s

// Make sure calls with empty struct params are well-behaved

// CHECK: %[[alloca:.*]] = alloca %struct.T
// Copy from t is a no-op, no code should be generated
// CHECK-NEXT: call float @"\01?test@@YAMUT@@@Z"(%struct.T* nonnull %[[alloca]])

struct T {
};

float test(T t);

float test2(T t): SV_Target {
  return test(t);
}
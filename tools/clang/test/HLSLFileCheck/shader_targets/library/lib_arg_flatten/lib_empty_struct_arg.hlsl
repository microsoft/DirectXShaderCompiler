// RUN: %dxc -T lib_6_3 -auto-binding-space 11 -default-linkage external %s | FileCheck %s

// Make sure calls with empty struct params are well-behaved

// CHECK: %[[alloca:.*]] = alloca %struct.T
// CHECK-NOT:memcpy
// CHECK-NEXT: call float @"\01?test@@YAMUT@@@Z"(%struct.T* nonnull %[[alloca]])

struct T {
};

float test(T t);

float test2(T t): SV_Target {
  return test(t);
}
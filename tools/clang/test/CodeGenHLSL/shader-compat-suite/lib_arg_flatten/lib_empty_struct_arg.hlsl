// RUN: %dxc -T lib_6_3 -auto-binding-space 11 -default-linkage external %s | FileCheck %s

// Make sure empty struct arg works.
// CHECK: call float @"\01?test@@YAMUT@@@Z"(%struct.T* %t)

struct T {
};

float test(T t);

float test2(T t): SV_Target {
  return test(t);
}
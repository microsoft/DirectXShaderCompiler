// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure function call on external function is flattened.

// CHECK: call void @"\01?test_extern@@YAMUT@@@Z"(float %0, float %1

struct T {
  float a;
  float b;
};

float test_extern(T t);

float test(T t)
{
  return test_extern(t);
}

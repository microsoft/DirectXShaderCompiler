// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure struct parameter not replaced as function call arg.

// CHECK: call void @"\01?test_extern@@YAMUT@@@Z"(float %{{.*}}, float %{{.*}}, float* nonnull %{{.*}})

struct T {
  float a;
  float b;
};

float test_extern(T t);

float test(T t)
{
  T tmp = t;
  float x = test_extern(tmp);
  return x + tmp.b;
}

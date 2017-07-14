// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure function call on external function is flattened.

// CHECK: call void @"\01?test_extern@@YAMUT@@Y01U1@0AAV?$matrix@M$01$01@@@Z"(float %{{.*}}, float %{{.*}}, [2 x float]* nonnull %{{.*}}, [2 x float]* nonnull %{{.*}}, float* nonnull %{{.*}}, float* nonnull %{{.*}}, [4 x float]* nonnull %{{.*}}, float* nonnull %{{.*}})

struct T {
  float a;
  float b;
};

float test_extern(T t, T t2[2], out T t3, inout float2x2 m);

float test(T t, T t2[2], out T t3, inout float2x2 m)
{
  return test_extern(t, t2, t3, m);
}

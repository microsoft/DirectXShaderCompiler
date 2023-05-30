// RUN: %dxc -T lib_6_4 -fcgl %s | FileCheck %s

void CalledFunction(inout float X, inout float Y, inout float Z) {
  X = 1.0;
  Y = 2.0;
  Z = 3.0;
}

void fn() {
  float X, Y, Z = 0.0;
  CalledFunction(X, Y, Z);

  CalledFunction(X, X, Z);

  CalledFunction(X, Y, X);
}

// CHECK: define internal void @"\01?fn{{[@$?.A-Za-z0-9_]+}}"()
// CHECK: [[X:%[0-9A-Z]+]] = alloca float, align 4
// CHECK: [[Y:%[0-9A-Z]+]] = alloca float, align 4
// CHECK: [[Z:%[0-9A-Z]+]] = alloca float, align 4
// CHECK: [[Tmp1:%[0-9a-z.]+]] = alloca float
// CHECK: [[Tmp2:%[0-9a-z.]+]] = alloca float

// First call has no copy-in/copy out parameters since all parameters are unique.
// CHECK: call void @"\01?CalledFunction
// CHECK-SAME: (float* dereferenceable(4) [[X]], float* dereferenceable(4) [[Y]], float* dereferenceable(4) [[Z]])

// Second call, copies X for parameter 2.
// CHECK: [[TmpX:%[0-9A-Z]+]] = load float, float* [[X]], align 4
// CHECK: store float [[TmpX]], float* [[Tmp1]]

// CHECK: call void @"\01?CalledFunction
// CHECK-SAME: (float* dereferenceable(4) [[X]], float* dereferenceable(4) [[Tmp1]], float* dereferenceable(4) [[Z]])

// Second call, saves parameter 2 to X after the call.
// CHECK: [[TmpX:%[0-9A-Z]+]] = load float, float* [[Tmp1]]
// CHECK: store float [[TmpX]], float* [[X]]

// The third call copies X for the third parameter.
// CHECK: [[TmpX:%[0-9A-Z]+]] = load float, float* [[X]], align 4
// CHECK: store float [[TmpX]], float* [[Tmp2]]

// CHECK: call void @"\01?CalledFunction
// CHECK-SAME: (float* dereferenceable(4) [[X]], float* dereferenceable(4) [[Y]], float* dereferenceable(4) [[Tmp2]])

// The third call stores parameter 3 to X after the call.
// CHECK: [[TmpX:%[0-9A-Z]+]] = load float, float* [[Tmp2]]
// CHECK: store float [[TmpX]], float* [[X]]

// CHECK: ret

void fn2() {
  float X = 0.0;
  CalledFunction(X, X, X);
}

// CHECK: define internal void @"\01?fn2

// CHECK: [[X:%[0-9A-Z]+]] = alloca float, align 4
// CHECK: [[Tmp1:%[0-9a-z.]+]] = alloca float
// CHECK: [[Tmp2:%[0-9a-z.]+]] = alloca float

// X gets copied in for both parameters two and three. The MSVC ABI dictates
// right to left construction of arguments.
// CHECK: [[TmpX:%[0-9A-Z]+]] = load float, float* [[X]], align 4
// CHECK: store float [[TmpX]], float* [[Tmp1]]
// CHECK: [[TmpX:%[0-9A-Z]+]] = load float, float* [[X]], align 4
// CHECK: store float [[TmpX]], float* [[Tmp2]]

// CHECK: call void @"\01?CalledFunction
// CHECK-SAME: (float* dereferenceable(4) [[X]], float* dereferenceable(4) [[Tmp2]], float* dereferenceable(4) [[Tmp1]])

// X gets restored from parameter 2 _then_ parameter 3, so the last paramter is
// the final value of X.

// CHECK: [[X1:%[0-9A-Z]+]] = load float, float* [[Tmp2]]
// CHECK: store float [[X1]], float* [[X]]

// CHECK: [[X2:%[0-9A-Z]+]] = load float, float* [[Tmp1]]
// CHECK: store float [[X2]], float* [[X]]

// CHECK: ret

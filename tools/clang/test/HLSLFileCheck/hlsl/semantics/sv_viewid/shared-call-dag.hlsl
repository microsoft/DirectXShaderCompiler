// RUN: %dxc -E main -T ps_6_1 %s -Fc %t
// RUN: FileCheck %s --input-file=%t
// RUN: FileCheck %s --check-prefix=CALLS --input-file=%t
// RUN: FileCheck %s --check-prefix=FUNCTIONS --input-file=%t
// This fork has no FileCheck COUNT directive. Check each edge and definition.
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f13@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f13@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f12@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f12@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f11@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f11@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f10@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f10@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f9@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f9@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f8@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f8@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f7@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f7@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f6@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f6@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f5@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f5@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f4@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f4@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f3@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f3@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f2@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f2@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f1@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f1@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f0@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f0@@
// CALLS-NOT: call fastcc float
// CALLS: call fastcc float @"\01?f14@@
// CALLS-NOT: call fastcc float
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f14@@
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f13@@
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f12@@
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f11@@
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f10@@
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f9@@
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f8@@
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f7@@
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f6@@
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f5@@
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f4@@
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f3@@
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f2@@
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f1@@
// FUNCTIONS-NOT: define internal fastcc float
// FUNCTIONS: define internal fastcc float @"\01?f0@@
// FUNCTIONS-NOT: define internal fastcc float
// CHECK: output 0 depends on inputs: { 0 }
[noinline] float f0(float x) { return x*x+1; }
[noinline] float f1(float x) { [branch] if (x > 1) return f0(x+0.25)*2; else return f0(x-0.5)+3; }
[noinline] float f2(float x) { [branch] if (x > 2) return f1(x+0.25)*2; else return f1(x-0.5)+3; }
[noinline] float f3(float x) { [branch] if (x > 3) return f2(x+0.25)*2; else return f2(x-0.5)+3; }
[noinline] float f4(float x) { [branch] if (x > 4) return f3(x+0.25)*2; else return f3(x-0.5)+3; }
[noinline] float f5(float x) { [branch] if (x > 5) return f4(x+0.25)*2; else return f4(x-0.5)+3; }
[noinline] float f6(float x) { [branch] if (x > 6) return f5(x+0.25)*2; else return f5(x-0.5)+3; }
[noinline] float f7(float x) { [branch] if (x > 7) return f6(x+0.25)*2; else return f6(x-0.5)+3; }
[noinline] float f8(float x) { [branch] if (x > 8) return f7(x+0.25)*2; else return f7(x-0.5)+3; }
[noinline] float f9(float x) { [branch] if (x > 9) return f8(x+0.25)*2; else return f8(x-0.5)+3; }
[noinline] float f10(float x) { [branch] if (x > 10) return f9(x+0.25)*2; else return f9(x-0.5)+3; }
[noinline] float f11(float x) { [branch] if (x > 11) return f10(x+0.25)*2; else return f10(x-0.5)+3; }
[noinline] float f12(float x) { [branch] if (x > 12) return f11(x+0.25)*2; else return f11(x-0.5)+3; }
[noinline] float f13(float x) { [branch] if (x > 13) return f12(x+0.25)*2; else return f12(x-0.5)+3; }
[noinline] float f14(float x) { [branch] if (x > 14) return f13(x+0.25)*2; else return f13(x-0.5)+3; }
float main(float x : INPUT) : SV_Target { return f14(x); }

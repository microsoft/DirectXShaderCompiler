// RUN: %dxc -T ps_6_0 -E main -HV 202x %s | FileCheck %s

// Verify that const-instance methods generate valid DXIL: a const method
// called on a non-const local lvalue should be inlined as a normal read of
// the object's fields, and a const method called on a cbuffer member should
// lower to cbufferLoadLegacy.

struct S {
  int x;
  int y;
  int sum() const { return x + y; }
};

cbuffer CB { S cs; };

int main(int idx : A) : SV_Target {
  S ls = {3, 4};
  return ls.sum() + cs.sum();
}

// CHECK: define void @main()
// CHECK: call %dx.types.Handle @dx.op.createHandle(
// CHECK: call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(
// The 3+4 from the local 'ls' is constant-folded to 7 and added to the
// two i32 lanes loaded from the cbuffer.
// CHECK: add i32 {{.*}}, 7
// CHECK: call void @dx.op.storeOutput.i32(
// CHECK: ret void

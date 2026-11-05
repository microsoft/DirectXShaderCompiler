// RUN: %dxc -T ps_6_0 -E main -HV 202x %s | FileCheck %s

// Verify that const-instance methods generate valid DXIL: a const method
// called on a non-const local lvalue should be inlined as a normal read of
// the object's fields, and a const method called on a cbuffer member should
// lower to cbufferLoadLegacy.

struct LocalS {
  int x;
  int y;
  mutable int cache;
  int sum() const { return x + y; }
  int incrementCache() const { return ++cache; }
};

struct BufferS {
  int x;
  int y;
  int sum() const { return x + y; }
};

cbuffer CB { BufferS cs; };

int main(int idx : A) : SV_Target {
  LocalS ls = {3, 4, 0};
  return ls.sum() + ls.incrementCache() + cs.sum();
}

// CHECK: define void @main()
// CHECK: call %dx.types.Handle @dx.op.createHandle(
// CHECK: call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(
// The local sum and cache increment are constant-folded to 8 and added to the
// two i32 lanes loaded from the cbuffer.
// CHECK: add i32 {{.*}}, 8
// CHECK: call void @dx.op.storeOutput.i32(
// CHECK: ret void

// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// Regression test for a crash when an expression classified
// as an xvalue by clang is used as an argument to a function
// expecting a prvalue.

// StructuredBuffer::Load() returns a struct type by value
// rather than by reference like StructuredBuffer::operator[].
// A struct returned by value is considered an xvalue so it can be moved,

// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK: extractvalue %dx.types.ResRet.i32
// CHECK: call void @dx.op.bufferStore.i32

struct S { int x; };
StructuredBuffer<S> structbuf;
AppendStructuredBuffer<S> appbuf;
void main() { appbuf.Append(structbuf.Load(0)); }
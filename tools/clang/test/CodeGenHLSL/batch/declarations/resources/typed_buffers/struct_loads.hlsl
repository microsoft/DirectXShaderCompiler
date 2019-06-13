// RUN: %dxc -E main -T vs_6_2 %s | FileCheck %s

// Test that individual fields can be loaded from a
// typed buffer based on a struct type.
// Regression test for GitHub #2258

AppendStructuredBuffer<int4> output;

struct Scalars { int a, b; };
Buffer<Scalars> buf_scalars;

struct Vectors { int2 a, b; };
Buffer<Vectors> buf_vectors;

void main()
{
  // CHECK: %[[scalretres:.*]] = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, {{.*}}, i32 0, i32 undef)
  // CHECK: %[[scalval:.*]] = extractvalue %dx.types.ResRet.i32 %[[scalretres]], 1
  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, {{.*}}, i32 {{.*}}, i32 0, i32 %[[scalval]], i32 0, i32 0, i32 0, i8 15, i32 4)
  output.Append(int4(buf_scalars[0].b, 0, 0, 0));

  // CHECK: %[[vecretres:.*]] = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, {{.*}}, i32 0, i32 undef)
  // CHECK: %[[vecvalx:.*]] = extractvalue %dx.types.ResRet.i32 %[[vecretres]], 2
  // CHECK: %[[vecvaly:.*]] = extractvalue %dx.types.ResRet.i32 %[[vecretres]], 3
  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, {{.*}}, i32 {{.*}}, i32 0, i32 %[[vecvalx]], i32 %[[vecvaly]], i32 0, i32 0, i8 15, i32 4)
  output.Append(int4(buf_vectors[0].b, 0, 0));
}
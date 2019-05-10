// RUN: %dxc -E main -T vs_6_2 -HV 2018 -enable-16bit-types %s | FileCheck %s

struct Empty {};
struct ComplexStruct
{
  int16_t i16;
  // 2-byte padding
  struct { float f32; } s; // Nested type
  struct {} _; // Zero-sized field.
};

AppendStructuredBuffer<int> buf;

void main() {
  // Test size of literals
  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, {{.*}}, i32 4, i32 undef
  buf.Append(sizeof 42);
  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, {{.*}}, i32 4, i32 undef
  buf.Append(sizeof 42.0);

  // Test size and packing of scalar types, vectors and arrays all at once.

  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, {{.*}}, i32 12, i32 undef
  buf.Append(sizeof(int16_t3[2]));
  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, {{.*}}, i32 12, i32 undef
  buf.Append(sizeof(half3[2]));

  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, {{.*}}, i32 24, i32 undef
  buf.Append(sizeof(int3[2]));
  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, {{.*}}, i32 24, i32 undef
  buf.Append(sizeof(float3[2]));
  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, {{.*}}, i32 24, i32 undef
  buf.Append(sizeof(bool3[2]));

  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, {{.*}}, i32 48, i32 undef
  buf.Append(sizeof(int64_t3[2]));
  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, {{.*}}, i32 48, i32 undef
  buf.Append(sizeof(double3[2]));

  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, {{.*}}, i32 0, i32 undef
  buf.Append(sizeof(Empty[2]));

  // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, {{.*}}, i32 8, i32 undef
  buf.Append(sizeof(ComplexStruct));
}
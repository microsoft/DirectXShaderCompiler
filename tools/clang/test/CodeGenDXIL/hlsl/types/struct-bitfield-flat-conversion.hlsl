// RUN: %dxc -T cs_6_0 -E main -HV 2021 %s | FileCheck %s

// Flat conversions, splats and aggregate initializer lists treat each bitfield
// as one scalar element. Bits a and b share one 32-bit storage unit, so every
// conversion must read and write them at their own offset and width rather
// than as the whole storage unit. All inputs are constants, so each store
// checks the folded element values.

struct Bits {
  uint a : 16;
  uint b : 16;
  uint c;
};

struct Plain {
  uint a;
  uint b;
  uint c;
};

struct Base {
  uint base;
};

struct DerivedBits : Base {
  uint a : 16;
  uint b : 16;
  uint c;
};

struct DerivedPlain : Base {
  uint a;
  uint b;
  uint c;
};

struct SignedBits {
  int a : 4;
  int b : 28;
};

struct BoolBits {
  bool a : 1;
  bool b : 1;
  uint c;
};

struct PlainBools {
  bool a;
  bool b;
  uint c;
};

struct Pair {
  uint a : 16;
  uint b : 16;
};

RWStructuredBuffer<uint4> Out : register(u0);
RWStructuredBuffer<int2> SignedOut : register(u1);
RWStructuredBuffer<uint4> BoolOut : register(u2);

[numthreads(1, 1, 1)]
void main() {
  Bits s;
  s.a = 1;
  s.b = 2;
  s.c = 3;

  Plain p;
  p.a = 4;
  p.b = 5;
  p.c = 6;

  DerivedBits d;
  d.base = 7;
  d.a = 8;
  d.b = 9;
  d.c = 10;

  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 0, i32 0, i32 1, i32 2, i32 3, i32 0, i8 15)
  Plain p2 = (Plain)s;
  Out[0] = uint4(p2.a, p2.b, p2.c, 0);

  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 1, i32 0, i32 4, i32 5, i32 6, i32 0, i8 15)
  Bits s2 = (Bits)p;
  Out[1] = uint4(s2.a, s2.b, s2.c, 0);

  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 2, i32 0, i32 1, i32 2, i32 3, i32 0, i8 15)
  uint3 v = (uint3)s;
  Out[2] = uint4(v, 0);

  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 3, i32 0, i32 21, i32 22, i32 23, i32 0, i8 15)
  Bits s3 = (Bits)uint3(21, 22, 23);
  Out[3] = uint4(s3.a, s3.b, s3.c, 0);

  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 4, i32 0, i32 7, i32 7, i32 7, i32 0, i8 15)
  Bits s4 = (Bits)7;
  Out[4] = uint4(s4.a, s4.b, s4.c, 0);

  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 5, i32 0, i32 1, i32 2, i32 3, i32 0, i8 15)
  Plain p3 = {s};
  Out[5] = uint4(p3.a, p3.b, p3.c, 0);

  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 6, i32 0, i32 4, i32 5, i32 6, i32 0, i8 15)
  Bits s5 = {p};
  Out[6] = uint4(s5.a, s5.b, s5.c, 0);

  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 7, i32 0, i32 7, i32 8, i32 9, i32 10, i8 15)
  DerivedPlain dp = (DerivedPlain)d;
  Out[7] = uint4(dp.base, dp.a, dp.b, dp.c);

  // A signed bitfield is sign-extended when it is read as an element.
  SignedBits sb;
  sb.a = -3;
  sb.b = 5;
  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 0, i32 0, i32 -3, i32 5, i32 undef, i32 undef, i8 3)
  SignedOut[0] = (int2)sb;

  // Boolean bitfields convert to and from 0 and 1, and each keeps its own bit.
  BoolBits bb;
  bb.a = true;
  bb.b = false;
  bb.c = 3;

  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 0, i32 0, i32 1, i32 0, i32 3, i32 0, i8 15)
  BoolOut[0] = uint4((uint3)bb, 0);

  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 1, i32 0, i32 0, i32 1, i32 6, i32 0, i8 15)
  BoolBits bb2 = (BoolBits)uint3(0, 5, 6);
  BoolOut[1] = uint4(bb2.a, bb2.b, bb2.c, 0);

  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 2, i32 0, i32 1, i32 1, i32 2, i32 0, i8 15)
  BoolBits bb3 = (BoolBits)2;
  BoolOut[2] = uint4(bb3.a, bb3.b, bb3.c, 0);

  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 3, i32 0, i32 1, i32 0, i32 3, i32 0, i8 15)
  PlainBools pb = (PlainBools)bb;
  BoolOut[3] = uint4(pb.a, pb.b, pb.c, 0);

  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 4, i32 0, i32 1, i32 0, i32 3, i32 0, i8 15)
  BoolBits bb4 = {pb};
  BoolOut[4] = uint4(bb4.a, bb4.b, bb4.c, 0);

  // Arrays of structs with bitfields convert element by element.
  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 8, i32 0, i32 11, i32 12, i32 13, i32 14, i8 15)
  Pair arr[2] = (Pair[2])uint4(11, 12, 13, 14);
  Out[8] = uint4(arr[0].a, arr[0].b, arr[1].a, arr[1].b);

  // CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{[0-9]+}}, i32 9, i32 0, i32 11, i32 12, i32 13, i32 14, i8 15)
  Out[9] = (uint4)arr;
}

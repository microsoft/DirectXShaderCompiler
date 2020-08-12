// RUN: %dxc -DINTRIN=InterlockedAdd -T ps_6_6 %s | FileCheck %s -check-prefix=CHECK
// RUN: %dxc -DINTRIN=InterlockedMin -T ps_6_6 %s | FileCheck %s -check-prefix=CHECK
// RUN: %dxc -DINTRIN=InterlockedMax -T ps_6_6 %s | FileCheck %s -check-prefix=CHECK
// RUN: %dxc -DINTRIN=InterlockedAnd -T ps_6_6 %s | FileCheck %s -check-prefix=CHECK
// RUN: %dxc -DINTRIN=InterlockedOr -T ps_6_6 %s | FileCheck %s -check-prefix=CHECK
// RUN: %dxc -DINTRIN=InterlockedXor -T ps_6_6 %s | FileCheck %s -check-prefix=CHECK

// RUN: %dxc -DINTRIN=InterlockedAdd -T ps_6_5 %s | FileCheck %s -check-prefix=ERRCHECK
// RUN: %dxc -DINTRIN=InterlockedMin -T ps_6_5 %s | FileCheck %s -check-prefix=ERRCHECK
// RUN: %dxc -DINTRIN=InterlockedMax -T ps_6_5 %s | FileCheck %s -check-prefix=ERRCHECK
// RUN: %dxc -DINTRIN=InterlockedAnd -T ps_6_5 %s | FileCheck %s -check-prefix=ERRCHECK
// RUN: %dxc -DINTRIN=InterlockedOr -T ps_6_5 %s | FileCheck %s -check-prefix=ERRCHECK
// RUN: %dxc -DINTRIN=InterlockedXor -T ps_6_5 %s | FileCheck %s -check-prefix=ERRCHECK

// Verify that the first arg determines the overload and the others can be what they will

RWByteAddressBuffer res;

void main( uint a : A, uint b: B, uint c :C) : SV_Target
{
  uint uv = b - c;
  uint uv2 = b + c;
  int iv = b / c;
  int iv2 = b * c;
  uint64_t bb = b;
  uint64_t cc = c;
  uint64_t luv = bb * cc;
  uint64_t luv2 = bb / cc;
  int64_t liv = bb + cc;
  int64_t liv2 = bb - cc;
  uint ix = 0;

  // CHECK: call i32 @dx.op.atomicBinOp.i32
  // CHECK: call i32 @dx.op.atomicBinOp.i32
  // CHECK: call i64 @dx.op.atomicBinOp.i64
  // CHECK: call i64 @dx.op.atomicBinOp.i64
  // ERRCHECK: error: opcode '64-bit atomic operations' should only be used in 'Shader Model 6.6+'
  // ERRCHECK: error: opcode '64-bit atomic operations' should only be used in 'Shader Model 6.6+'
  res.INTRIN( ix++, uv, uv2 );
  res.INTRIN( ix++, iv, iv2 );
  res.INTRIN( ix++, luv, luv2 );
  res.INTRIN( ix++, liv, liv2 );

  // CHECK: call i32 @dx.op.atomicBinOp.i32
  // CHECK: call i32 @dx.op.atomicBinOp.i32
  // CHECK: call i64 @dx.op.atomicBinOp.i64
  // CHECK: call i64 @dx.op.atomicBinOp.i64
  // ERRCHECK: error: opcode '64-bit atomic operations' should only be used in 'Shader Model 6.6+'
  // ERRCHECK: error: opcode '64-bit atomic operations' should only be used in 'Shader Model 6.6+'
  res.INTRIN( ix++, iv, uv2 );
  res.INTRIN( ix++, uv, iv2 );
  res.INTRIN( ix++, liv, luv2 );
  res.INTRIN( ix++, luv, liv2 );

  // CHECK: call i32 @dx.op.atomicBinOp.i32
  // CHECK: call i32 @dx.op.atomicBinOp.i32
  // CHECK: call i64 @dx.op.atomicBinOp.i64
  // CHECK: call i64 @dx.op.atomicBinOp.i64
  // ERRCHECK: error: opcode '64-bit atomic operations' should only be used in 'Shader Model 6.6+'
  // ERRCHECK: error: opcode '64-bit atomic operations' should only be used in 'Shader Model 6.6+'
  res.INTRIN( ix++, uv, luv2 );
  res.INTRIN( ix++, iv, liv2 );
  res.INTRIN( ix++, luv, uv2 );
  res.INTRIN( ix++, liv, iv2 );
}

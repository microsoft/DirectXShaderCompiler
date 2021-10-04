// RUN: %dxc -T cs_6_6 %s | FileCheck %s

// A test to verify that 64-bit atomic binary operation intrinsics select the right variant

groupshared int64_t gs[256];
RWBuffer<int64_t> tb;
RWStructuredBuffer<int64_t> sb;
RWByteAddressBuffer rb;

groupshared uint64_t ugs[256];
RWBuffer<uint64_t> utb;
RWStructuredBuffer<uint64_t> usb;

[numthreads(1,1,1)]
void main( uint3 gtid : SV_GroupThreadID)
{
  uint a = gtid.x;
  uint b = gtid.y;
  uint64_t luv = a * b;
  int64_t liv = a + b;
  uint ix = 0;

  // GSCHECK: atomicrmw add i64 
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 0
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 0
  InterlockedAdd( gs[a], liv );
  InterlockedAdd( tb[a], liv );
  InterlockedAdd( sb[a], liv );
  rb.InterlockedAdd( ix++, liv );

  // GSCHECK: atomicrmw and i64 
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 1
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 1
  InterlockedAnd( gs[a], liv );
  InterlockedAnd( tb[a], liv );
  InterlockedAnd( sb[a], liv );
  rb.InterlockedAnd( ix++, liv );

  // GSCHECK: atomicrmw or i64 
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 2
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 2
  InterlockedOr( gs[a], liv );
  InterlockedOr( tb[a], liv );
  InterlockedOr( sb[a], liv );
  rb.InterlockedOr( ix++, liv );

  // GSCHECK: atomicrmw xor i64 
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 3
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 3
  InterlockedXor( gs[a], liv );
  InterlockedXor( tb[a], liv );
  InterlockedXor( sb[a], liv );
  rb.InterlockedXor( ix++, liv );

  // GSCHECK: atomicrmw min i64 
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 4
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 4
  InterlockedMin( gs[a], liv );
  InterlockedMin( tb[a], liv );
  InterlockedMin( sb[a], liv );
  rb.InterlockedMin( ix++, liv );

  // GSCHECK: atomicrmw max i64 
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 5
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 5
  InterlockedMax( gs[a], liv );
  InterlockedMax( tb[a], liv );
  InterlockedMax( sb[a], liv );
  rb.InterlockedMax( ix++, liv );

  // GSCHECK: atomicrmw umin i64 
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 6
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 6
  InterlockedMin( ugs[a], luv );
  InterlockedMin( utb[a], luv );
  InterlockedMin( usb[a], luv );
  rb.InterlockedMin( ix++, luv );

  // GSCHECK: atomicrmw umax i64 
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 7
  // CHECK: call i64 @dx.op.atomicBinOp.i64(i32 78, %dx.types.Handle %{{[0-9]*}}, i32 7
  InterlockedMax( ugs[a], luv );
  InterlockedMax( utb[a], luv );
  InterlockedMax( usb[a], luv );
  rb.InterlockedMax( ix++, luv );

}

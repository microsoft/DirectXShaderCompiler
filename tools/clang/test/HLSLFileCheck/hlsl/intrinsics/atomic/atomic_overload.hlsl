// RUN: %dxc -no-warnings -T vs_6_0 -DTYPE=double  %s | %FileCheck %s -check-prefixes=INTFAIL,FLTFAIL
// RUN: %dxc -no-warnings -T vs_6_2 -DTYPE=float16_t -enable-16bit-types  %s | %FileCheck %s -check-prefixes=INTFAIL,FLTFAIL
// RUN: %dxc -no-warnings -T vs_6_2 -DTYPE=int16_t -enable-16bit-types  %s | %FileCheck %s -check-prefixes=INTFAIL,FLTFAIL
// RUN: %dxc -no-warnings -T vs_6_2 -DTYPE=uint16_t -enable-16bit-types  %s | %FileCheck %s -check-prefixes=INTFAIL,FLTFAIL
// RUN: %dxc -no-warnings -T vs_6_0 -DTYPE=bool  %s | %FileCheck %s -check-prefixes=INTFAIL,FLTFAIL
// RUN: %dxc -no-warnings -T vs_6_0 -DTYPE=int64_t  %s | %FileCheck %s -check-prefixes=INTFAIL,FLTFAIL
// RUN: %dxc -no-warnings -T vs_6_0 -DTYPE=uint64_t  %s | %FileCheck %s -check-prefixes=INTFAIL,FLTFAIL

// RUN: %dxc -no-warnings -T vs_6_0 -DTYPE=float  %s | %FileCheck %s -check-prefixes=INTFAIL,
// RUN: %dxc -no-warnings -T vs_6_0 -DTYPE=half  %s | %FileCheck %s -check-prefixes=INTFAIL

// RUN: %dxc -no-warnings -T vs_6_0 -DTYPE=int  %s | %FileCheck %s -check-prefixes=INTCHK
// RUN: %dxc -no-warnings -T vs_6_0 -DTYPE=uint  %s | %FileCheck %s -check-prefixes=INTCHK


// Test various Interlocked ops using different memory types with invalid types

RWBuffer<TYPE> rw_res;
groupshared TYPE gs_res;
RWByteAddressBuffer ba_res;

float main() :OUT{
  int val = 1;
  TYPE comp = 1;
  TYPE orig;

  // add
  // INTFAIL: error: no matching function for call to 'InterlockedAdd'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedAdd'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedAdd'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedAdd'
  // INTFAIL: note: candidate function not viable: no known conversion from

  // INTCHK: call i32 @dx.op.atomicBinOp.i32
  // INTCHK: call i32 @dx.op.atomicBinOp.i32
  // INTCHK: atomicrmw add i32
  // INTCHK: atomicrmw add i32
  InterlockedAdd(rw_res[0], val);
  InterlockedAdd(rw_res[0], val, orig);
  InterlockedAdd(gs_res, val);
  InterlockedAdd(gs_res, val, orig);

  // min
  // INTFAIL: error: no matching function for call to 'InterlockedMin'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedMin'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedMin'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedMin'
  // INTFAIL: note: candidate function not viable: no known conversion from

  // INTCHK: call i32 @dx.op.atomicBinOp.i32
  // INTCHK: call i32 @dx.op.atomicBinOp.i32
  // INTCHK: atomicrmw {{u?}}min i32
  // INTCHK: atomicrmw {{u?}}min i32
  InterlockedMin(rw_res[0], val);
  InterlockedMin(rw_res[0], val, orig);
  InterlockedMin(gs_res, val);
  InterlockedMin(gs_res, val, orig);

  // max
  // INTFAIL: error: no matching function for call to 'InterlockedMax'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedMax'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedMax'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedMax'
  // INTFAIL: note: candidate function not viable: no known conversion from

  // INTCHK: call i32 @dx.op.atomicBinOp.i32
  // INTCHK: call i32 @dx.op.atomicBinOp.i32
  // INTCHK: atomicrmw {{u?}}max i32
  // INTCHK: atomicrmw {{u?}}max i32
  InterlockedMax(rw_res[0], val);
  InterlockedMax(rw_res[0], val, orig);
  InterlockedMax(gs_res, val);
  InterlockedMax(gs_res, val, orig);

  // and
  // INTFAIL: error: no matching function for call to 'InterlockedAnd'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedAnd'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedAnd'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedAnd'
  // INTFAIL: note: candidate function not viable: no known conversion from

  // INTCHK: call i32 @dx.op.atomicBinOp.i32
  // INTCHK: call i32 @dx.op.atomicBinOp.i32
  // INTCHK: atomicrmw and i32
  // INTCHK: atomicrmw and i32
  InterlockedAnd(rw_res[0], val);
  InterlockedAnd(rw_res[0], val, orig);
  InterlockedAnd(gs_res, val);
  InterlockedAnd(gs_res, val, orig);

  // or
  // INTFAIL: error: no matching function for call to 'InterlockedOr'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedOr'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedOr'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedOr'
  // INTFAIL: note: candidate function not viable: no known conversion from

  // INTCHK: call i32 @dx.op.atomicBinOp.i32
  // INTCHK: call i32 @dx.op.atomicBinOp.i32
  // INTCHK: atomicrmw or i32
  // INTCHK: atomicrmw or i32
  InterlockedOr(rw_res[0], val);
  InterlockedOr(rw_res[0], val, orig);
  InterlockedOr(gs_res, val);
  InterlockedOr(gs_res, val, orig);

  // xor
  // INTFAIL: error: no matching function for call to 'InterlockedXor'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedXor'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedXor'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedXor'
  // INTFAIL: note: candidate function not viable: no known conversion from

  // INTCHK: call i32 @dx.op.atomicBinOp.i32
  // INTCHK: call i32 @dx.op.atomicBinOp.i32
  // INTCHK: atomicrmw xor i32
  // INTCHK: atomicrmw xor i32
  InterlockedXor(rw_res[0], val);
  InterlockedXor(rw_res[0], val, orig);
  InterlockedXor(gs_res, val);
  InterlockedXor(gs_res, val, orig);

  // compareStore
  // INTFAIL: error: no matching function for call to 'InterlockedCompareStore'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedCompareStore'
  // INTFAIL: note: candidate function not viable: no known conversion from

  // INTCHK: call i32 @dx.op.atomicCompareExchange.i32
  // INTCHK: cmpxchg i32
  InterlockedCompareStore(rw_res[0], comp, val);
  InterlockedCompareStore(gs_res, comp, val);

  // exchange
  // FLTFAIL: error: no matching function for call to 'InterlockedExchange'
  // FLTFAIL: note: candidate function not viable: no known conversion from
  // FLTFAIL: error: no matching function for call to 'InterlockedExchange'
  // FLTFAIL: note: candidate function not viable: no known conversion from

  // INTCHK: call i32 @dx.op.atomicBinOp.i32
  // INTCHK: atomicrmw xchg i32
  InterlockedExchange(rw_res[0], val, orig);
  InterlockedExchange(gs_res, val, orig);

  // compareExchange
  // INTFAIL: error: no matching function for call to 'InterlockedCompareExchange'
  // INTFAIL: note: candidate function not viable: no known conversion from
  // INTFAIL: error: no matching function for call to 'InterlockedCompareExchange'
  // INTFAIL: note: candidate function not viable: no known conversion from

  // INTCHK: call i32 @dx.op.atomicCompareExchange.i32
  // INTCHK: cmpxchg i32
  InterlockedCompareExchange(rw_res[0], comp, val, orig);
  InterlockedCompareExchange(gs_res, comp, val, orig);

  return (float)rw_res[0] + gs_res;
}

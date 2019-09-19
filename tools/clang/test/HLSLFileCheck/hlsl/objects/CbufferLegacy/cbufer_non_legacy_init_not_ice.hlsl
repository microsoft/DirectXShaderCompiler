// RUN: %dxc -E main -T vs_6_0  -not_use_legacy_cbuf_load %s 2>&1 | FileCheck %s

// CHECK: ; cbuffer $Globals
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct $Globals
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       uint CB_One;                                  ; Offset:    0
// CHECK: ;
// CHECK: ;   } $Globals;                                       ; Offset:    0 Size:     4
// CHECK: ;
// CHECK: ; }

// Make sure it compiles, and make sure cbuffer load isn't optimized away
// CHECK: call i32 @dx.op.cbufferLoad.i32(i32 58, %dx.types.Handle %"$Globals_cbuffer", i32 0, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)

// The following is const by default for cbuffer, but not the way clang normally
// interprets a const initialized global, since the initializer will be thrown away.
uint CB_One = 1;

// Simplified repro:
int2 main() : OUTPUT {
  const uint ConstVal = CB_One;
  return int(ConstVal);
}

int2 main2(int2 input : INPUT) : OUTPUT {
  const uint ConstFactor = CB_One;
  int2 result = input;
  [unroll]
  for (uint LoopIdx = 0; LoopIdx < 2; LoopIdx++)
  {
    const int2 Offset = (LoopIdx == 0 ? 1 : -1) * (int(ConstFactor) * result);
    result += Offset;
  }
  return result;
}


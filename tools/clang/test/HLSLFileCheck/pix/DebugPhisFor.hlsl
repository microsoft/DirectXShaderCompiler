// RUN: %dxc -EForLoopPS -Tps_6_0 %s -Od | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation | %FileCheck %s

// Ensure that the pass added at the begining of the for body:
// CHECK: br label %PIXDebug
// CHECK: br label %PIXDebug
// CHECK: br label %PIXDebug

// Followed by lots of new pix debug blocks:

// CHECK: PIXDebug
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78
// CHECK: br label

// CHECK: PIXDebug
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78
// CHECK: br label

// CHECK: PIXDebug
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78
// CHECK: br label

// CHECK: PIXDebug
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78
// CHECK: br label

// CHECK: PIXDebug
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78
// CHECK: br label


struct VS_OUTPUT_ENV {
  float4 Pos : SV_Position;
  float2 Tex : TEXCOORD0;
};

uint i32;

float4 ForLoopPS(VS_OUTPUT_ENV input) : SV_Target {
  float4 ret = float4(0, 0, 0, 0);
  for (uint i = 0; i < abs(input.Tex.x * 200); ++i) {
    ret.x += (float)i32;
    if (i + i32 == 0) {
      break;
    }
    ret.y += (float)i32;
    if (i + i32 == 1) {
      continue;
    }
    ret.z += (float)i32;
    if (i + i32 == 2) {
      break;
    }
    ret.w += (float)i32;
    if (i + i32 == 3) {
      continue;
    }
  }
  return ret;
}

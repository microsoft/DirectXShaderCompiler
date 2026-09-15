// RUN: %dxc -T vs_6_9 %s | FileCheck %s
// Tests a specific case of a precise native vector requiring extraction
// and reinsertion during the alloca phase where conditionalmem2reg is concerned.
// Serves as the source for longvec-precise.ll and its specific pass tests

precise float4 main (float4 pos : POSITION, float4 scale : SCL, float4 shift : OFF) : SV_Position {
  precise float4 position = pos;
  // Initial multiplication to avoid optimizaton just using the input scalar
  // CHECK-NOT: fmul fast
  // CHECK: [[pos:%.*]] = fmul <4 x float>
  position = position * scale;

  // CHECK: [[z:%.*]] = extractelement <4 x float> [[pos]], i32 2
  // CHECK-NOT: fadd fast
  // CHECK: [[sz:%.*]] = fadd float [[z]], 0x
  // CHECK: [[spos:%.*]] = insertelement <4 x float> [[pos]], float [[sz]], i32 2
  position.z += 0.01f;
  // CHECK-NOT: fadd fast
  // CHECK: fadd <4 x float> %{{.*}}, [[spos]]
  position += shift;

  return position;
}

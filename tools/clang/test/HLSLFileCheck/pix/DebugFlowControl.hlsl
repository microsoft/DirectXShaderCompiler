// RUN: %dxc -EFlowControlPS -Tps_6_0 %s | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation | %FileCheck %s

// Check that flow control constructs don't break the instrumentation.

// check instrumentation for one branch. 

// CHECK:  %UAVInc Result2 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_DebugUAV_Handle, i32 0, i32 0, i32 undef, i32 undef, i32 %IncrementForThisInvocation1)
// CHECK:  %MaskedForUAVLimit3 = and i32 %UAVIncResult2, 983039
// CHECK:  %MultipliedForInterest4 = mul i32 %MaskedForUAVLimit3, %OffsetMultiplicand
// CHECK:  %AddedForInterest5 = add i32 %MultipliedForInterest4, %OffsetAddend
// CHECK:  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %PIX_DebugUAV_Handle, i32 %AddedForInterest5, i32 undef, i32 64771, i32 undef, i32 undef, i32 undef, i8 1)
// CHECK:  switch i32
// CHECK:    i32 0, label 
// CHECK:    i32 32, label
// CHECK:  ]

int i32;
float f32;

float4 Vectorize(float f)
{
  return float4((float)f / 128.f, (float)f / 128.f, (float)f / 128.f, 1.f);
}

float4 FlowControlPS() : SV_Target
{
  float4 ret = { 0,0,0,1 };
  switch (i32)
  {
  case 0:
    ret = float4(1, 0, 1, 1);
    break;
  case 32:
    ret = Vectorize(f32);
    break;
  }

  if (i32 > 10)
  {
    ret.r += 0.1f;
  }
  else
  {
    ret.g += 0.1f;
  }

  for (uint i = 0; i < 3; ++i)
  {
    ret.b += (float)i32 / 10.f;
  }

  return ret;
}
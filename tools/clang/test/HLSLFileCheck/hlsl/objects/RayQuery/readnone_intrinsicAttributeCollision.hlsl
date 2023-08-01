// RUN: %dxc -T ps_6_5 -E PS -fcgl %s | FileCheck %s
// RUN: %dxc -T ps_6_5 -E PS %s | FileCheck %s -check-prefix=CHECKDXIL

// IncrementCounter called before GetRenderTargetSampleCount.
// Don't be sensitive to HL Opcode because those can change.
// CHECK: call i32 [[GetRenderTargetSampleCount:@"[^"]+"]](i32
// CHECK: call i32 [[IncrementCounter:@"[^"]+"]](i32

// Ensure HL declarations are not collapsed when attributes differ
// CHECK-DAG: declare i32 [[GetRenderTargetSampleCount]]({{.*}}) #[[AttrGetRenderTargetSampleCount:[0-9]+]]
// CHECK-DAG: declare i32 [[IncrementCounter]]({{.*}}) #[[AttrIncrementCounter:[0-9]+]]

// Ensure correct attributes for each HL intrinsic
// CHECK-DAG: attributes #[[AttrGetRenderTargetSampleCount]] = { nounwind readnone }
// CHECK-DAG: attributes #[[AttrIncrementCounter]] = { nounwind }

// Ensure GetRenderTargetSampleCount not eliminated in final DXIL:
// CHECKDXIL: call i32 @dx.op.renderTargetGetSampleCount(
// CHECKDXIL: call i32 @dx.op.bufferUpdateCounter(

RaytracingAccelerationStructure AccelerationStructure : register(t0);
RWByteAddressBuffer log : register(u0);

StructuredBuffer<int> buf[]: register(t3);
RWStructuredBuffer<int> uav;

// test read-none attr
int PS(int a : A, int b : B) : SV_Target
{
  int res = 0;
  
  for (;;) {    
    uint x = GetRenderTargetSampleCount();
    x += uav.IncrementCounter();
    
    if (a != x) {
      res += buf[(int)x][b];
      break;
    }
  }
  return res;
}

// RUN: %dxc -T cs_6_9 %s | FileCheck %s
// https://github.com/microsoft/DirectXShaderCompiler/issues/7915
// Test long vector casting between uint2 and float2
// which would crash as reported by a user.

// CHECK: call %dx.types.Handle @dx.op.createHandleFromBinding
// CHECK: fptoui
// CHECK: uitofp
// CHECK: fptoui
// CHECK: uitofp
// CHECK: call void @dx.op.rawBufferVectorStore.v2f32

RWStructuredBuffer<float2> input;
RWStructuredBuffer<float2> output;

float2 f1(uint2 p) { return p; }
uint2 f2(float2 p) { return f1(p); }

[numthreads(1,1,1)]
void main()
{
  output[0] = f2(input[0]);
}

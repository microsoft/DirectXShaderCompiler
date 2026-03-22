// RUN: %dxc -T cs_6_0 -E main -O3 -Vd %s -spirv | FileCheck %s

// CHECK: OpLoopMerge {{%[0-9]+}} {{%[0-9]+}} Unroll
// CHECK: [[which:%[0-9]+]] = OpSelect %_ptr_Uniform_type_StructuredBuffer_S {{%[0-9]+}} %gSBuffer1 %gSBuffer2
// CHECK: [[idx:%[0-9]+]] = OpBitcast %uint {{%[0-9]+}}
// CHECK: [[src:%[0-9]+]] = OpAccessChain %_ptr_Uniform_S [[which]] %int_0 [[idx]]
// CHECK: [[val:%[0-9]+]] = OpLoad %S [[src]]
// CHECK: [[dst:%[0-9]+]] = OpAccessChain %_ptr_Uniform_S %gRWSBuffer %int_0 [[idx]]
// CHECK: OpStore [[dst]] [[val]]

struct S {
  float4 f;
};

struct CombinedBuffers {
  StructuredBuffer<S> SBuffer;
  RWStructuredBuffer<S> RWSBuffer;
};

StructuredBuffer<S> gSBuffer1;
StructuredBuffer<S> gSBuffer2;
RWStructuredBuffer<S> gRWSBuffer;

#define constant 0

[numthreads(1,1,1)]
void main() {

  StructuredBuffer<S> lSBuffer;

  [unroll]
  for( int j = 0; j < 2; j++ ) {
    if (constant > j) {
      lSBuffer = gSBuffer1;
    } else {
      lSBuffer = gSBuffer2;
    }
    gRWSBuffer[j] = lSBuffer[j];
  }
}

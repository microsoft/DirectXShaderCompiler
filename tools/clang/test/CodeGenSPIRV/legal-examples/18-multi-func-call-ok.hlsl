// Run: %dxc -T cs_6_0 -E main -O3

// CHECK:      [[src:%\d+]] = OpAccessChain %_ptr_Uniform_S %gSBuffer
// CHECK-NEXT: [[val:%\d+]] = OpLoad %S [[src]]
// CHECK-NEXT: [[dst:%\d+]] = OpAccessChain %_ptr_Uniform_S %gRWSBuffer1
// CHECK-NEXT:                OpStore [[dst]] [[val]]
// CHECK-NEXT: [[val:%\d+]] = OpLoad %S [[src]]
// CHECK-NEXT: [[dst:%\d+]] = OpAccessChain %_ptr_Uniform_S %gRWSBuffer2
// CHECK-NEXT:                OpStore [[dst]] [[val]]

struct S {
  float4 f;
};

int i;

StructuredBuffer<S> gSBuffer;
RWStructuredBuffer<S> gRWSBuffer1;
RWStructuredBuffer<S> gRWSBuffer2;


void foo(RWStructuredBuffer<S> pRWSBuffer) {
  pRWSBuffer[i] = gSBuffer[i];
}

void main() {
  foo(gRWSBuffer1);
  foo(gRWSBuffer2);
}

// Run: %dxc -T cs_6_0 -E main -O3

// CHECK:      [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_S %gSBuffer
// CHECK-NEXT: [[val:%\d+]] = OpLoad %S [[ptr]]
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_S %gRWSBuffer
// CHECK-NEXT:                OpStore [[ptr]] [[val]]

struct S {
  float4 f;
};

struct CombinedBuffers {
  StructuredBuffer<S> SBuffer;
  RWStructuredBuffer<S> RWSBuffer;
};


int i;

StructuredBuffer<S> gSBuffer;
RWStructuredBuffer<S> gRWSBuffer;

void foo(StructuredBuffer<S> pSBuffer) {
  gRWSBuffer[i] = pSBuffer[i];
}

void main() {
  foo(gSBuffer);
}

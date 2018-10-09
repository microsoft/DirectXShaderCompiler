// Run: %dxc -T cs_6_0 -E main -O3

// TODO: This example is expected to trigger validation failure. The validator
// is not checking it yet.

// CHECK: OpPhi %_ptr_Uniform_type_StructuredBuffer_S %gSBuffer1 {{%\d+}} %gSBuffer2 {{%\d+}}

struct S {
  float4 f;
};

struct CombinedBuffers {
  StructuredBuffer<S> SBuffer;
  RWStructuredBuffer<S> RWSBuffer;
};


int i;

StructuredBuffer<S> gSBuffer1;
StructuredBuffer<S> gSBuffer2;
RWStructuredBuffer<S> gRWSBuffer;

#define constant 0

void main() {

  StructuredBuffer<S> lSBuffer;
  switch(i) {                   // Compiler can't determine which case will run.
    case 0:                     // Will produce invalid SPIR-V for Vulkan.
      lSBuffer = gSBuffer1;
      break;
    default:
      lSBuffer = gSBuffer2;
      break;
  }
  gRWSBuffer[i] = lSBuffer[i];
}

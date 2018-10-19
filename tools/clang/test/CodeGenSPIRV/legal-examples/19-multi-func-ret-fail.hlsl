// Run: %dxc -T cs_6_0 -E main -O3

// CHECK: Using pointers with OpPhi requires capability

struct S {
  float4 f;
};

int i;

StructuredBuffer<S> gSBuffer;
RWStructuredBuffer<S> gRWSBuffer1;
RWStructuredBuffer<S> gRWSBuffer2;

RWStructuredBuffer<S> foo(int l) {
  if (l == 0) {       // Compiler does not know which branch will be taken:
                      // Branch taken depends on input i.
    return gRWSBuffer1;
  } else {
    return gRWSBuffer2;
  }
}

void main() {
  RWStructuredBuffer<S> lRWSBuffer = foo(i);
  lRWSBuffer[i] = gSBuffer[i];
}

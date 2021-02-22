// RUN: %dxc -T cs_6_6 %s | %FileCheck %s

// Make sure nonUniformIndex is true.
// CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 218, i32 %{{[0-9]+}}, i1 false, i1 true)
// CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 218, i32 %{{[0-9]+}}, i1 false, i1 true)

float read(uint ID) {
  Buffer<float> buf = ResourceDescriptorHeap[NonUniformResourceIndex(ID)];
  return buf[0];
}

void write(uint ID, float f) {
  RWBuffer<float> buf = ResourceDescriptorHeap[NonUniformResourceIndex(ID)];
  buf[0] = f;
}

[numthreads(8, 8, 1)]
void main( uint2 ID : SV_DispatchThreadID) {
  float v = read(ID.x);
  write(ID.y, v);
}
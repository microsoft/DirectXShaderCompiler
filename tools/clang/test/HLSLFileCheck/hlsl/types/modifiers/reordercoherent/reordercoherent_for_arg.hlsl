// RUN: %dxc -E main -T lib_6_9 %s | FileCheck %s

// Make sure not crash.
// CHECK:call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32

RWBuffer<float> OutBuf : register(u1);
reordercoherent RWBuffer<float> u : register(u2);

float read(RWBuffer<float> buf) {
  return buf[0];
}

[shader("raygeneration")]
void main() {
  OutBuf[0] = read(u);
}
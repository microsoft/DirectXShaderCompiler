// RUN: %dxc -T ps_6_3 %s | FileCheck %s
StructuredBuffer<int> buf[]: register(t2);

// CHECK: @dx.break = internal global

// Simple test to verify functionality in pixel shaders
// CHECK: load volatile i32
// CHECK-SAME: @dx.break
// CHECK-NEXT: icmp eq i32

// CHECK: call i32 @dx.op.waveReadLaneFirst

// These verify the first break block keeps the conditional
// CHECK: call %dx.types.Handle @dx.op.createHandle
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad
// CHECK: add
// CHECK: br i1
// These verify the second break block doesn't
// CHECK: call %dx.types.Handle @dx.op.createHandle
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad
// CHECK: add
// CHECK-NOT: br i1
// CHECK: call void @dx.op.storeOutput

int main(int a : A, int b : B) : SV_Target
{
  int res = 0;
  int i = 0;
  int u = 0;

  for (;;) {
      u += WaveReadLaneFirst(a);
      if (a == u) {
          res += buf[u][b];
          break;
        }
    }
  for (;;) {
      if (a == u) {
          res += buf[b][u];
          break;
        }
    }
  return res;
}

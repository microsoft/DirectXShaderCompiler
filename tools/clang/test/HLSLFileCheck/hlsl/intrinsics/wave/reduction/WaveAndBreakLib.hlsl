// RUN: %dxc -T lib_6_3 %s | FileCheck %s
StructuredBuffer<int> buf[]: register(t2);
// CHECK: @dx.break = internal global

// Cannonical example. Expected to keep the block in loop
// Verify this function loads the global
// CHECK: load volatile i32
// CHECK-SAME: @dx.break
// CHECK: icmp eq i32

// CHECK: call i32 @dx.op.waveReadLaneFirst

// These verify the break block keeps the conditional
// CHECK: call %dx.types.Handle @"dx.op.createHandleForLib.class.StructuredBuffer<int>"
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad
// CHECK: add
// CHECK: br i1
export
int WaveInLoop(int a : A, int b : B)
{
  int res = 0;
  int i = 0;

  for (;;) {
      int u = WaveReadLaneFirst(a);
      if (a == u) {
          res += buf[b][u];
          break;
        }
    }
  return res;
}

// Wave moved to after the break block. Expected to keep the block in loop
// Verify this function loads the global
// CHECK: load volatile i32
// CHECK-SAME: @dx.break
// CHECK: icmp eq i32

// These verify the break block keeps the conditional
// CHECK: call %dx.types.Handle @"dx.op.createHandleForLib.class.StructuredBuffer<int>"
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad
// CHECK: add
// CHECK: br i1

// CHECK: call i32 @dx.op.waveReadLaneFirst
export
int WaveInPostLoop(int a : A, int b : B)
{
  int res = 0;
  int i = 0;
  int u = 0;

  for (;;) {
      if (a == u) {
          res += buf[b][u];
          break;
        }
      u += WaveReadLaneFirst(a);
    }
  return res;
}

// Wave in entry block. Expected to allow the break block to move out of loop
// CHECK: call i32 @dx.op.waveReadLaneFirst

// Verify this function loads the global
// CHECK: load volatile i32
// CHECK-SAME: @dx.break
// CHECK: icmp eq i32

// These verify the break block doesn't keep the conditional
// CHECK: call %dx.types.Handle @"dx.op.createHandleForLib.class.StructuredBuffer<int>"
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad
export
int WaveInEntry(int a : A, int b : B)
{
  int res = 0;
  int i = 0;

  int u = WaveReadLaneFirst(a);

  for (;;) {
      if (a == u) {
          res += buf[b][u];
          break;
        }
    }
  return res;
}

// Wave in subloop of larger loop. Expected to keep the block in loop
// Verify this function loads the global
// CHECK: load volatile i32
// CHECK-SAME: @dx.break
// CHECK: icmp eq i32

// CHECK: call i32 @dx.op.waveReadLaneFirst

// These verify the break block keeps the conditional
// CHECK: call %dx.types.Handle @"dx.op.createHandleForLib.class.StructuredBuffer<int>"
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad
// CHECK: add
// CHECK: br i1
export
int WaveInSubLoop(int a : A, int b : B)
{
  int res = 0;
  int i = 0;

  for (;;) {
      int u = 0;
      for (int i = 0; i < b; i ++)
        u += WaveReadLaneFirst(a);
      if (a == u) {
          res += buf[b][u];
          break;
        }
    }
  return res;
}

// Wave in a separate loop. Expected to allow the break block to move out of loop
export
// CHECK: load volatile i32
// CHECK: icmp eq i32

// CHECK: call i32 @dx.op.waveReadLaneFirst

// These verify the first break block keeps the conditional
// CHECK: call %dx.types.Handle @"dx.op.createHandleForLib.class.StructuredBuffer<int>"
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad
// CHECK: add
// CHECK: br i1
// These verify the second break block doesn't
// CHECK: call %dx.types.Handle @"dx.op.createHandleForLib.class.StructuredBuffer<int>"
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad
// CHECK: add
// CHECK-NOT: br i1
int WaveInOtherLoop(int a : A, int b : B)
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

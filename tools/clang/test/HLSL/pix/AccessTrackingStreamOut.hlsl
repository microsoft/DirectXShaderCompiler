// RUN: %dxc -Emain -Tgs_6_0 %s | %opt -S -hlsl-dxil-pix-shader-access-instrumentation,config=O0:1:3i1;.. | %FileCheck %s

// Check for udpate of UAV for each stream (last column is byte offset into UAV, indexed by stream #)
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_CountUAV_Handle, i32 2, i32 4
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_CountUAV_Handle, i32 2, i32 8
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_CountUAV_Handle, i32 2, i32 12

struct MyStruct
{
  float4 pos : SV_Position;
  float2 a : AAA;
};

struct MyStruct2
{
  uint3 X : XXX;
  float4 p[3] : PPP;
  uint3 Y : YYY;
};

[maxvertexcount(12)]
void main(point float4 array[1] : COORD, inout PointStream<MyStruct> OutputStream0,
  inout PointStream<MyStruct2> OutputStream1,
  inout PointStream<MyStruct> OutputStream2)
{
  float4 r = array[0];
  MyStruct a = (MyStruct)0;
  MyStruct2 b = (MyStruct2)0;
  a.pos = array[r.x];
  a.a = r.xy;
  b.X = r.xyz;
  b.Y = a.pos.xyz;
  b.p[2] = a.pos * 44;

  OutputStream0.Append(a);
  OutputStream0.RestartStrip();

  OutputStream1.Append(b);
  OutputStream1.RestartStrip();

  OutputStream2.Append(a);
  OutputStream2.RestartStrip();
}
// RUN: %dxc -E main -T gs_6_0 %s | FileCheck %s

// Make sure load input has precise.
// CHECK: OutputTopology=point

struct MyStruct
{
  precise  float4 pos : SV_Position;
};


[maxvertexcount(12)]
void main(point float4 array[1] : COORD, inout PointStream<MyStruct> OutputStream0)
{
  float4 r = array[0];
  MyStruct a = (MyStruct)0;

  a.pos = array[r.x] + 1;

  OutputStream0.Append(a);
  OutputStream0.RestartStrip();

}

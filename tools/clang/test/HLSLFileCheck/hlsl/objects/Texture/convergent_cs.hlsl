// RUN: %dxc -E MainCS -T cs_6_6 %s | FileCheck %s
// RUN: %dxc -E MainAS -T as_6_6 %s | FileCheck %s
// RUN: %dxc -E MainMS -T ms_6_6 %s | FileCheck %s

// Make sure add is not sunk into if.
// Compute shader variant of convergent.hlsl

// CHECK: add
// CHECK: add
// CHECK: icmp
// CHECK-NEXT: br


Texture2D<float4> tex;
RWBuffer<float4> output;
SamplerState s;

void doit(uint ix, uint3 id){

  float2 coord = id.xy + id.z;
  float4 c = id.z;
  if (id.z > 2) {
    c += tex.Sample(s, coord);
  }
  output[ix] = c;

}

[numthreads(4,4,4)]
void MainCS(uint ix : SV_GroupIndex, uint3 id : SV_GroupThreadID) {
  doit(ix, id);
}

struct Payload { int nothing; };

[numthreads(4,4,4)]
void MainAS(uint ix : SV_GroupIndex, uint3 id : SV_GroupThreadID) {
  doit(ix, id);
  Payload pld = (Payload)0;
  DispatchMesh(1,1,1,pld);
}


[numthreads(4,4,4)]
[outputtopology("triangle")]
void MainMS(uint ix : SV_GroupIndex, uint3 id : SV_GroupThreadID) {
  doit(ix, id);
}
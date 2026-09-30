// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -Od %s | FileCheck %s

// Out and inout matrix parameters whose argument has a different element type
// are converted like an explicit matrix cast when copied in and out, instead
// of hitting an assert in the scalar/vector conversion.

StructuredBuffer<float4> In : register(t0);
StructuredBuffer<int4> InInt : register(t1);
RWStructuredBuffer<float4> Out : register(u0);
RWStructuredBuffer<uint4> OutUint : register(u1);

void fill(out float2x2 m) { m = float2x2(In[0].xy, In[0].zw); }
void twice(inout float2x2 m) { m = m * 2; }
void fillInt(out int2x2 m) { m = int2x2(InInt[0].xy, InInt[0].zw); }

[numthreads(1, 1, 1)]
void main() {
  // out float2x2 -> half2x2: converted when copied back.
  // CHECK: fptrunc float %{{.*}} to half
  // CHECK: fptrunc float %{{.*}} to half
  // CHECK: fptrunc float %{{.*}} to half
  // CHECK: fptrunc float %{{.*}} to half
  half2x2 h;
  fill(h);
  Out[0] = float4(h[0], h[1]);

  // inout half2x2 -> float2x2: converted on the way in and on the way out.
  // CHECK: rawBufferLoad.f32
  // CHECK: fpext half %{{.*}} to float
  // CHECK: fmul fast float %{{.*}}, 2.000000e+00
  // CHECK: fptrunc float %{{.*}} to half
  half2x2 m = half2x2(In[1].xy, In[1].zw);
  twice(m);
  Out[1] = float4(m[0], m[1]);

  // out int2x2 -> uint2x2: same bits, no conversion needed.
  // CHECK: rawBufferLoad.i32
  // CHECK: rawBufferStore.i32
  uint2x2 u;
  fillInt(u);
  OutUint[0] = uint4(u[0], u[1]);
}

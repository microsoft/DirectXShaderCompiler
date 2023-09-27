// RUN: %dxc -T lib_6_3 %s -verify

struct Payload {
    float2 dummy;
};

//[numthreads(8, 1, 1)]
[shader("amplification")]
void ASmain() /* expected-error{{amplification entry point must have the numthreads attribute}} */
{
    Payload pld;
    pld.dummy = float2(1.0,2.0);
    DispatchMesh(8, 1, 1, pld);
}

[shader("hull")]
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
//[patchconstantfunc("HSPatch")]
float4 HSmain(uint ix : SV_OutputControlPointID) /* expected-error{{hull entry point must have the patchconstantfunc attribute}} */
{
  return 0;
}

struct VSOut {
  float4 pos : SV_Position;
};

//[maxvertexcount(3)]
[shader("geometry")]
void GSmain(inout TriangleStream<VSOut> stream) { /* expected-error{{geometry entry point must have the maxvertexcount attribute}} */
  VSOut v = {0.0, 0.0, 0.0, 0.0};
  stream.Append(v);
  stream.RestartStrip();
}

struct myvert {
    float4 position : SV_Position;
};


[shader("mesh")]
//[NumThreads(8, 8, 2)]
//[OutputTopology("triangle")]
void MSmain(out vertices myvert verts[32], /* expected-error{{mesh entry point must have the numthreads attribute}} */ /* expected-error{{mesh entry point must have the outputtopology attribute}} */
          uint ix : SV_GroupIndex) {
  SetMeshOutputCounts(32, 16);
  myvert v = {0.0, 0.0, 0.0, 0.0};
  verts[ix] = v;
}

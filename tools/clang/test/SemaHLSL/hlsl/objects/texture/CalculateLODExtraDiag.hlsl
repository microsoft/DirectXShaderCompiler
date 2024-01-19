// RUN: %dxc -T lib_6_8 %s -verify

// Check cs/as/mesh and node.

SamplerState ss : register(s2);

RWStructuredBuffer<uint> o;
Texture1D        <float>  t1;

// expected-note@+3{{declared here}}
[numthreads(3,8,1)]
[shader("compute")]
void foo(uint3 id : SV_GroupThreadID)
{
    // expected-error@+1 {{Intrinsic CalculateLevelOfDetail called by foo requires derivatives - when used in compute/amplification/mesh shaders or broadcast nodes, numthreads must be either 1D with X as a multiple of 4 or both X and Y must be multiples of 2}}
    o[0] = t1.CalculateLevelOfDetail(ss, 0.5);
}

// expected-note@+3{{declared here}}
[numthreads(3,1,1)]
[shader("compute")]
void bar(uint3 id : SV_GroupThreadID)
{
    // expected-error@+1 {{Intrinsic CalculateLevelOfDetail called by bar requires derivatives - when used in compute/amplification/mesh shaders or broadcast nodes, numthreads must be either 1D with X as a multiple of 4 or both X and Y must be multiples of 2}}
    o[0] = t1.CalculateLevelOfDetail(ss, 0.5);
}

// expected-note@+4{{declared here}}
[shader("mesh")]
[numthreads(3,1,1)]
[outputtopology("triangle")]
void mesh(uint ix : SV_GroupIndex, uint3 id : SV_GroupThreadID) {
    // expected-error@+1 {{Intrinsic CalculateLevelOfDetail called by mesh requires derivatives - when used in compute/amplification/mesh shaders or broadcast nodes, numthreads must be either 1D with X as a multiple of 4 or both X and Y must be multiples of 2}}
    o[0] = t1.CalculateLevelOfDetail(ss, 0.5);
}


struct Payload {
    float2 dummy;
};

// expected-note@+3{{declared here}}
[numthreads(3, 2, 1)]
[shader("amplification")]
void ASmain()
{
    // expected-error@+1 {{Intrinsic CalculateLevelOfDetail called by ASmain requires derivatives - when used in compute/amplification/mesh shaders or broadcast nodes, numthreads must be either 1D with X as a multiple of 4 or both X and Y must be multiples of 2}}
    o[0] = t1.CalculateLevelOfDetail(ss, 0.5);
    Payload pld;
    pld.dummy = float2(1.0,2.0);
    DispatchMesh(8, 1, 1, pld);
}

struct RECORD {
  uint a;
};

// expected-note@+5{{declared here}}
[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1,1,1)]
void node01(DispatchNodeInputRecord<RECORD> input) {
    // expected-error@+1 {{Intrinsic CalculateLevelOfDetail called by node01 requires derivatives - when used in compute/amplification/mesh shaders or broadcast nodes, numthreads must be either 1D with X as a multiple of 4 or both X and Y must be multiples of 2}}
    o[0] = t1.CalculateLevelOfDetail(ss, 0.5);
 }

// expected-note@+5{{declared here}}
[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(1024,1,1)]
[NodeIsProgramEntry]
void node02()
{
    // expected-error@+1 {{Intrinsic CalculateLevelOfDetail called by node02 requires derivatives - only available in pixel, compute, amplification, mesh, or broadcast node shaders}}
    o[0] = t1.CalculateLevelOfDetail(ss, 0.5);
}


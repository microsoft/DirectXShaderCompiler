// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 %s -verify

// expected-no-diagnostics

RWByteAddressBuffer buf;
void CallFunction()
{
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat;
  __builtin_LinAlg_MatrixLoadFromDescriptor(mat, buf, 5, 5, 5);
}

// --- Allowed Stages ---

[shader("compute")]
[numthreads(4,4,4)]
void mainCS(uint ix : SV_GroupIndex, uint3 id : SV_GroupThreadID) {
  CallFunction();
}

struct Verts {
    float4 position : SV_Position;
};

[shader("mesh")]
[NumThreads(8, 8, 2)]
[OutputTopology("triangle")]
void mainMeS(out vertices Verts verts[32], uint ix : SV_GroupIndex) {
  CallFunction();
  SetMeshOutputCounts(32, 16);
  Verts v = {0.0, 0.0, 0.0, 0.0};
  verts[ix] = v;
}

struct AmpPayload {
    float2 dummy;
};

[numthreads(8, 1, 1)]
[shader("amplification")]
void mainAS()
{
    CallFunction();
    AmpPayload pld;
    pld.dummy = float2(1.0,2.0);
    DispatchMesh(8, 1, 1, pld);
}

[shader("pixel")]
float4 mainPS(uint ix : SV_PrimitiveID) : SV_TARGET {
  CallFunction();
  return 1.0;
}

[shader("vertex")]
float4 mainVS(uint ix : SV_VertexID) : OUT {
  CallFunction();
  return 1.0;
}

[shader("node")]
[nodedispatchgrid(8,1,1)]
[numthreads(64,2,2)]
void mainNS() {
  CallFunction();
}

[shader("raygeneration")]
void mainRG() {
  CallFunction();
}

[shader("intersection")]
void mainIS() {
  CallFunction();
}

struct Attribs { float2 barys; };

[shader("callable")]
void mainCALL(inout Attribs attrs) {
  CallFunction();
}

struct [raypayload] RayPayload
{
    float elem
          : write(caller,closesthit,anyhit,miss)
          : read(caller,closesthit,anyhit,miss);
};

[shader("anyhit")]
void mainAH(inout RayPayload pld, in Attribs attrs) {
 CallFunction();
}

[shader("closesthit")]
void mainCH(inout RayPayload pld, in Attribs attrs) {
  CallFunction();
}

[shader("miss")]
void mainMS(inout RayPayload pld) {
  CallFunction();
}

struct PosStruct {
  float4 pos : SV_Position;
};

struct PCStruct
{
  float Edges[3]  : SV_TessFactor;
  float Inside : SV_InsideTessFactor;
  float4 test : TEST;
};

PCStruct HSPatch(InputPatch<PosStruct, 3> ip,
                 OutputPatch<PosStruct, 3> op,
                 uint ix : SV_PrimitiveID)
{
  PCStruct a;
  a.Edges[0] = ip[0].pos.w;
  a.Edges[1] = ip[0].pos.w;
  a.Edges[2] = ip[0].pos.w;
  a.Inside = ip[0].pos.w;
  return a;
}

[shader("hull")]
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("HSPatch")]
PosStruct mainHS(InputPatch<PosStruct, 3> p, uint ix : SV_OutputControlPointID)
{
  CallFunction();
  PosStruct s;
  s.pos = p[ix].pos;
  return s;
}

[shader("domain")]
[domain("tri")]
PosStruct mainDS(const OutputPatch<PosStruct, 3> patch,
                 uint ix : SV_PrimitiveID)
{
  CallFunction();
  PosStruct v;
  v.pos = patch[0].pos;
  return v;
}

float4 a;

[shader("geometry")]
[maxvertexcount(1)]
void mainGS(triangle float4 array[3] : SV_Position, uint ix : SV_GSInstanceID,
            inout PointStream<PosStruct> OutputStream)
{
  CallFunction();
  PosStruct s;
  s.pos = a;
  OutputStream.Append(s);
  OutputStream.RestartStrip();
}

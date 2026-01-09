// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -E main %s -verify

// expected-no-diagnostics

RWByteAddressBuffer buff;

void CallMatrixLoad()
{
  __builtin_LinAlg_MatrixRef mat = __builtin_LinAlg_CreateMatrix();
  __builtin_LinAlg_MatrixLoadFromDescriptor(mat, buff, 1,1,0);
}

// --- Allowed Stages ---

[shader("compute")]
[numthreads(4,4,4)]
void mainMatrixLoadCS(uint ix : SV_GroupIndex, uint3 id : SV_GroupThreadID) {
  CallMatrixLoad();
}

struct Verts {
    float4 position : SV_Position;
};

[shader("mesh")]
[NumThreads(8, 8, 2)]
[OutputTopology("triangle")]
void mainMatrixLoadMeS(out vertices Verts verts[32],
                          uint ix : SV_GroupIndex) {
  CallMatrixLoad();
  SetMeshOutputCounts(32, 16);
  Verts v = {0.0, 0.0, 0.0, 0.0};
  verts[ix] = v;
}

struct AmpPayload {
    float2 dummy;
};

[numthreads(8, 1, 1)]
[shader("amplification")]
void mainMatrixLoadAS()
{
    CallMatrixLoad();
    AmpPayload pld;
    pld.dummy = float2(1.0,2.0);
    DispatchMesh(8, 1, 1, pld);
}

[shader("pixel")]
float4 mainMatrixLoadPS(uint ix : SV_PrimitiveID) : SV_TARGET {
  CallMatrixLoad();
  return 1.0;
}

[shader("vertex")]
float4 mainMatrixLoadVS(uint ix : SV_VertexID) : OUT {
  CallMatrixLoad();
  return 1.0;
}

[shader("node")]
[nodedispatchgrid(8,1,1)]
[numthreads(64,2,2)]
void mainMatrixLoadNS() {
  CallMatrixLoad();
}

[shader("raygeneration")]
void mainMatrixLoadRG() {
  CallMatrixLoad();
}

[shader("intersection")]
void mainMatrixLoadIS() {
  CallMatrixLoad();
}

struct Attribs { float2 barys; };

[shader("callable")]
void mainMatrixLoadCALL(inout Attribs attrs) {
  CallMatrixLoad();
}

struct [raypayload] RayPayload
{
    float elem
          : write(caller,closesthit,anyhit,miss)
          : read(caller,closesthit,anyhit,miss);
};

[shader("anyhit")]
void mainMatrixLoadAH(inout RayPayload pld, in Attribs attrs) {
 CallMatrixLoad();
}

[shader("closesthit")]
void mainMatrixLoadCH(inout RayPayload pld, in Attribs attrs) {
  CallMatrixLoad();
}

[shader("miss")]
void mainMatrixLoadMS(inout RayPayload pld) {
  CallMatrixLoad();
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
PosStruct mainMatrixLoadHS(InputPatch<PosStruct, 3> p,
                 uint ix : SV_OutputControlPointID)
{
  CallMatrixLoad();
  PosStruct s;
  s.pos = p[ix].pos;
  return s;
}

[shader("domain")]
[domain("tri")]
PosStruct mainMatrixLoadDS(const OutputPatch<PosStruct, 3> patch,
                             uint ix : SV_PrimitiveID)
{
  CallMatrixLoad();
  PosStruct v;
  v.pos = patch[0].pos;
  return v;
}

float4 a;

[shader("geometry")]
[maxvertexcount(1)]
void mainMatrixLoadGS(triangle float4 array[3] : SV_Position, uint ix : SV_GSInstanceID,
                        inout PointStream<PosStruct> OutputStream)
{
  CallMatrixLoad();
  PosStruct s;
  s.pos = a;
  OutputStream.Append(s);
  OutputStream.RestartStrip();
}

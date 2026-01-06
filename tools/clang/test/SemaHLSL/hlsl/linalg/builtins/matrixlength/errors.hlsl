// REQUIRES: dxil-1-10
// RUN: not %dxc -T lib_6_10 %s 2>&1 | FileCheck %s

// CHECK: Opcode MatrixLength not valid in shader model lib_6_10(gs).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixLength{{.*}} of function 'mainMatrixLengthGS'.
// CHECK-NEXT: Opcode MatrixLength not valid in shader model lib_6_10(ds).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixLength{{.*}} of function 'mainMatrixLengthDS'.
// CHECK-NEXT: Opcode MatrixLength not valid in shader model lib_6_10(hs).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixLength{{.*}} of function 'mainMatrixLengthHS'.
// CHECK-NEXT: Opcode MatrixLength not valid in shader model lib_6_10(vs).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixLength{{.*}} of function 'mainMatrixLengthVS'.
// CHECK-NEXT: Opcode MatrixLength not valid in shader model lib_6_10(ps).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixLength{{.*}} of function 'mainMatrixLengthPS'.
// CHECK-NEXT: Opcode MatrixLength not valid in shader model lib_6_10(miss).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixLength{{.*}} of function '{{.*}}mainMatrixLengthMS@@YAXURayPayload@@@Z'.
// CHECK-NEXT: Opcode MatrixLength not valid in shader model lib_6_10(closesthit).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixLength{{.*}} of function '{{.*}}mainMatrixLengthCH@@YAXURayPayload@@UAttribs@@@Z'.
// CHECK-NEXT: Opcode MatrixLength not valid in shader model lib_6_10(anyhit).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixLength{{.*}} of function '{{.*}}mainMatrixLengthAH@@YAXURayPayload@@UAttribs@@@Z'.
// CHECK-NEXT: Opcode MatrixLength not valid in shader model lib_6_10(callable).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixLength{{.*}} of function '{{.*}}mainMatrixLengthCALL@@YAXUAttribs@@@Z'.
// CHECK-NEXT: Opcode MatrixLength not valid in shader model lib_6_10(intersection).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixLength{{.*}} of function '{{.*}}mainMatrixLengthIS@@YAXXZ'.
// CHECK-NEXT: Opcode MatrixLength not valid in shader model lib_6_10(raygeneration).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixLength{{.*}} of function '{{.*}}mainMatrixLengthRG@@YAXXZ'.
// CHECK-NEXT: Opcode MatrixLength not valid in shader model lib_6_10(node).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixLength{{.*}} of function 'mainMatrixLengthNS'.
// CHECK-NEXT: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
// CHECK-NEXT: Function uses features incompatible with the shader stage (node) of the entry function.
// CHECK-NEXT: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
// CHECK-NEXT: Function uses features incompatible with the shader stage (raygeneration) of the entry function.
// CHECK-NEXT: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
// CHECK-NEXT: Function uses features incompatible with the shader stage (intersection) of the entry function.
// CHECK-NEXT: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
// CHECK-NEXT: Function uses features incompatible with the shader stage (callable) of the entry function.
// CHECK-NEXT: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
// CHECK-NEXT: Function uses features incompatible with the shader stage (anyhit) of the entry function.
// CHECK-NEXT: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
// CHECK-NEXT: Function uses features incompatible with the shader stage (closesthit) of the entry function.
// CHECK-NEXT: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
// CHECK-NEXT: Function uses features incompatible with the shader stage (miss) of the entry function.
// CHECK-NEXT: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
// CHECK-NEXT: Function uses features incompatible with the shader stage (ps) of the entry function.
// CHECK-NEXT: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
// CHECK-NEXT: Function uses features incompatible with the shader stage (vs) of the entry function.
// CHECK-NEXT: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
// CHECK-NEXT: Function uses features incompatible with the shader stage (hs) of the entry function.
// CHECK-NEXT: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
// CHECK-NEXT: Function uses features incompatible with the shader stage (ds) of the entry function.
// CHECK-NEXT: Entry function performs some operation that is incompatible with the shader stage or other entry properties.  See other errors for details.
// CHECK-NEXT: Function uses features incompatible with the shader stage (gs) of the entry function.
// CHECK-NEXT: Validation failed.

uint CallMatrixLength()
{
  __builtin_LinAlg_MatrixRef mat = __builtin_LinAlg_CreateMatrix();
  return __builtin_LinAlg_MatrixLength(mat);
}

// --- Allowed Stages ---

[shader("compute")]
[numthreads(4,4,4)]
void mainMatrixLengthCS(uint ix : SV_GroupIndex, uint3 id : SV_GroupThreadID) {
  CallMatrixLength();
}

struct Verts {
    float4 position : SV_Position;
};

[shader("mesh")]
[NumThreads(8, 8, 2)]
[OutputTopology("triangle")]
void mainMatrixLengthMeS(out vertices Verts verts[32],
                          uint ix : SV_GroupIndex) {
  CallMatrixLength();
  SetMeshOutputCounts(32, 16);
  Verts v = {0.0, 0.0, 0.0, 0.0};
  verts[ix] = v;
}

struct AmpPayload {
    float2 dummy;
};

[numthreads(8, 1, 1)]
[shader("amplification")]
void mainMatrixLengthAS()
{
    CallMatrixLength();
    AmpPayload pld;
    pld.dummy = float2(1.0,2.0);
    DispatchMesh(8, 1, 1, pld);
}

// --- Prohibited Stages ---

[shader("pixel")]
float4 mainMatrixLengthPS(uint ix : SV_PrimitiveID) : SV_TARGET {
  CallMatrixLength();
  return 1.0;
}

[shader("vertex")]
float4 mainMatrixLengthVS(uint ix : SV_VertexID) : OUT {
  CallMatrixLength();
  return 1.0;
}

[shader("node")]
[nodedispatchgrid(8,1,1)]
[numthreads(64,2,2)]
void mainMatrixLengthNS() {
  CallMatrixLength();
}

[shader("raygeneration")]
void mainMatrixLengthRG() {
  CallMatrixLength();
}

[shader("intersection")]
void mainMatrixLengthIS() {
  CallMatrixLength();
}

struct Attribs { float2 barys; };

[shader("callable")]
void mainMatrixLengthCALL(inout Attribs attrs) {
  CallMatrixLength();
}

struct [raypayload] RayPayload
{
    float elem
          : write(caller,closesthit,anyhit,miss)
          : read(caller,closesthit,anyhit,miss);
};

[shader("anyhit")]
void mainMatrixLengthAH(inout RayPayload pld, in Attribs attrs) {
 CallMatrixLength();
}

[shader("closesthit")]
void mainMatrixLengthCH(inout RayPayload pld, in Attribs attrs) {
  CallMatrixLength();
}

[shader("miss")]
void mainMatrixLengthMS(inout RayPayload pld) {
  CallMatrixLength();
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
PosStruct mainMatrixLengthHS(InputPatch<PosStruct, 3> p,
                 uint ix : SV_OutputControlPointID)
{
  CallMatrixLength();
  PosStruct s;
  s.pos = p[ix].pos;
  return s;
}

[shader("domain")]
[domain("tri")]
PosStruct mainMatrixLengthDS(const OutputPatch<PosStruct, 3> patch,
                             uint ix : SV_PrimitiveID)
{
  CallMatrixLength();
  PosStruct v;
  v.pos = patch[0].pos;
  return v;
}

float4 a;

[shader("geometry")]
[maxvertexcount(1)]
void mainMatrixLengthGS(triangle float4 array[3] : SV_Position, uint ix : SV_GSInstanceID,
                        inout PointStream<PosStruct> OutputStream)
{
  CallMatrixLength();
  PosStruct s;
  s.pos = a;
  OutputStream.Append(s);
  OutputStream.RestartStrip();
}

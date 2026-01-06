// REQUIRES: dxil-1-10
// RUN: not %dxc -T lib_6_10 %s 2>&1 | FileCheck %s

// CHECK: Opcode MatrixStoreToDescriptor not valid in shader model lib_6_10(gs).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixStoreToDescriptor{{.*}} of function 'mainMatrixStoreGS'.
// CHECK-NEXT: Opcode MatrixStoreToDescriptor not valid in shader model lib_6_10(ds).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixStoreToDescriptor{{.*}} of function 'mainMatrixStoreDS'.
// CHECK-NEXT: Opcode MatrixStoreToDescriptor not valid in shader model lib_6_10(hs).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixStoreToDescriptor{{.*}} of function 'mainMatrixStoreHS'.
// CHECK-NEXT: Opcode MatrixStoreToDescriptor not valid in shader model lib_6_10(vs).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixStoreToDescriptor{{.*}} of function 'mainMatrixStoreVS'.
// CHECK-NEXT: Opcode MatrixStoreToDescriptor not valid in shader model lib_6_10(ps).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixStoreToDescriptor{{.*}} of function 'mainMatrixStorePS'.
// CHECK-NEXT: Opcode MatrixStoreToDescriptor not valid in shader model lib_6_10(miss).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixStoreToDescriptor{{.*}} of function '{{.*}}mainMatrixStoreMS@@YAXURayPayload@@@Z'.
// CHECK-NEXT: Opcode MatrixStoreToDescriptor not valid in shader model lib_6_10(closesthit).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixStoreToDescriptor{{.*}} of function '{{.*}}mainMatrixStoreCH@@YAXURayPayload@@UAttribs@@@Z'.
// CHECK-NEXT: Opcode MatrixStoreToDescriptor not valid in shader model lib_6_10(anyhit).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixStoreToDescriptor{{.*}} of function '{{.*}}mainMatrixStoreAH@@YAXURayPayload@@UAttribs@@@Z'.
// CHECK-NEXT: Opcode MatrixStoreToDescriptor not valid in shader model lib_6_10(callable).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixStoreToDescriptor{{.*}} of function '{{.*}}mainMatrixStoreCALL@@YAXUAttribs@@@Z'.
// CHECK-NEXT: Opcode MatrixStoreToDescriptor not valid in shader model lib_6_10(intersection).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixStoreToDescriptor{{.*}} of function '{{.*}}mainMatrixStoreIS@@YAXXZ'.
// CHECK-NEXT: Opcode MatrixStoreToDescriptor not valid in shader model lib_6_10(raygeneration).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixStoreToDescriptor{{.*}} of function '{{.*}}mainMatrixStoreRG@@YAXXZ'.
// CHECK-NEXT: Opcode MatrixStoreToDescriptor not valid in shader model lib_6_10(node).
// CHECK-NEXT: note: at {{.*}} @dx.op.matrixStoreToDescriptor{{.*}} of function 'mainMatrixStoreNS'.
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

RWByteAddressBuffer buff;

void CallMatrixStore()
{
  __builtin_LinAlg_MatrixRef mat = __builtin_LinAlg_CreateMatrix();
  __builtin_LinAlg_MatrixStoreToDescriptor(mat, buff, 1,1,0);
}

// --- Allowed Stages ---

[shader("compute")]
[numthreads(4,4,4)]
void mainMatrixStoreCS(uint ix : SV_GroupIndex, uint3 id : SV_GroupThreadID) {
  CallMatrixStore();
}

struct Verts {
    float4 position : SV_Position;
};

[shader("mesh")]
[NumThreads(8, 8, 2)]
[OutputTopology("triangle")]
void mainMatrixStoreMeS(out vertices Verts verts[32],
                          uint ix : SV_GroupIndex) {
  CallMatrixStore();
  SetMeshOutputCounts(32, 16);
  Verts v = {0.0, 0.0, 0.0, 0.0};
  verts[ix] = v;
}

struct AmpPayload {
    float2 dummy;
};

[numthreads(8, 1, 1)]
[shader("amplification")]
void mainMatrixStoreAS()
{
    CallMatrixStore();
    AmpPayload pld;
    pld.dummy = float2(1.0,2.0);
    DispatchMesh(8, 1, 1, pld);
}

// --- Prohibited Stages ---

[shader("pixel")]
float4 mainMatrixStorePS(uint ix : SV_PrimitiveID) : SV_TARGET {
  CallMatrixStore();
  return 1.0;
}

[shader("vertex")]
float4 mainMatrixStoreVS(uint ix : SV_VertexID) : OUT {
  CallMatrixStore();
  return 1.0;
}

[shader("node")]
[nodedispatchgrid(8,1,1)]
[numthreads(64,2,2)]
void mainMatrixStoreNS() {
  CallMatrixStore();
}

[shader("raygeneration")]
void mainMatrixStoreRG() {
  CallMatrixStore();
}

[shader("intersection")]
void mainMatrixStoreIS() {
  CallMatrixStore();
}

struct Attribs { float2 barys; };

[shader("callable")]
void mainMatrixStoreCALL(inout Attribs attrs) {
  CallMatrixStore();
}

struct [raypayload] RayPayload
{
    float elem
          : write(caller,closesthit,anyhit,miss)
          : read(caller,closesthit,anyhit,miss);
};

[shader("anyhit")]
void mainMatrixStoreAH(inout RayPayload pld, in Attribs attrs) {
 CallMatrixStore();
}

[shader("closesthit")]
void mainMatrixStoreCH(inout RayPayload pld, in Attribs attrs) {
  CallMatrixStore();
}

[shader("miss")]
void mainMatrixStoreMS(inout RayPayload pld) {
  CallMatrixStore();
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
PosStruct mainMatrixStoreHS(InputPatch<PosStruct, 3> p,
                 uint ix : SV_OutputControlPointID)
{
  CallMatrixStore();
  PosStruct s;
  s.pos = p[ix].pos;
  return s;
}

[shader("domain")]
[domain("tri")]
PosStruct mainMatrixStoreDS(const OutputPatch<PosStruct, 3> patch,
                             uint ix : SV_PrimitiveID)
{
  CallMatrixStore();
  PosStruct v;
  v.pos = patch[0].pos;
  return v;
}

float4 a;

[shader("geometry")]
[maxvertexcount(1)]
void mainMatrixStoreGS(triangle float4 array[3] : SV_Position, uint ix : SV_GSInstanceID,
                        inout PointStream<PosStruct> OutputStream)
{
  CallMatrixStore();
  PosStruct s;
  s.pos = a;
  OutputStream.Append(s);
  OutputStream.RestartStrip();
}

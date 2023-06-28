// RUN: %dxc -E main -T cs_6_0 -Zi %s | FileCheck %s

// CHECK: !DIGlobalVariable(name: "UnusedCbufferFloat0", linkageName: "\01?UnusedCbufferFloat0@cb0@@3MB",
// CHECK: !DIGlobalVariable(name: "UnusedCbufferFloat1", linkageName: "\01?UnusedCbufferFloat1@cb0@@3MB",
// CHECK: !DIGlobalVariable(name: "UnusedIntConstant0",
// CHECK: !DIGlobalVariable(name: "UnusedIntConstant1",
// CHECK: !DIGlobalVariable(name: "UnusedIntConstant2",
// CHECK: !DIGlobalVariable(name: "UnusedIntConstant3",
// CHECK: !DIGlobalVariable(name: "UnusedIntConstant4",
// CHECK: !DIGlobalVariable(name: "UnusedIntConstant5",
// CHECK: !DIGlobalVariable(name: "UnusedIntConstant6",
// CHECK: !DIGlobalVariable(name: "UnusedIntConstant7",
// CHECK: !DIGlobalVariable(name: "UnusedIntConstant8",
// CHECK: !DIGlobalVariable(name: "UnusedIntConstant9",
// CHECK: !DIGlobalVariable(name: "UnusedIntConstant10",
// CHECK: !DIGlobalVariable(name: "UnusedIntConstant11",
// CHECK: !DIGlobalVariable(name: "UnusedIntConstant12",
// CHECK: !DIGlobalVariable(name: "UnusedIntConstant13",
// CHECK: !DIGlobalVariable(name: "UnusedFloatArrayConstant0",
// CHECK: !DIGlobalVariable(name: "UnusedFloatArrayConstant1",
// CHECK: !DIGlobalVariable(name: "g_xAxis",
// CHECK: !DIGlobalVariable(name: "g_yAxis",
// CHECK: !DIGlobalVariable(name: "g_zAxis",
// CHECK: !DIGlobalVariable(name: "g_gridResolution",
// CHECK: !DIGlobalVariable(name: "UsedTexture", linkageName: "\01?UsedTexture@@3V?$Texture2D@M@@A",
// CHECK: !DIGlobalVariable(name: "Unused3dTexture0", linkageName: "\01?Unused3dTexture0@@3V?$Texture3D@M@@A",
// CHECK: !DIGlobalVariable(name: "Unused3dTexture1", linkageName: "\01?Unused3dTexture1@@3V?$Texture3D@M@@A",
// CHECK: !DIGlobalVariable(name: "g_xAxis.2", linkageName: "g_xAxis.2",
// CHECK: !DIGlobalVariable(name: "g_yAxis.1", linkageName: "g_yAxis.1",
// CHECK: !DIGlobalVariable(name: "g_yAxis.0", linkageName: "g_yAxis.0",
// CHECK: !DIGlobalVariable(name: "g_xAxis.1", linkageName: "g_xAxis.1",
// CHECK: !DIGlobalVariable(name: "g_xAxis.0", linkageName: "g_xAxis.0",
// CHECK: !DIGlobalVariable(name: "g_gridResolution.1", linkageName: "g_gridResolution.1",
// CHECK: !DIGlobalVariable(name: "g_gridResolution.0", linkageName: "g_gridResolution.0",
// CHECK: !DIGlobalVariable(name: "g_yAxis.2", linkageName: "g_yAxis.2",
// CHECK: !DIGlobalVariable(name: "g_zAxis.0", linkageName: "g_zAxis.0",
// CHECK: !DIGlobalVariable(name: "g_zAxis.1", linkageName: "g_zAxis.1",
// CHECK: !DIGlobalVariable(name: "g_zAxis.2", linkageName: "g_zAxis.2",
// CHECK: !DIGlobalVariable(name: "g_gridResolution.2", linkageName: "g_gridResolution.2",

// Exclude quoted source file (see readme)
// CHECK-LABEL: {{!"[^"]*\\0A[^"]*"}}

static const int UnusedIntConstant0 = 0;
static const int UnusedIntConstant1 = 0;
static const int UnusedIntConstant2 = 0;
static const int UnusedIntConstant3 = 0;
static const int UnusedIntConstant4 = 0;
static const int UnusedIntConstant5 = 0;
static const int UnusedIntConstant6 = 0;
static const int UnusedIntConstant7 = 0;
static const int UnusedIntConstant8 = 0;
static const int UnusedIntConstant9 = 0;
static const int UnusedIntConstant10 = 0;
static const int UnusedIntConstant11 = 0;
static const int UnusedIntConstant12 = 0;
static const int UnusedIntConstant13 = 0;

static const float2 UnusedFloatArrayConstant0[2] = {float2(0.0f, 0.0f), float2(0.0f, 0.0f)};
static const float2 UnusedFloatArrayConstant1[2] = {float2(0.0f, 0.0f), float2(0.0f, 0.0f)};

static const uint3 g_xAxis = uint3(1, 0, 0);
static const uint3 g_yAxis = uint3(0, 1, 0);
static const uint3 g_zAxis = uint3(0, 0, 1);

static const uint3 g_gridResolution = uint3(1, 1, 1);

cbuffer cb0 : register(b0)
{
    float UnusedCbufferFloat0;
    float UnusedCbufferFloat1;
}

Texture2D<float> UsedTexture : register(t0);
Texture3D<float> Unused3dTexture0 : register(t1);
Texture3D<float> Unused3dTexture1 : register(t2);

bool IsInside(uint gridCoords, uint gridResolution)
{
    return gridCoords < gridResolution;
}

float sample(uint gridCoords)
{
    if (IsInside(gridCoords.x, g_gridResolution.x))
        return UsedTexture[gridCoords.xx];
    else
        return 0.0f;
}

[numthreads(4, 4, 4)]
void main(uint3 dID : SV_DispatchThreadID)
{
    if (!IsInside(dID.x, g_gridResolution.x))
        return;

    float valueX = sample(dID.x + g_xAxis.x) - sample(dID.x - g_xAxis.x);
    float valueY = sample(dID.x + g_yAxis.x) - sample(dID.x - g_yAxis.x);
    float valueZ = sample(dID.x + g_zAxis.x) - sample(dID.x - g_zAxis.x);
}

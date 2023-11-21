// RUN: %dxc -T cs_6_4 -HV 2021 -E main

struct ColorRGB { 
    uint R : 8;
    uint G : 8;
    uint B : 8;
};

struct ColorRGBA { 
    uint R : 8;
    uint G : 8;
    uint B : 8;
    uint A : 8;
};

struct TwoColors {
    ColorRGBA rgba1;
    ColorRGBA rgba2;
};

struct Mixed {
    float f;
    uint i;
};

struct Vectors {
    uint2 p1;
    uint2 p2;
};

RWStructuredBuffer<uint> buf : r0;
RWStructuredBuffer<uint64_t> lbuf : r1;

// CHECK: OpName [[BUF:%[^ ]*]] "buf"
// CHECK: OpName [[LBUF:%[^ ]*]] "lbuf"
// CHECK: OpName [[COLORRGB:%[^ ]*]] "ColorRGB"
// CHECK: OpName [[COLORRGBA:%[^ ]*]] "ColorRGBA"
// CHECK: OpName [[TWOCOLORS:%[^ ]*]] "TwoColors"
// CHECK: OpName [[VECTORS:%[^ ]*]] "Vectors"
// CHECK: OpName [[MIXED:%[^ ]*]] "Mixed"

[numthreads(1,1,1)]
void main()
{
    ColorRGB rgb;
    ColorRGBA c0;
    ColorRGBA c1;
    TwoColors colors;
    Vectors v;
    Mixed m = {-1.0, 1};
    rgb.R = 127;
    rgb.G = 127;
    rgb.B = 127;
    c0.R = 255;
    c0.G = 127;
    c0.B = 63;
    c0.A = 31;
    c1.R = 15;
    c1.G = 7;
    c1.B = 3;
    c1.A = 1;
    colors.rgba1 = c0;
    colors.rgba2 = c1;
    v.p1.x = 3;
    v.p1.y = 2;
    v.p2.x = 1;
    v.p2.y = 0;

// CHECK-DAG: [[FLOAT:%[^ ]*]] = OpTypeFloat 32
// CHECK-DAG: [[FN1:%[^ ]*]] = OpConstant [[FLOAT]] -1
// CHECK-DAG: [[UINT:%[^ ]*]] = OpTypeInt 32 0
// CHECK-DAG: [[U127:%[^ ]*]] = OpConstant [[UINT]] 127
// CHECK-DAG: [[INT:%[^ ]*]] = OpTypeInt 32 1
// CHECK-DAG: [[I0:%[^ ]*]] = OpConstant [[INT]] 0
// CHECK-DAG: [[U0:%[^ ]*]] = OpConstant [[UINT]] 0
// CHECK-DAG: [[U8:%[^ ]*]] = OpConstant [[UINT]] 8
// CHECK-DAG: [[U255:%[^ ]*]] = OpConstant [[UINT]] 255
// CHECK-DAG: [[U3:%[^ ]*]] = OpConstant [[UINT]] 3
// CHECK-DAG: [[ULONG:%[^ ]*]] = OpTypeInt 64 0
// CHECK-DAG: [[DOUBLE:%[^ ]*]] = OpTypeFloat 64

    buf[0] = (uint) colors;
// CHECK: [[COLORS:%[^ ]*]] = OpLoad [[TWOCOLORS]]
// CHECK: [[COLORS0:%[^ ]*]] = OpCompositeExtract [[COLORRGBA]] [[COLORS]] 0
// CHECK: [[COLORS00:%[^ ]*]] = OpCompositeExtract [[UINT]] [[COLORS0]] 0
// CHECK: [[COLORS000:%[^ ]*]] = OpBitFieldUExtract [[UINT]] [[COLORS00]] [[U0]] [[U8]]
// CHECK: [[BUF00:%[^ ]*]] = OpAccessChain %{{[^ ]*}} [[BUF]] [[I0]] [[U0]]
// CHECK: OpStore [[BUF00]] [[COLORS000]]

    buf[0] -= (uint) rgb;
// CHECK: [[RGB:%[^ ]*]] = OpLoad [[COLORRGB]]
// CHECK: [[RGB0:%[^ ]*]] = OpCompositeExtract [[UINT]] [[RGB]] 0
// CHECK: [[RGB00:%[^ ]*]] = OpBitFieldUExtract [[UINT]] [[RGB0]] [[U0]] [[U8]]
// CHECK: [[BUF00:%[^ ]*]] = OpAccessChain %{{[^ ]*}} [[BUF]] [[I0]] [[U0]]
// CHECK: [[V1:%[^ ]*]] = OpLoad [[UINT]] [[BUF00]]
// CHECK: [[V2:%[^ ]*]] = OpISub [[UINT]] [[V1]] [[RGB00]]
// CHECK: OpStore [[BUF00]] [[V2]]

    lbuf[0] = (uint64_t) v;
// CHECK: [[VECS:%[^ ]*]] = OpLoad [[VECTORS]]
// CHECK: [[VECS00:%[^ ]*]] = OpCompositeExtract [[UINT]] [[VECS]] 0 0
// CHECK: [[V1:%[^ ]*]] = OpUConvert [[ULONG]] [[VECS00]]
// CHECK: [[LBUF00:%[^ ]*]] = OpAccessChain %{{[^ ]*}} [[LBUF]] [[I0]] [[U0]]
// CHECK: OpStore [[LBUF00]] [[V1]]

    lbuf[0] += (uint64_t) m;
// CHECK: [[MIX:%[^ ]*]] = OpLoad [[MIXED]]
// CHECK: [[MIX0:%[^ ]*]] = OpCompositeExtract [[FLOAT]] [[MIX]] 0
// CHECK: [[V1:%[^ ]*]] = OpFConvert [[DOUBLE]] [[MIX0]]
// CHECK: [[V2:%[^ ]*]] = OpConvertFToU [[ULONG]] [[V1]]
// CHECK: [[LBUF00:%[^ ]*]] = OpAccessChain %{{[^ ]*}} [[LBUF]] [[I0]] [[U0]]
// CHECK: [[V3:%[^ ]*]] = OpLoad [[ULONG]] [[LBUF00]]
// CHECK: [[V4:%[^ ]*]] = OpIAdd [[ULONG]] [[V3]] [[V2]]
// CHECK: OpStore [[LBUF00]] [[V4]]
}


// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

struct INPUT {
    float a;
    Texture2D t2d;
    float b;
};

// CHECK: @main
float4 main(
            float4 a : A
            // SGV
           ,uint vertid : SV_VertexID
           ,uint instid : SV_InstanceID
           ,uniform Texture2D t2d : t2d
           ,INPUT inWithRes : INPUT
           ) : SV_Position
{
    float4 r = 0;
    r += a;
    r += vertid;
    r += instid;
    r += t2d[int2(0,0)];
    r += inWithRes.t2d[int2(0,0)];
    return r;
}
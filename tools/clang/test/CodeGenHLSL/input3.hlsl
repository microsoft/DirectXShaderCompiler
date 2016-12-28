// RUN: %dxc -E main -T vs_5_0 %s

float4 main(
            float4 a : A
            // SGV
           ,uint vertid : SV_VertexID
           ,uint instid : SV_InstanceID
           ) : SV_Position
{
    float4 r = 0;
    r += a;
    r += vertid;
    r += instid;
    return r;
}

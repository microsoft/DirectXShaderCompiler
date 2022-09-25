// RUN: %dxc -T cs_6_0 -E main

RWTexture2D<float> g_output;

void foo(inout float3 x)
{
    x += float3(1, 2, 3);
}

// CHECK: error: casting from 'float' to 'float3' unsupported

[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadId)
{
    float bar = 1;
    foo(bar);
    g_output[id.xy] = bar;
}

// RUN: %dxc -T vs_6_0 -E main -fspv-target-env=vulkan1.3 %s -spirv | FileCheck %s

// CHECK-NOT: OpCapability DerivativeControl
// CHECK-NOT: OpExtension "SPV_KHR_compute_shader_derivatives"

struct VSOut
{
    float4 pos : SV_Position;
};

VSOut main(float4 pos : POSITION)
{
    VSOut output;
    output.pos = pos;
    if (false)
    {
        output.pos += ddx(pos);
    }
    return output;
}

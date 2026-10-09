// RUN: %dxc -T ps_6_7 -E PS -fspv-reflect %s -spirv | FileCheck %s --implicit-check-not="OpCapability ImageMSArray"

// The shader from
// https://github.com/microsoft/DirectXShaderCompiler/issues/5244.
//
// This RUN line deliberately omits -fcgl, unlike the other RWTexture2DMS
// tests. The full pipeline runs the capability-trimming pass and then
// spirv-val, so this is what proves StorageImageMultisample survives trimming
// and that the module a user actually gets is valid. Instruction order is
// therefore optimizer-dependent, and the assertions below avoid depending on
// it.

// CHECK-DAG: OpCapability StorageImageMultisample
// CHECK-DAG: OpExtension "SPV_GOOGLE_hlsl_functionality1"
// CHECK-DAG: OpExtension "SPV_GOOGLE_user_type"

// The second template argument is the sample count, which SPIR-V has no
// operand for. It survives into the reflection string and nowhere else.
// CHECK-DAG: OpDecorateString %gUav UserTypeGOOGLE "rwtexture2dms:<uint4,2>"

// CHECK-DAG: OpTypeImage %uint 2D 2 0 1 2 Rgba32ui

RWTexture2DMS<uint4, 2> gUav : register(u1);

struct VSOUTPUT
{
    float4 Pos : SV_POSITION;
};

// The plain subscript reads sample 0 and .sample[1][coord] reads and writes
// sample 1, so the two forms must not collapse into the same access.
// CHECK-DAG: OpImageRead %v4uint {{%[0-9]+}} {{%[0-9]+}} Sample %uint_0
// CHECK-DAG: OpImageRead %v4uint {{%[0-9]+}} {{%[0-9]+}} Sample %uint_1
// CHECK-DAG: OpImageWrite {{%[0-9]+}} {{%[0-9]+}} {{%[0-9]+}} Sample %uint_1
float4 PS(VSOUTPUT pin) : SV_Target
{
    uint4 col = gUav[(uint2) pin.Pos.xy];

    gUav.sample[1][uint2(pin.Pos.xy)] = col;
    uint4 outColor = gUav.sample[1][uint2(pin.Pos.xy)];
    return outColor;
}

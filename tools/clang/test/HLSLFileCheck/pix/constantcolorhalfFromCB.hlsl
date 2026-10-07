// RUN: %dxc -enable-16bit-types -Emain -Tps_6_2 %s | %opt -S -hlsl-dxil-constantColor,mod-mode=1 | %FileCheck %s

// From-constant-buffer mode against a native half SV_Target0. The tools
// constant buffer is four 32-bit components; loaded values narrow to half.

// CB return type is f32:
// CHECK: %dx.types.CBufRet.f32 = type { float, float, float, float }

// Create handle:
// CHECK: %PIX_Constant_Color_CB_Handle = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)

// Load the row:
// CHECK: %PIX_Constant_Color_Value = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %PIX_Constant_Color_CB_Handle, i32 0)

// Extract components:
// CHECK: %PIX_Constant_Color_Value0 = extractvalue %dx.types.CBufRet.f32 %PIX_Constant_Color_Value, 0
// CHECK: %PIX_Constant_Color_Value1 = extractvalue %dx.types.CBufRet.f32 %PIX_Constant_Color_Value, 1
// CHECK: %PIX_Constant_Color_Value2 = extractvalue %dx.types.CBufRet.f32 %PIX_Constant_Color_Value, 2
// CHECK: %PIX_Constant_Color_Value3 = extractvalue %dx.types.CBufRet.f32 %PIX_Constant_Color_Value, 3

// Narrow to half:
// CHECK: %PIX_Constant_Color_ValueNarrowed0 = fptrunc float %PIX_Constant_Color_Value0 to half
// CHECK: %PIX_Constant_Color_ValueNarrowed1 = fptrunc float %PIX_Constant_Color_Value1 to half
// CHECK: %PIX_Constant_Color_ValueNarrowed2 = fptrunc float %PIX_Constant_Color_Value2 to half
// CHECK: %PIX_Constant_Color_ValueNarrowed3 = fptrunc float %PIX_Constant_Color_Value3 to half

// Store SV_Target0:
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 0, i32 0, i8 0, half %PIX_Constant_Color_ValueNarrowed0)
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 0, i32 0, i8 1, half %PIX_Constant_Color_ValueNarrowed1)
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 0, i32 0, i8 2, half %PIX_Constant_Color_ValueNarrowed2)
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 0, i32 0, i8 3, half %PIX_Constant_Color_ValueNarrowed3)

[RootSignature("")]
half4 main() : SV_Target {
    return half4(0, 0, 0, 0);
}

// This file contains tests covering all overloads of mul intrinsic
// as documented here: https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-mul

// *****************************
// float overloads
// *****************************

// RUN: %dxc -T vs_6_0 -E main -DELEM_TY1=float4 -DELEM_TY2=float4 -DRET_TY=float4 %s  | FileCheck %s -check-prefix=FL4_OVRLD
// FL4_OVRLD: call float @dx.op.dot4.f32

// RUN: %dxc -T vs_6_0 -E main -DELEM_TY1=float3 -DELEM_TY2=float3 -DRET_TY=float3 %s  | FileCheck %s -check-prefix=FL3_OVRLD
// FL3_OVRLD: call float @dx.op.dot3.f32

// RUN: %dxc -T vs_6_0 -E main -DELEM_TY1=float2 -DELEM_TY2=float2 -DRET_TY=float2 %s  | FileCheck %s -check-prefix=FL2_OVRLD
// FL2_OVRLD: call float @dx.op.dot2.f32

// RUN: %dxc -T vs_6_0 -E main -DELEM_TY1=float -DELEM_TY2=float -DRET_TY=float %s  | FileCheck %s -check-prefix=FL_OVRLD
// FL_OVRLD: fmul fast float

// RUN: %dxc -T vs_6_0 -E main -Od -DELEM_TY1=float4 -DELEM_TY2=float4 -DRET_TY=float4 %s  | FileCheck %s -check-prefix=FL4_OVRLD_OD
// FL4_OVRLD_OD: call float @dx.op.dot4.f32

// RUN: %dxc -T vs_6_0 -E main -Od -DELEM_TY1=float3 -DELEM_TY2=float3 -DRET_TY=float3 %s  | FileCheck %s -check-prefix=FL3_OVRLD_OD
// FL3_OVRLD_OD: call float @dx.op.dot3.f32

// RUN: %dxc -T vs_6_0 -E main -Od -DELEM_TY1=float2 -DELEM_TY2=float2 -DRET_TY=float2 %s  | FileCheck %s -check-prefix=FL2_OVRLD_OD
// FL2_OVRLD_OD: call float @dx.op.dot2.f32

// RUN: %dxc -T vs_6_0 -E main -DELEM_TY1=float2x4 -DELEM_TY2=float4x3 -DRET_TY=float2x3 %s  | FileCheck %s -check-prefix=FLMAT1_OVRLD
// FLMAT1_OVRLD: call float @dx.op.tertiary.f32
// FLMAT1_OVRLD: call float @dx.op.tertiary.f32
// FLMAT1_OVRLD: call float @dx.op.tertiary.f32
// FLMAT1_OVRLD: fmul fast float

// FLMAT1_OVRLD: call float @dx.op.tertiary.f32
// FLMAT1_OVRLD: call float @dx.op.tertiary.f32
// FLMAT1_OVRLD: call float @dx.op.tertiary.f32
// FLMAT1_OVRLD: fmul fast float

// FLMAT1_OVRLD: call float @dx.op.tertiary.f32
// FLMAT1_OVRLD: call float @dx.op.tertiary.f32
// FLMAT1_OVRLD: call float @dx.op.tertiary.f32
// FLMAT1_OVRLD: fmul fast float

// FLMAT1_OVRLD: call float @dx.op.tertiary.f32
// FLMAT1_OVRLD: call float @dx.op.tertiary.f32
// FLMAT1_OVRLD: call float @dx.op.tertiary.f32
// FLMAT1_OVRLD: fmul fast float

// RUN: %dxc -T vs_6_0 -E main -DELEM_TY1=float1x4 -DELEM_TY2=float4x1 -DRET_TY=float1x1 %s  | FileCheck %s -check-prefix=FLMAT2_OVRLD
// FLMAT2_OVRLD: fmul fast float
// FLMAT2_OVRLD: call float @dx.op.tertiary.f32
// FLMAT2_OVRLD: call float @dx.op.tertiary.f32
// FLMAT2_OVRLD: call float @dx.op.tertiary.f32

cbuffer CB {
  ELEM_TY1 e1;
  ELEM_TY2 e2;
};

RET_TY main(): OUT
{
	return mul(e1, e2);
}
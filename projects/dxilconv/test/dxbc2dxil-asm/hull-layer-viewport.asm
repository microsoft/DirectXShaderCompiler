/*// RUN: %testasm %s /Fo %t.dxbc*/
// RUN: %dxbc2dxil %t.dxbc /emit-llvm | %FileCheck %s

// Reproduces the hull shader conversion crash reported in
// https://github.com/HansKristian-Work/vkd3d-proton/pull/3462.
// The accompanying DXBC is extracted from hs_layer_viewport_code_dxbc at
// commit 50c07246e6cf2d7d00e68a2a0adcb986e8d57f21; this is its disassembly.
// FXC cannot assemble Shader Model 5 text. Recompiling the original HLSL with
// FXC removes the identity control-point phase or indexes through a temporary,
// so it does not preserve the direct vOutputControlPointID indexing below.
// Direct control-point ID indexing must lower to dx.op.outputControlPointID
// rather than leaving null vertex-index operands in the generated load calls.

// Input and output signatures:
// SV_Position               0  xyzw  register 0  float
// SV_RenderTargetArrayIndex 0  x     register 1  uint
// SV_ViewportArrayIndex     0  y     register 1  uint
// Patch constant signature:
// SV_TessFactor             0  x     register 0  float (TRIEDGE)
// SV_TessFactor             1  x     register 1  float (TRIEDGE)
// SV_TessFactor             2  x     register 2  float (TRIEDGE)
// SV_InsideTessFactor       0  x     register 3  float (TRIINT)

// CHECK-LABEL: define void @main()
// CHECK: %[[ID:[0-9]+]] = call i32 @dx.op.outputControlPointID.i32(i32 107)
// CHECK-DAG: call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 %[[ID]])
// CHECK-DAG: call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 %[[ID]])
// CHECK-DAG: call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 %[[ID]])
// CHECK-DAG: call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 %[[ID]])
// CHECK: %[[LAYERID:[0-9]+]] = call i32 @dx.op.outputControlPointID.i32(i32 107)
// CHECK: call i32 @dx.op.loadInput.i32(i32 4, i32 1, i32 0, i8 0, i32 %[[LAYERID]])
// CHECK: %[[VIEWPORTID:[0-9]+]] = call i32 @dx.op.outputControlPointID.i32(i32 107)
// CHECK: call i32 @dx.op.loadInput.i32(i32 4, i32 2, i32 0, i8 0, i32 %[[VIEWPORTID]])

hs_5_0
hs_decls
dcl_input_control_point_count 3
dcl_output_control_point_count 3
dcl_tessellator_domain domain_tri
dcl_tessellator_partitioning partitioning_integer
dcl_tessellator_output_primitive output_triangle_ccw
hs_control_point_phase
dcl_output o0.xyzw
dcl_output o1.x
dcl_output o1.y
dcl_input v[3][0].xyzw
dcl_input v[3][1].x
dcl_input v[3][1].y
dcl_input vOutputControlPointID
mov o0.xyzw, v[vOutputControlPointID0.x + 0][0].xyzw
mov o1.x, v[vOutputControlPointID0.x + 0][1].xxxx
mov o1.y, v[vOutputControlPointID0.x + 0][1].yyyy
ret
hs_fork_phase
dcl_output_siv o0.x, finalTriUeq0EdgeTessFactor
dcl_output_siv o1.x, finalTriVeq0EdgeTessFactor
dcl_output_siv o2.x, finalTriWeq0EdgeTessFactor
dcl_output_siv o3.x, finalTriInsideTessFactor
mov o0.x, l(1.000000)
mov o1.x, l(1.000000)
mov o2.x, l(1.000000)
mov o3.x, l(1.000000)
ret

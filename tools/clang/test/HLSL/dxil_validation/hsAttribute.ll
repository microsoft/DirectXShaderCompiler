; RUN: %dxv %s | FileCheck %s

; CHECK: HS input control point count must be [1..32].  36 specified
; CHECK: Invalid Tessellator Domain specified. Must be isoline, tri or quad
; CHECK: Invalid Tessellator Partitioning specified. Must be integer, pow2, fractional_odd or fractional_even.
; CHECK: Invalid Tessellator Output Primitive specified. Must be point, line, triangleCW or triangleCCW.
; CHECK: Hull Shader MaxTessFactor must be [1.000000..64.000000].  65.000000 specified
; CHECK: Invalid Tessellator Domain specified. Must be isoline, tri or quad
; CHECK: output control point count must be [0..32].  36 specified




target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%class.Texture2D = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%struct.PSSceneIn = type { <4 x float>, <2 x float>, <3 x float> }
%struct.VSSceneIn = type { <3 x float>, <3 x float>, <2 x float> }
%struct.HSPerPatchData = type { [3 x float], float }
%struct.HSPerVertexData = type { %struct.PSSceneIn }

@dx.typevar.0 = external addrspace(1) constant %class.Texture2D
@dx.typevar.1 = external addrspace(1) constant %"class.Texture2D<vector<float, 4> >::mips_type"
@dx.typevar.2 = external addrspace(1) constant %struct.PSSceneIn
@dx.typevar.3 = external addrspace(1) constant %struct.VSSceneIn
@dx.typevar.4 = external addrspace(1) constant %struct.HSPerPatchData
@dx.typevar.5 = external addrspace(1) constant %struct.HSPerVertexData
@llvm.used = appending global [6 x i8*] [i8* addrspacecast (i8 addrspace(1)* bitcast (%class.Texture2D addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%"class.Texture2D<vector<float, 4> >::mips_type" addrspace(1)* @dx.typevar.1 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.PSSceneIn addrspace(1)* @dx.typevar.2 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.VSSceneIn addrspace(1)* @dx.typevar.3 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.HSPerPatchData addrspace(1)* @dx.typevar.4 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.HSPerVertexData addrspace(1)* @dx.typevar.5 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @"\01?HSPerPatchFunc@@YA?AUHSPerPatchData@@V?$InputPatch@UPSSceneIn@@$02@@@Z.flat"([3 x <4 x float>]* nocapture readnone, [3 x <2 x float>]* nocapture readnone, [3 x <3 x float>]* nocapture readnone, [3 x float]* nocapture readnone, float* nocapture readnone) #0 {
entry:
  %retval.0 = alloca [3 x float], align 4
  %arrayidx3 = getelementptr inbounds [3 x float], [3 x float]* %retval.0, i32 0, i32 0
  store float 1.000000e+00, float* %arrayidx3, align 4, !tbaa !56
  %arrayidx22 = getelementptr inbounds [3 x float], [3 x float]* %retval.0, i32 0, i32 1
  store float 1.000000e+00, float* %arrayidx22, align 4, !tbaa !56
  %arrayidx41 = getelementptr inbounds [3 x float], [3 x float]* %retval.0, i32 0, i32 2
  store float 1.000000e+00, float* %arrayidx41, align 4, !tbaa !56
  %load = load [3 x float], [3 x float]* %retval.0, align 4
  %5 = extractvalue [3 x float] %load, 0
  call void @dx.op.storePatchConstant.f32(i32 109, i32 0, i32 0, i8 0, float %5)
  %6 = extractvalue [3 x float] %load, 1
  call void @dx.op.storePatchConstant.f32(i32 109, i32 0, i32 1, i8 0, float %6)
  %7 = extractvalue [3 x float] %load, 2
  call void @dx.op.storePatchConstant.f32(i32 109, i32 0, i32 2, i8 0, float %7)
  call void @dx.op.storePatchConstant.f32(i32 109, i32 1, i32 0, i8 0, float 1.000000e+00)
  ret void
}

; Function Attrs: nounwind
define void @main.flat(i32, [3 x <4 x float>]* nocapture readnone, [3 x <2 x float>]* nocapture readnone, [3 x <3 x float>]* nocapture readnone, <4 x float>* nocapture readnone, <2 x float>* nocapture readnone, <3 x float>* nocapture readnone) #0 {
entry:
  %7 = call i32 @dx.op.outputControlPointID.i32(i32 110)
  %8 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 %7)
  %9 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 %7)
  %10 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 %7)
  %11 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 %7)
  %12 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 %7)
  %13 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 %7)
  %14 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 0, i32 %7)
  %15 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 1, i32 %7)
  %16 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 2, i32 %7)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %8)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %9)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %10)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %11)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %12)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %13)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float %14)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 1, float %15)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 2, float %16)
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.outputControlPointID.i32(i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind
declare void @dx.op.storePatchConstant.f32(i32, i32, i32, i8, float) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.valver = !{!1}
!dx.version = !{!2}
!dx.shaderModel = !{!3}
!dx.typeAnnotations = !{!4, !23}
!dx.entryPoints = !{!45}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 1, i32 0}
!2 = !{i32 0, i32 7}
!3 = !{!"hs", i32 6, i32 0}
!4 = !{i32 0, %class.Texture2D addrspace(1)* @dx.typevar.0, !5, %"class.Texture2D<vector<float, 4> >::mips_type" addrspace(1)* @dx.typevar.1, !8, %struct.PSSceneIn addrspace(1)* @dx.typevar.2, !10, %struct.VSSceneIn addrspace(1)* @dx.typevar.3, !14, %struct.HSPerPatchData addrspace(1)* @dx.typevar.4, !18, %struct.HSPerVertexData addrspace(1)* @dx.typevar.5, !21}
!5 = !{i32 20, !6, !7}
!6 = !{i32 3, i32 0, i32 6, !"h", i32 7, i32 9}
!7 = !{i32 3, i32 16, i32 6, !"mips"}
!8 = !{i32 4, !9}
!9 = !{i32 3, i32 0, i32 6, !"handle", i32 7, i32 5}
!10 = !{i32 44, !11, !12, !13}
!11 = !{i32 3, i32 0, i32 4, !"SV_Position", i32 6, !"pos", i32 7, i32 9}
!12 = !{i32 3, i32 16, i32 4, !"TEXCOORD0", i32 6, !"tex", i32 7, i32 9}
!13 = !{i32 3, i32 32, i32 4, !"NORMAL", i32 6, !"norm", i32 7, i32 9}
!14 = !{i32 40, !15, !16, !17}
!15 = !{i32 3, i32 0, i32 4, !"POSITION", i32 6, !"pos", i32 7, i32 9}
!16 = !{i32 3, i32 16, i32 4, !"NORMAL", i32 6, !"norm", i32 7, i32 9}
!17 = !{i32 3, i32 32, i32 4, !"TEXCOORD0", i32 6, !"tex", i32 7, i32 9}
!18 = !{i32 40, !19, !20}
!19 = !{i32 3, i32 0, i32 4, !"SV_TessFactor", i32 6, !"edges", i32 7, i32 9}
!20 = !{i32 3, i32 36, i32 4, !"SV_InsideTessFactor", i32 6, !"inside", i32 7, i32 9}
!21 = !{i32 44, !22}
!22 = !{i32 3, i32 0, i32 6, !"v"}
!23 = !{i32 1, void ([3 x <4 x float>]*, [3 x <2 x float>]*, [3 x <3 x float>]*, [3 x float]*, float*)* @"\01?HSPerPatchFunc@@YA?AUHSPerPatchData@@V?$InputPatch@UPSSceneIn@@$02@@@Z.flat", !24, void (i32, [3 x <4 x float>]*, [3 x <2 x float>]*, [3 x <3 x float>]*, <4 x float>*, <2 x float>*, <3 x float>*)* @main.flat, !39}
!24 = !{!25, !27, !30, !32, !34, !37}
!25 = !{i32 0, !26, !26}
!26 = !{}
!27 = !{i32 3, !28, !29}
!28 = !{i32 4, !"SV_Position", i32 7, i32 9}
!29 = !{i32 0}
!30 = !{i32 3, !31, !29}
!31 = !{i32 4, !"TEXCOORD0", i32 7, i32 9}
!32 = !{i32 3, !33, !29}
!33 = !{i32 4, !"NORMAL", i32 7, i32 9}
!34 = !{i32 1, !35, !36}
!35 = !{i32 4, !"SV_TessFactor", i32 7, i32 9}
!36 = !{i32 0, i32 1, i32 2}
!37 = !{i32 1, !38, !29}
!38 = !{i32 4, !"SV_InsideTessFactor", i32 7, i32 9}
!39 = !{!25, !40, !27, !30, !32, !42, !43, !44}
!40 = !{i32 0, !41, !29}
!41 = !{i32 4, !"SV_OutputControlPointID", i32 7, i32 5}
!42 = !{i32 1, !28, !29}
!43 = !{i32 1, !31, !29}
!44 = !{i32 1, !33, !29}
!45 = !{void (i32, [3 x <4 x float>]*, [3 x <2 x float>]*, [3 x <3 x float>]*, <4 x float>*, <2 x float>*, <3 x float>*)* @main.flat, !"", !46, null, !54}
!46 = !{!47, !47, !51}
!47 = !{!48, !49, !50}
!48 = !{i32 0, !"SV_Position", i8 9, i8 3, !29, i8 4, i32 1, i8 4, i32 0, i8 0, null}
!49 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !29, i8 2, i32 1, i8 2, i32 1, i8 0, null}
!50 = !{i32 2, !"NORMAL", i8 9, i8 0, !29, i8 2, i32 1, i8 3, i32 2, i8 0, null}
!51 = !{!52, !53}
!52 = !{i32 0, !"SV_TessFactor", i8 9, i8 25, !36, i8 0, i32 3, i8 1, i32 0, i8 0, null}
!53 = !{i32 1, !"SV_InsideTessFactor", i8 9, i8 26, !29, i8 0, i32 1, i8 1, i32 3, i8 0, null}
!54 = !{i32 3, !55}
!55 = !{void ([3 x <4 x float>]*, [3 x <2 x float>]*, [3 x <3 x float>]*, [3 x float]*, float*)* @"\01?HSPerPatchFunc@@YA?AUHSPerPatchData@@V?$InputPatch@UPSSceneIn@@$02@@@Z.flat", i32 36, i32 36, i32 0, i32 0, i32 0, float 6.500000e+01}
!56 = !{!57, !57, i64 0}
!57 = !{!"float", !58, i64 0}
!58 = !{!"omnipotent char", !59, i64 0}
!59 = !{!"Simple C/C++ TBAA"}

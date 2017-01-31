; RUN: %dxv %s | FileCheck %s

; CHECK: Hull Shader declared with Tri Domain must specify output primitive point, triangle_cw or triangle_ccw. Line output is not compatible with the Tri domain


target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%class.Texture2D = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%struct.PSSceneIn = type { <4 x float>, <2 x float>, <3 x float> }
%struct.VSSceneIn = type { <3 x float>, <3 x float>, <2 x float> }
%struct.HSPerPatchData = type { [3 x float], float }
%class.InputPatch = type { [3 x %struct.PSSceneIn] }
%class.OutputPatch = type { [3 x %struct.PSSceneIn] }
%struct.HSPerVertexData = type { %struct.PSSceneIn }

@dx.typevar.0 = external addrspace(1) constant %class.Texture2D
@dx.typevar.1 = external addrspace(1) constant %"class.Texture2D<vector<float, 4> >::mips_type"
@dx.typevar.2 = external addrspace(1) constant %struct.PSSceneIn
@dx.typevar.3 = external addrspace(1) constant %struct.VSSceneIn
@dx.typevar.4 = external addrspace(1) constant %struct.HSPerPatchData
@dx.typevar.5 = external addrspace(1) constant %class.InputPatch
@dx.typevar.6 = external addrspace(1) constant %class.OutputPatch
@dx.typevar.7 = external addrspace(1) constant %struct.HSPerVertexData
@llvm.used = appending global [8 x i8*] [i8* addrspacecast (i8 addrspace(1)* bitcast (%class.Texture2D addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%"class.Texture2D<vector<float, 4> >::mips_type" addrspace(1)* @dx.typevar.1 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.PSSceneIn addrspace(1)* @dx.typevar.2 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.VSSceneIn addrspace(1)* @dx.typevar.3 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.HSPerPatchData addrspace(1)* @dx.typevar.4 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.InputPatch addrspace(1)* @dx.typevar.5 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.OutputPatch addrspace(1)* @dx.typevar.6 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.HSPerVertexData addrspace(1)* @dx.typevar.7 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @"\01?HSPerPatchFunc@@YA?AUHSPerPatchData@@V?$InputPatch@UPSSceneIn@@$02@@V?$OutputPatch@UPSSceneIn@@$02@@@Z.flat"([3 x <4 x float>]* nocapture readnone, [3 x <2 x float>]* nocapture readnone, [3 x <3 x float>]* nocapture readnone, [3 x <4 x float>]* nocapture readnone, [3 x <2 x float>]* nocapture readnone, [3 x <3 x float>]* nocapture readnone, [3 x float]* nocapture readnone, float* nocapture readnone) #0 {
entry:
  %retval.0 = alloca [3 x float], align 4
  %8 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 0, i32 0)
  %arrayidx3 = getelementptr inbounds [3 x float], [3 x float]* %retval.0, i32 0, i32 0
  store float %8, float* %arrayidx3, align 4, !tbaa !65
  %9 = call float @dx.op.loadOutputControlPoint.f32(i32 106, i32 1, i32 0, i8 1, i32 1)
  %arrayidx32 = getelementptr inbounds [3 x float], [3 x float]* %retval.0, i32 0, i32 1
  store float %9, float* %arrayidx32, align 4, !tbaa !65
  %arrayidx51 = getelementptr inbounds [3 x float], [3 x float]* %retval.0, i32 0, i32 2
  store float 1.000000e+00, float* %arrayidx51, align 4, !tbaa !65
  %load = load [3 x float], [3 x float]* %retval.0, align 4
  %10 = extractvalue [3 x float] %load, 0
  call void @dx.op.storePatchConstant.f32(i32 109, i32 0, i32 0, i8 0, float %10)
  %11 = extractvalue [3 x float] %load, 1
  call void @dx.op.storePatchConstant.f32(i32 109, i32 0, i32 1, i8 0, float %11)
  %12 = extractvalue [3 x float] %load, 2
  call void @dx.op.storePatchConstant.f32(i32 109, i32 0, i32 2, i8 0, float %12)
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

; Function Attrs: nounwind readnone
declare float @dx.op.loadOutputControlPoint.f32(i32, i32, i32, i8, i32) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.typeAnnotations = !{!3, !24}
!dx.entryPoints = !{!49}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 7}
!2 = !{!"hs", i32 6, i32 0}
!3 = !{i32 0, %class.Texture2D addrspace(1)* @dx.typevar.0, !4, %"class.Texture2D<vector<float, 4> >::mips_type" addrspace(1)* @dx.typevar.1, !7, %struct.PSSceneIn addrspace(1)* @dx.typevar.2, !9, %struct.VSSceneIn addrspace(1)* @dx.typevar.3, !13, %struct.HSPerPatchData addrspace(1)* @dx.typevar.4, !17, %class.InputPatch addrspace(1)* @dx.typevar.5, !20, %class.OutputPatch addrspace(1)* @dx.typevar.6, !20, %struct.HSPerVertexData addrspace(1)* @dx.typevar.7, !22}
!4 = !{i32 20, !5, !6}
!5 = !{i32 3, i32 0, i32 6, !"h", i32 7, i32 9}
!6 = !{i32 3, i32 16, i32 6, !"mips"}
!7 = !{i32 4, !8}
!8 = !{i32 3, i32 0, i32 6, !"handle", i32 7, i32 5}
!9 = !{i32 44, !10, !11, !12}
!10 = !{i32 3, i32 0, i32 4, !"SV_Position", i32 6, !"pos", i32 7, i32 9}
!11 = !{i32 3, i32 16, i32 4, !"TEXCOORD0", i32 6, !"tex", i32 7, i32 9}
!12 = !{i32 3, i32 32, i32 4, !"NORMAL", i32 6, !"norm", i32 7, i32 9}
!13 = !{i32 40, !14, !15, !16}
!14 = !{i32 3, i32 0, i32 4, !"POSITION", i32 6, !"pos", i32 7, i32 9}
!15 = !{i32 3, i32 16, i32 4, !"NORMAL", i32 6, !"norm", i32 7, i32 9}
!16 = !{i32 3, i32 32, i32 4, !"TEXCOORD0", i32 6, !"tex", i32 7, i32 9}
!17 = !{i32 40, !18, !19}
!18 = !{i32 3, i32 0, i32 4, !"SV_TessFactor", i32 6, !"edges", i32 7, i32 9}
!19 = !{i32 3, i32 36, i32 4, !"SV_InsideTessFactor", i32 6, !"inside", i32 7, i32 9}
!20 = !{i32 140, !21}
!21 = !{i32 3, i32 0, i32 6, !"h"}
!22 = !{i32 44, !23}
!23 = !{i32 3, i32 0, i32 6, !"v"}
!24 = !{i32 1, void (i32, [3 x <4 x float>]*, [3 x <2 x float>]*, [3 x <3 x float>]*, <4 x float>*, <2 x float>*, <3 x float>*)* @main.flat, !25, void ([3 x <4 x float>]*, [3 x <2 x float>]*, [3 x <3 x float>]*, [3 x <4 x float>]*, [3 x <2 x float>]*, [3 x <3 x float>]*, [3 x float]*, float*)* @"\01?HSPerPatchFunc@@YA?AUHSPerPatchData@@V?$InputPatch@UPSSceneIn@@$02@@V?$OutputPatch@UPSSceneIn@@$02@@@Z.flat", !40}
!25 = !{!26, !28, !31, !33, !35, !37, !38, !39}
!26 = !{i32 0, !27, !27}
!27 = !{}
!28 = !{i32 0, !29, !30}
!29 = !{i32 4, !"SV_OutputControlPointID", i32 7, i32 5}
!30 = !{i32 0}
!31 = !{i32 3, !32, !30}
!32 = !{i32 4, !"SV_Position", i32 7, i32 9}
!33 = !{i32 3, !34, !30}
!34 = !{i32 4, !"TEXCOORD0", i32 7, i32 9}
!35 = !{i32 3, !36, !30}
!36 = !{i32 4, !"NORMAL", i32 7, i32 9}
!37 = !{i32 1, !32, !30}
!38 = !{i32 1, !34, !30}
!39 = !{i32 1, !36, !30}
!40 = !{!26, !31, !33, !35, !41, !42, !43, !44, !47}
!41 = !{i32 4, !32, !30}
!42 = !{i32 4, !34, !30}
!43 = !{i32 4, !36, !30}
!44 = !{i32 1, !45, !46}
!45 = !{i32 4, !"SV_TessFactor", i32 7, i32 9}
!46 = !{i32 0, i32 1, i32 2}
!47 = !{i32 1, !48, !30}
!48 = !{i32 4, !"SV_InsideTessFactor", i32 7, i32 9}
!49 = !{void (i32, [3 x <4 x float>]*, [3 x <2 x float>]*, [3 x <3 x float>]*, <4 x float>*, <2 x float>*, <3 x float>*)* @main.flat, !"", !50, null, !63}
!50 = !{!51, !56, !60}
!51 = !{!53, !54, !55}
!53 = !{i32 0, !"SV_Position", i8 9, i8 3, !30, i8 4, i32 1, i8 4, i32 0, i8 0, null}
!54 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !30, i8 2, i32 1, i8 2, i32 1, i8 0, null}
!55 = !{i32 2, !"NORMAL", i8 9, i8 0, !30, i8 2, i32 1, i8 3, i32 2, i8 0, null}
!56 = !{!57, !58, !59}
!57 = !{i32 0, !"SV_Position", i8 9, i8 3, !30, i8 4, i32 1, i8 4, i32 0, i8 0, null}
!58 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !30, i8 2, i32 1, i8 2, i32 1, i8 0, null}
!59 = !{i32 2, !"NORMAL", i8 9, i8 0, !30, i8 2, i32 1, i8 3, i32 2, i8 0, null}
!60 = !{!61, !62}
!61 = !{i32 0, !"SV_TessFactor", i8 9, i8 25, !46, i8 0, i32 3, i8 1, i32 0, i8 0, null}
!62 = !{i32 1, !"SV_InsideTessFactor", i8 9, i8 26, !30, i8 0, i32 1, i8 1, i32 3, i8 0, null}
!63 = !{i32 3, !64}
!64 = !{void ([3 x <4 x float>]*, [3 x <2 x float>]*, [3 x <3 x float>]*, [3 x <4 x float>]*, [3 x <2 x float>]*, [3 x <3 x float>]*, [3 x float]*, float*)* @"\01?HSPerPatchFunc@@YA?AUHSPerPatchData@@V?$InputPatch@UPSSceneIn@@$02@@V?$OutputPatch@UPSSceneIn@@$02@@@Z.flat", i32 3, i32 3, i32 2, i32 3, i32 2, float 6.400000e+01}
!65 = !{!66, !66, i64 0}
!66 = !{!"float", !67, i64 0}
!67 = !{!"omnipotent char", !68, i64 0}
!68 = !{!"Simple C/C++ TBAA"}

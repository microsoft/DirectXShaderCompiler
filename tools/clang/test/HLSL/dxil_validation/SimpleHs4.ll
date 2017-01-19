; RUN: %dxv %s | FileCheck %s

; CHECK: Hull Shader declared with IsoLine Domain must specify output primitive point or line. Triangle_cw or triangle_ccw output are not compatible with the IsoLine Domain.


;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float
; TEXCOORD                 0   xy          1     NONE   float
; NORMAL                   0   xyz         2     NONE   float
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float   xyzw
; TEXCOORD                 0   xy          1     NONE   float   xyzw
; NORMAL                   0   xyz         2     NONE   float   xyzw
;
;
; Patch Constant signature signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_TessFactor            0   x           0  LINEDEN   float   xyzw
; SV_TessFactor            1   x           1  LINEDET   float   xyzw
;
;
; Pipeline Runtime Information:
;
; Hull Shader
; InputControlPointCount=2
; OutputControlPointCount=2
; Domain=isoline
; OutputPrimitive=line
;
;
; Input signature:
;
; Name                 Index             InterpMode
; -------------------- ----- ----------------------
; SV_Position              0          noperspective
; TEXCOORD                 0                 linear
; NORMAL                   0                 linear
;
; Output signature:
;
; Name                 Index             InterpMode
; -------------------- ----- ----------------------
; SV_Position              0          noperspective
; TEXCOORD                 0                 linear
; NORMAL                   0                 linear
;
; Patch Constant signature signature:
;
; Name                 Index             InterpMode
; -------------------- ----- ----------------------
; SV_TessFactor            0
;
; Buffer Definitions:
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
;
target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%class.Texture2D = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%struct.PSSceneIn = type { <4 x float>, <2 x float>, <3 x float> }
%struct.VSSceneIn = type { <3 x float>, <3 x float>, <2 x float> }
%struct.HSPerPatchData = type { [2 x float] }
%struct.HSPerVertexData = type { %struct.PSSceneIn }
%class.InputPatch = type { [2 x %struct.PSSceneIn] }
%class.OutputPatch = type { [2 x %struct.HSPerVertexData] }

@dx.typevar.0 = external addrspace(1) constant %class.Texture2D
@dx.typevar.1 = external addrspace(1) constant %"class.Texture2D<vector<float, 4> >::mips_type"
@dx.typevar.2 = external addrspace(1) constant %struct.PSSceneIn
@dx.typevar.3 = external addrspace(1) constant %struct.VSSceneIn
@dx.typevar.4 = external addrspace(1) constant %struct.HSPerPatchData
@dx.typevar.5 = external addrspace(1) constant %struct.HSPerVertexData
@dx.typevar.6 = external addrspace(1) constant %class.InputPatch
@dx.typevar.7 = external addrspace(1) constant %class.OutputPatch
@llvm.used = appending global [8 x i8*] [i8* addrspacecast (i8 addrspace(1)* bitcast (%class.Texture2D addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%"class.Texture2D<vector<float, 4> >::mips_type" addrspace(1)* @dx.typevar.1 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.PSSceneIn addrspace(1)* @dx.typevar.2 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.VSSceneIn addrspace(1)* @dx.typevar.3 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.HSPerPatchData addrspace(1)* @dx.typevar.4 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.HSPerVertexData addrspace(1)* @dx.typevar.5 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.InputPatch addrspace(1)* @dx.typevar.6 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.OutputPatch addrspace(1)* @dx.typevar.7 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @"\01?HSPerPatchFunc@@YA?AUHSPerPatchData@@V?$InputPatch@UPSSceneIn@@$01@@V?$OutputPatch@UHSPerVertexData@@$01@@@Z.flat"([2 x <4 x float>]* nocapture readnone, [2 x <2 x float>]* nocapture readnone, [2 x <3 x float>]* nocapture readnone, [2 x <4 x float>]* nocapture readnone, [2 x <2 x float>]* nocapture readnone, [2 x <3 x float>]* nocapture readnone, [2 x float]* nocapture readnone) #0 {
entry:
  %retval.0 = alloca [2 x float], align 4
  %7 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 0, i32 0)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %arrayidx2 = getelementptr inbounds [2 x float], [2 x float]* %retval.0, i32 0, i32 0
  store float %7, float* %arrayidx2, align 4, !tbaa !61
  %8 = call float @dx.op.loadOutputControlPoint.f32(i32 106, i32 1, i32 0, i8 0, i32 1)  ; LoadOutputControlPoint(inputSigId,row,col,index)
  %arrayidx31 = getelementptr inbounds [2 x float], [2 x float]* %retval.0, i32 0, i32 1
  store float %8, float* %arrayidx31, align 4, !tbaa !61
  %load = load [2 x float], [2 x float]* %retval.0, align 4
  %9 = extractvalue [2 x float] %load, 0
  call void @dx.op.storePatchConstant.f32(i32 109, i32 0, i32 0, i8 0, float %9)  ; StorePatchConstant(outputSigID,row,col,value)
  %10 = extractvalue [2 x float] %load, 1
  call void @dx.op.storePatchConstant.f32(i32 109, i32 0, i32 1, i8 0, float %10)  ; StorePatchConstant(outputSigID,row,col,value)
  ret void
}

; Function Attrs: nounwind
define void @main.flat(i32, [2 x <4 x float>]* nocapture readnone, [2 x <2 x float>]* nocapture readnone, [2 x <3 x float>]* nocapture readnone, <4 x float>* nocapture readnone, <2 x float>* nocapture readnone, <3 x float>* nocapture readnone) #0 {
entry:
  %7 = call i32 @dx.op.outputControlPointID.i32(i32 110)  ; OutputControlPointID()
  %8 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 %7)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %9 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 %7)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %10 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 %7)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %11 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 %7)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %12 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 %7)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %13 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 %7)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %14 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 0, i32 %7)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %15 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 1, i32 %7)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %16 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 2, i32 %7)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %8)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %9)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %10)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %11)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %12)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %13)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float %14)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 1, float %15)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 2, float %16)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
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
!dx.typeAnnotations = !{!3, !23}
!dx.entryPoints = !{!46}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 1, i32 0}
!2 = !{!"hs", i32 6, i32 0}
!3 = !{i32 0, %class.Texture2D addrspace(1)* @dx.typevar.0, !4, %"class.Texture2D<vector<float, 4> >::mips_type" addrspace(1)* @dx.typevar.1, !7, %struct.PSSceneIn addrspace(1)* @dx.typevar.2, !9, %struct.VSSceneIn addrspace(1)* @dx.typevar.3, !13, %struct.HSPerPatchData addrspace(1)* @dx.typevar.4, !17, %struct.HSPerVertexData addrspace(1)* @dx.typevar.5, !19, %class.InputPatch addrspace(1)* @dx.typevar.6, !21, %class.OutputPatch addrspace(1)* @dx.typevar.7, !21}
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
!17 = !{i32 20, !18}
!18 = !{i32 3, i32 0, i32 4, !"SV_TessFactor", i32 6, !"edges", i32 7, i32 9}
!19 = !{i32 44, !20}
!20 = !{i32 3, i32 0, i32 6, !"v"}
!21 = !{i32 92, !22}
!22 = !{i32 3, i32 0, i32 6, !"h"}
!23 = !{i32 1, void ([2 x <4 x float>]*, [2 x <2 x float>]*, [2 x <3 x float>]*, [2 x <4 x float>]*, [2 x <2 x float>]*, [2 x <3 x float>]*, [2 x float]*)* @"\01?HSPerPatchFunc@@YA?AUHSPerPatchData@@V?$InputPatch@UPSSceneIn@@$01@@V?$OutputPatch@UHSPerVertexData@@$01@@@Z.flat", !24, void (i32, [2 x <4 x float>]*, [2 x <2 x float>]*, [2 x <3 x float>]*, <4 x float>*, <2 x float>*, <3 x float>*)* @main.flat, !40}
!24 = !{!25, !27, !30, !32, !34, !35, !36, !37}
!25 = !{i32 0, !26, !26}
!26 = !{}
!27 = !{i32 3, !28, !29}
!28 = !{i32 4, !"SV_Position", i32 7, i32 9}
!29 = !{i32 0}
!30 = !{i32 3, !31, !29}
!31 = !{i32 4, !"TEXCOORD0", i32 7, i32 9}
!32 = !{i32 3, !33, !29}
!33 = !{i32 4, !"NORMAL", i32 7, i32 9}
!34 = !{i32 4, !28, !29}
!35 = !{i32 4, !31, !29}
!36 = !{i32 4, !33, !29}
!37 = !{i32 1, !38, !39}
!38 = !{i32 4, !"SV_TessFactor", i32 7, i32 9}
!39 = !{i32 0, i32 1}
!40 = !{!25, !41, !27, !30, !32, !43, !44, !45}
!41 = !{i32 0, !42, !29}
!42 = !{i32 4, !"SV_OutputControlPointID", i32 7, i32 5}
!43 = !{i32 1, !28, !29}
!44 = !{i32 1, !31, !29}
!45 = !{i32 1, !33, !29}
!46 = !{void (i32, [2 x <4 x float>]*, [2 x <2 x float>]*, [2 x <3 x float>]*, <4 x float>*, <2 x float>*, <3 x float>*)* @main.flat, !"", !47, null, !59}
!47 = !{!48, !53, !57}
!48 = !{!50, !51, !52}
!50 = !{i32 0, !"SV_Position", i8 9, i8 3, !29, i8 4, i32 1, i8 4, i32 0, i8 0, null}
!51 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !29, i8 2, i32 1, i8 2, i32 1, i8 0, null}
!52 = !{i32 2, !"NORMAL", i8 9, i8 0, !29, i8 2, i32 1, i8 3, i32 2, i8 0, null}
!53 = !{!54, !55, !56}
!54 = !{i32 0, !"SV_Position", i8 9, i8 3, !29, i8 4, i32 1, i8 4, i32 0, i8 0, null}
!55 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !29, i8 2, i32 1, i8 2, i32 1, i8 0, null}
!56 = !{i32 2, !"NORMAL", i8 9, i8 0, !29, i8 2, i32 1, i8 3, i32 2, i8 0, null}
!57 = !{!58}
!58 = !{i32 0, !"SV_TessFactor", i8 9, i8 25, !39, i8 0, i32 2, i8 1, i32 0, i8 0, null}
!59 = !{i32 3, !60}
!60 = !{void ([2 x <4 x float>]*, [2 x <2 x float>]*, [2 x <3 x float>]*, [2 x <4 x float>]*, [2 x <2 x float>]*, [2 x <3 x float>]*, [2 x float]*)* @"\01?HSPerPatchFunc@@YA?AUHSPerPatchData@@V?$InputPatch@UPSSceneIn@@$01@@V?$OutputPatch@UHSPerVertexData@@$01@@@Z.flat", i32 2, i32 2, i32 1, i32 3, i32 3, float 6.400000e+01}
!61 = !{!62, !62, i64 0}
!62 = !{!"float", !63, i64 0}
!63 = !{!"omnipotent char", !64, i64 0}
!64 = !{!"Simple C/C++ TBAA"}


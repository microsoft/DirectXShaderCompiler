; RUN: %dxv %s | FileCheck %s

;
; Note: shader requires additional functionality:
;       SV_RenderTargetArrayIndex or SV_ViewportArrayIndex from any shader feeding rasterizer
;
;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; POSSIZE                  0   xyz         0     NONE   float
; COLOR                    0   xyzw        1     NONE   float
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; TEXCOORD                 0   xy          0     NONE   float   xyzw
; COLOR                    0   xyzw        1     NONE   float   xyzw
; SV_Position              0   xyzw        2      POS   float   xyzw
; TEXCOORD                 0   xy          3     NONE   float   xyzw
; COLOR                    0   xyzw        4     NONE   float   xyzw
; SV_Position              0   xyzw        5      POS   float   xyzw
; SV_ViewportArrayIndex     0   x           6  VPINDEX    uint   xyzw
;
;
; Pipeline Runtime Information:
;
; Geometry Shader
; InputPrimitive=point
; OutputTopology=point
; OutputStreamMask=3
; OutputPositionPresent=1
;
;
; Input signature:
;
; Name                 Index             InterpMode
; -------------------- ----- ----------------------
; POSSIZE                  0                 linear
; COLOR                    0                 linear
; SV_GSInstanceID          0        nointerpolation
;
; Output signature:
;
; Name                 Index             InterpMode
; -------------------- ----- ----------------------
; TEXCOORD                 0                 linear
; COLOR                    0                 linear
; SV_Position              0          noperspective
; TEXCOORD                 0                 linear
; COLOR                    0                 linear
; SV_Position              0          noperspective
; SV_ViewportArrayIndex     0        nointerpolation
;
; Buffer Definitions:
;
; cbuffer b
; {
;
;   struct b
;   {
;
;       float2 invViewportSize;                       ; Offset:    0
;
;   } b                                               ; Offset:    0 Size:     8
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; b                                 cbuffer      NA          NA     CB0            cb0     1
;
target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%b = type { <2 x float> }
%struct.VSOutGSIn = type { <3 x float>, <4 x float> }
%class.PointStream = type { %struct.VSOut }
%struct.VSOut = type { <2 x float>, <4 x float>, <4 x float> }
%dx.types.Handle = type { i8* }
%dx.types.CBufRet.f32 = type { float, float, float, float }

@b = external constant %b
@dx.typevar.0 = external addrspace(1) constant %struct.VSOutGSIn
@dx.typevar.1 = external addrspace(1) constant %class.PointStream
@dx.typevar.2 = external addrspace(1) constant %struct.VSOut
@dx.typevar.3 = external addrspace(1) constant %b
@llvm.used = appending global [5 x i8*] [i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.VSOutGSIn addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.PointStream addrspace(1)* @dx.typevar.1 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.VSOut addrspace(1)* @dx.typevar.2 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%b addrspace(1)* @dx.typevar.3 to i8 addrspace(1)*) to i8*), i8* bitcast (%b* @b to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @"\01?main@@YAXY00UVSOutGSIn@@V?$PointStream@UVSOut@@@@1IAAI@Z.flat"([1 x <3 x float>]* nocapture readnone, [1 x <4 x float>]* nocapture readnone, %class.PointStream* nocapture readnone, <2 x float>* nocapture readnone, <4 x float>* nocapture readnone, <4 x float>* nocapture readnone, %class.PointStream* nocapture readnone, <2 x float>* nocapture readnone, <4 x float>* nocapture readnone, <4 x float>* nocapture readnone, i32, i32* nocapture readnone) #0 {
entry:
  %12 = tail call i32 @dx.op.gsInstanceID.i32(i32 138)  ; GSInstanceID()
  %verts.0 = alloca [3 x float], align 4
  %verts.1 = alloca [3 x float], align 4
  %13 = getelementptr [3 x float], [3 x float]* %verts.0, i32 0, i32 0
  %14 = getelementptr [3 x float], [3 x float]* %verts.1, i32 0, i32 0
  store float -5.000000e-01, float* %13, align 4
  store float -5.000000e-01, float* %14, align 4
  %15 = getelementptr [3 x float], [3 x float]* %verts.0, i32 0, i32 1
  %16 = getelementptr [3 x float], [3 x float]* %verts.1, i32 0, i32 1
  store float 1.500000e+00, float* %15, align 4
  store float -5.000000e-01, float* %16, align 4
  %17 = getelementptr [3 x float], [3 x float]* %verts.0, i32 0, i32 2
  %18 = getelementptr [3 x float], [3 x float]* %verts.1, i32 0, i32 2
  store float -5.000000e-01, float* %17, align 4
  store float 1.500000e+00, float* %18, align 4
  %19 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 0)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %20 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 0)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %21 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 0)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %22 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 0)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %23 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 0)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %24 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 0)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %25 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 3, i32 0)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %rem = urem i32 %12, 3
  %26 = getelementptr [3 x float], [3 x float]* %verts.0, i32 0, i32 %rem
  %27 = getelementptr [3 x float], [3 x float]* %verts.1, i32 0, i32 %rem
  %load30 = load float, float* %26, align 4
  %load32 = load float, float* %27, align 4
  %mul.i0 = fmul fast float %load30, %19
  %mul.i1 = fmul fast float %load32, %19
  %add.i0 = fadd fast float %mul.i0, %20
  %add.i1 = fadd fast float %mul.i1, %21
  %28 = tail call %dx.types.Handle @dx.op.createHandle(i32 58, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %29 = tail call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 60, %dx.types.Handle %28, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %30 = extractvalue %dx.types.CBufRet.f32 %29, 0
  %31 = extractvalue %dx.types.CBufRet.f32 %29, 1
  %mul.i.i0 = fmul fast float %30, 2.000000e+00
  %mul.i.i1 = fmul fast float %31, 2.000000e+00
  %mul1.i.i0 = fmul fast float %mul.i.i0, %add.i0
  %mul1.i.i1 = fmul fast float %mul.i.i1, %add.i1
  %sub.i = fadd fast float %mul1.i.i0, -1.000000e+00
  %sub2.i = fsub fast float 1.000000e+00, %mul1.i.i1
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 0.000000e+00)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 0.000000e+00)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %22)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %23)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float %24)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float %25)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float %sub.i)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 1, float %sub2.i)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 2, float 5.000000e-01)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 3, float 1.000000e+00)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.emitStream(i32 97, i8 0)  ; EmitStream(streamId)
  %add10 = add nuw nsw i32 %rem, 1
  %32 = getelementptr [3 x float], [3 x float]* %verts.0, i32 0, i32 %add10
  %33 = getelementptr [3 x float], [3 x float]* %verts.1, i32 0, i32 %add10
  %load26 = load float, float* %32, align 4
  %load28 = load float, float* %33, align 4
  %mul14.i0 = fmul fast float %load26, %19
  %mul14.i1 = fmul fast float %load28, %19
  %add15.i0 = fadd fast float %mul14.i0, %20
  %add15.i1 = fadd fast float %mul14.i1, %21
  %mul1.i.31.i0 = fmul fast float %add15.i0, %mul.i.i0
  %mul1.i.31.i1 = fmul fast float %add15.i1, %mul.i.i1
  %sub.i.32 = fadd fast float %mul1.i.31.i0, -1.000000e+00
  %sub2.i.33 = fsub fast float 1.000000e+00, %mul1.i.31.i1
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 2.000000e+00)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 0.000000e+00)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %22)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %23)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float %24)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float %25)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float %sub.i.32)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 1, float %sub2.i.33)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 2, float 5.000000e-01)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 3, float 1.000000e+00)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.emitStream(i32 97, i8 0)  ; EmitStream(streamId)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 0, float 2.000000e+00)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 1, float 0.000000e+00)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 4, i32 0, i8 0, float %22)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 4, i32 0, i8 1, float %23)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 4, i32 0, i8 2, float %24)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 4, i32 0, i8 3, float %25)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 0, float %sub.i.32)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 1, float %sub2.i.33)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 2, float 5.000000e-01)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 3, float 1.000000e+00)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.emitStream(i32 97, i8 1)  ; EmitStream(streamId)
  %add21 = add nuw nsw i32 %rem, 2
  %34 = getelementptr [3 x float], [3 x float]* %verts.0, i32 0, i32 %add21
  %35 = getelementptr [3 x float], [3 x float]* %verts.1, i32 0, i32 %add21
  %load23 = load float, float* %34, align 4
  %load24 = load float, float* %35, align 4
  %mul25.i0 = fmul fast float %load23, %19
  %mul25.i1 = fmul fast float %load24, %19
  %add26.i0 = fadd fast float %mul25.i0, %20
  %add26.i1 = fadd fast float %mul25.i1, %21
  %mul1.i.36.i0 = fmul fast float %add26.i0, %mul.i.i0
  %mul1.i.36.i1 = fmul fast float %add26.i1, %mul.i.i1
  %sub.i.37 = fadd fast float %mul1.i.36.i0, -1.000000e+00
  %sub2.i.38 = fsub fast float 1.000000e+00, %mul1.i.36.i1
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 0.000000e+00)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 2.000000e+00)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %22)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %23)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float %24)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float %25)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float %sub.i.37)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 1, float %sub2.i.38)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 2, float 5.000000e-01)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 3, float 1.000000e+00)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.emitStream(i32 97, i8 0)  ; EmitStream(streamId)
  tail call void @dx.op.storeOutput.i32(i32 5, i32 6, i32 0, i8 0, i32 2)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  tail call void @dx.op.cutStream(i32 98, i8 0)  ; CutStream(streamId)
  tail call void @dx.op.cutStream(i32 98, i8 1)  ; CutStream(streamId)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind readnone
declare i32 @dx.op.gsInstanceID.i32(i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.i32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind readnone
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind
declare void @dx.op.cutStream(i32, i8) #0

; Function Attrs: nounwind
declare void @dx.op.emitStream(i32, i8) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.typeAnnotations = !{!6, !17}
!dx.entryPoints = !{!40}

!0 = !{!"clang version 3.7.0 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 4}
!2 = !{!"gs", i32 5, i32 0}
!3 = !{null, null, !4, null}
!4 = !{!5}
!5 = !{i32 0, %b* @b, !"b", i32 0, i32 0, i32 1, i32 8, null}
!6 = !{i32 0, %struct.VSOutGSIn addrspace(1)* @dx.typevar.0, !7, %class.PointStream addrspace(1)* @dx.typevar.1, !10, %struct.VSOut addrspace(1)* @dx.typevar.2, !12, %b addrspace(1)* @dx.typevar.3, !15}
!7 = !{i32 32, !8, !9}
!8 = !{i32 3, i32 0, i32 4, !"POSSIZE", i32 6, !"posSize", i32 7, i32 9}
!9 = !{i32 3, i32 16, i32 4, !"COLOR", i32 6, !"clr", i32 7, i32 9}
!10 = !{i32 48, !11}
!11 = !{i32 3, i32 0, i32 6, !"h"}
!12 = !{i32 48, !13, !9, !14}
!13 = !{i32 3, i32 0, i32 4, !"TEXCOORD0", i32 6, !"uv", i32 7, i32 9}
!14 = !{i32 3, i32 32, i32 4, !"SV_Position", i32 6, !"pos", i32 7, i32 9}
!15 = !{i32 0, !16}
!16 = !{i32 3, i32 0, i32 6, !"invViewportSize", i32 7, i32 9}
!17 = !{i32 1, void ([1 x <3 x float>]*, [1 x <4 x float>]*, %class.PointStream*, <2 x float>*, <4 x float>*, <4 x float>*, %class.PointStream*, <2 x float>*, <4 x float>*, <4 x float>*, i32, i32*)* @"\01?main@@YAXY00UVSOutGSIn@@V?$PointStream@UVSOut@@@@1IAAI@Z.flat", !18}
!18 = !{!19, !21, !24, !26, !27, !29, !30, !32, !33, !34, !35, !36, !38}
!19 = !{i32 0, !20, !20}
!20 = !{}
!21 = !{i32 0, !22, !23}
!22 = !{i32 4, !"POSSIZE", i32 7, i32 9}
!23 = !{i32 0}
!24 = !{i32 0, !25, !23}
!25 = !{i32 4, !"COLOR", i32 7, i32 9}
!26 = !{i32 5, !20, !20}
!27 = !{i32 5, !28, !23}
!28 = !{i32 4, !"TEXCOORD0", i32 7, i32 9}
!29 = !{i32 5, !25, !23}
!30 = !{i32 5, !31, !23}
!31 = !{i32 4, !"SV_Position", i32 7, i32 9}
!32 = !{i32 6, !20, !20}
!33 = !{i32 6, !28, !23}
!34 = !{i32 6, !25, !23}
!35 = !{i32 6, !31, !23}
!36 = !{i32 0, !37, !23}
!37 = !{i32 4, !"SV_GSInstanceID", i32 7, i32 5}
!38 = !{i32 1, !39, !23}
!39 = !{i32 4, !"SV_ViewportArrayIndex", i32 7, i32 5}
!40 = !{void ([1 x <3 x float>]*, [1 x <4 x float>]*, %class.PointStream*, <2 x float>*, <4 x float>*, <4 x float>*, %class.PointStream*, <2 x float>*, <4 x float>*, <4 x float>*, i32, i32*)* @"\01?main@@YAXY00UVSOutGSIn@@V?$PointStream@UVSOut@@@@1IAAI@Z.flat", !"", !41, !3, !54}
!41 = !{!42, !46, null}
!42 = !{!43, !44, !45}
!43 = !{i32 0, !"POSSIZE", i8 9, !23, i8 2, i32 1, i8 3, i32 0, i8 0, null}
!44 = !{i32 1, !"COLOR", i8 9, !23, i8 2, i32 1, i8 4, i32 1, i8 0, null}
!45 = !{i32 2, !"SV_GSInstanceID", i8 5, !23, i8 1, i32 1, i8 1, i32 2, i8 0, null}
!46 = !{!47, !44, !48, !49, !51, !52, !53}
!47 = !{i32 0, !"TEXCOORD", i8 9, !23, i8 2, i32 1, i8 2, i32 0, i8 0, null}
!48 = !{i32 2, !"SV_Position", i8 9, !23, i8 4, i32 1, i8 4, i32 2, i8 0, null}
!49 = !{i32 3, !"TEXCOORD", i8 9, !23, i8 2, i32 1, i8 2, i32 3, i8 0, !50}
!50 = !{i32 0, i32 1}
!51 = !{i32 4, !"COLOR", i8 9, !23, i8 2, i32 1, i8 4, i32 4, i8 0, !50}
!52 = !{i32 5, !"SV_Position", i8 9, !23, i8 4, i32 1, i8 4, i32 5, i8 0, !50}
!53 = !{i32 6, !"SV_ViewportArrayIndex", i8 5, !23, i8 1, i32 1, i8 1, i32 6, i8 0, null}
!54 = !{i32 0, i64 512, i32 1, !55}
!55 = !{i32 1, i32 3, i32 3, i32 1, i32 24}


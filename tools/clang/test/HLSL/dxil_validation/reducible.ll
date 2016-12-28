; RUN: %dxv %s | FileCheck %s
; CHECK: Execution flow must be reducible

target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%struct.Interpolants2 = type { <4 x float>, <4 x float>, <4 x float> }
%struct.Inh = type { %struct.Interpolants, float }
%struct.Interpolants = type { <4 x float>, <4 x float> }
%"$Globals" = type { %struct.Interpolants2, %struct.Inh, i32, <4 x i32> }
%struct.Vertex = type { <4 x float>, <4 x float> }
%dx.types.Handle = type { i8* }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.CBufRet.f32 = type { float, float, float, float }

@"\01?c2@@3UInterpolants2@@A" = global %struct.Interpolants2 zeroinitializer, align 4
@"\01?c@@3UInh@@A" = global %struct.Inh zeroinitializer, align 4
@"\01?i@@3HA" = global i32 0, align 4
@"\01?i4@@3V?$vector@I$03@@A" = global <4 x i32> zeroinitializer, align 4
@"$Globals" = external constant %"$Globals"
@dx.typevar.0 = external addrspace(1) constant %struct.Interpolants2
@dx.typevar.1 = external addrspace(1) constant %struct.Inh
@dx.typevar.2 = external addrspace(1) constant %struct.Interpolants
@dx.typevar.3 = external addrspace(1) constant %struct.Vertex
@dx.typevar.4 = external addrspace(1) constant %"$Globals"
@llvm.used = appending global [7 x i8*] [i8* bitcast (%"$Globals"* @"$Globals" to i8*), i8* bitcast (%"$Globals"* @"$Globals" to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.Interpolants2 addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.Inh addrspace(1)* @dx.typevar.1 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.Interpolants addrspace(1)* @dx.typevar.2 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.Vertex addrspace(1)* @dx.typevar.3 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%"$Globals" addrspace(1)* @dx.typevar.4 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @main.flat(<4 x float>* nocapture readnone, <4 x float>* nocapture readnone, <4 x float>* nocapture readnone, <4 x float>* nocapture readnone) #0 {
entry:
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 58, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 60, %dx.types.Handle %4, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %6 = extractvalue %dx.types.CBufRet.i32 %5, 1
  %cmp = icmp sgt i32 %6, 1
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %7 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 60, %dx.types.Handle %4, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %8 = extractvalue %dx.types.CBufRet.i32 %7, 2
  %9 = uitofp i32 %8 to float
  br label %if.then.5

if.else:                                          ; preds = %entry
  %cmp2 = icmp sgt i32 %6, 0
  br i1 %cmp2, label %if.then.5, label %if.else.6

if.then.5:                                        ; preds = %if.else
  %10 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 60, %dx.types.Handle %4, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %11 = extractvalue %dx.types.CBufRet.f32 %10, 0
  %12 = extractvalue %dx.types.CBufRet.f32 %10, 1
  %13 = extractvalue %dx.types.CBufRet.f32 %10, 2
  %14 = extractvalue %dx.types.CBufRet.f32 %10, 3
  %15 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 60, %dx.types.Handle %4, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %16 = extractvalue %dx.types.CBufRet.f32 %15, 0
  %17 = extractvalue %dx.types.CBufRet.f32 %15, 1
  %18 = extractvalue %dx.types.CBufRet.f32 %15, 2
  %19 = extractvalue %dx.types.CBufRet.f32 %15, 3
  %cmp12 = icmp sgt i32 %6, 1
  br i1 %cmp2, label %if.then, label %if.else.6  

if.else.6:                                        ; preds = %if.else
  %cmp7 = icmp sgt i32 %6, -1
  br i1 %cmp7, label %if.then.10, label %if.end.13

if.then.10:                                       ; preds = %if.else.6
  %20 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 60, %dx.types.Handle %4, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %21 = extractvalue %dx.types.CBufRet.f32 %20, 0
  %22 = extractvalue %dx.types.CBufRet.f32 %20, 1
  %23 = extractvalue %dx.types.CBufRet.f32 %20, 2
  %24 = extractvalue %dx.types.CBufRet.f32 %20, 3
  %25 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 60, %dx.types.Handle %4, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %26 = extractvalue %dx.types.CBufRet.f32 %25, 0
  %27 = extractvalue %dx.types.CBufRet.f32 %25, 1
  %28 = extractvalue %dx.types.CBufRet.f32 %25, 2
  %29 = extractvalue %dx.types.CBufRet.f32 %25, 3
  br label %return

if.end.13:                                        ; preds = %if.else.6
  %30 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %31 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %32 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %33 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %34 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %35 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %36 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %37 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  br label %return

return:                                           ; preds = %if.end.13, %if.then.10, %if.then.5, %if.then
  %retval.1.0.i0 = phi float [ %9, %if.then ], [ %16, %if.then.5 ], [ %26, %if.then.10 ], [ %34, %if.end.13 ]
  %retval.1.0.i1 = phi float [ %9, %if.then ], [ %17, %if.then.5 ], [ %27, %if.then.10 ], [ %35, %if.end.13 ]
  %retval.1.0.i2 = phi float [ %9, %if.then ], [ %18, %if.then.5 ], [ %28, %if.then.10 ], [ %36, %if.end.13 ]
  %retval.1.0.i3 = phi float [ %9, %if.then ], [ %19, %if.then.5 ], [ %29, %if.then.10 ], [ %37, %if.end.13 ]
  %retval.0.0.i0 = phi float [ %9, %if.then ], [ %11, %if.then.5 ], [ %21, %if.then.10 ], [ %30, %if.end.13 ]
  %retval.0.0.i1 = phi float [ %9, %if.then ], [ %12, %if.then.5 ], [ %22, %if.then.10 ], [ %31, %if.end.13 ]
  %retval.0.0.i2 = phi float [ %9, %if.then ], [ %13, %if.then.5 ], [ %23, %if.then.10 ], [ %32, %if.end.13 ]
  %retval.0.0.i3 = phi float [ %9, %if.then ], [ %14, %if.then.5 ], [ %24, %if.then.10 ], [ %33, %if.end.13 ]
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %retval.0.0.i0)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %retval.0.0.i1)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %retval.0.0.i2)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %retval.0.0.i3)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %retval.1.0.i0)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %retval.1.0.i1)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float %retval.1.0.i2)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float %retval.1.0.i3)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind readnone
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readnone
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.typeAnnotations = !{!6, !22}
!dx.entryPoints = !{!34}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 7}
!2 = !{!"vs", i32 5, i32 1}
!3 = !{null, null, !4, null}
!4 = !{!5}
!5 = !{i32 0, %"$Globals"* @"$Globals", !"$Globals", i32 0, i32 0, i32 1, i32 112, null}
!6 = !{i32 0, %struct.Interpolants2 addrspace(1)* @dx.typevar.0, !7, %struct.Inh addrspace(1)* @dx.typevar.1, !11, %struct.Interpolants addrspace(1)* @dx.typevar.2, !14, %struct.Vertex addrspace(1)* @dx.typevar.3, !15, %"$Globals" addrspace(1)* @dx.typevar.4, !17}
!7 = !{i32 48, !8, !9, !10}
!8 = !{i32 3, i32 0, i32 4, !"SV_POSITION0", i32 6, !"position", i32 7, i32 9}
!9 = !{i32 3, i32 16, i32 4, !"COLOR0", i32 6, !"color", i32 7, i32 9}
!10 = !{i32 3, i32 32, i32 4, !"COLOR2", i32 6, !"color2", i32 7, i32 9}
!11 = !{i32 36, !12, !13}
!12 = !{i32 3, i32 0, i32 6, !"Interpolants"}
!13 = !{i32 3, i32 32, i32 6, !"a", i32 7, i32 9}
!14 = !{i32 32, !8, !9}
!15 = !{i32 32, !16, !9}
!16 = !{i32 3, i32 0, i32 4, !"POSITION0", i32 6, !"position", i32 7, i32 9}
!17 = !{i32 0, !18, !19, !20, !21}
!18 = !{i32 3, i32 0, i32 6, !"c2"}
!19 = !{i32 3, i32 48, i32 6, !"c"}
!20 = !{i32 3, i32 84, i32 6, !"i", i32 7, i32 4}
!21 = !{i32 3, i32 96, i32 6, !"i4", i32 7, i32 5}
!22 = !{i32 1, void (<4 x float>*, <4 x float>*, <4 x float>*, <4 x float>*)* @main.flat, !23}
!23 = !{!24, !26, !29, !31, !33}
!24 = !{i32 0, !25, !25}
!25 = !{}
!26 = !{i32 0, !27, !28}
!27 = !{i32 4, !"POSITION0", i32 7, i32 9}
!28 = !{i32 0}
!29 = !{i32 0, !30, !28}
!30 = !{i32 4, !"COLOR0", i32 7, i32 9}
!31 = !{i32 1, !32, !28}
!32 = !{i32 4, !"SV_POSITION0", i32 7, i32 9}
!33 = !{i32 1, !30, !28}
!34 = !{void (<4 x float>*, <4 x float>*, <4 x float>*, <4 x float>*)* @main.flat, !"", !35, !3, null}
!35 = !{!36, !39, null}
!36 = !{!37, !38}
!37 = !{i32 0, !"POSITION", i8 9, i8 3, !28, i8 0, i32 1, i8 4, i32 0, i8 0, null}
!38 = !{i32 1, !"COLOR", i8 9, i8 0, !28, i8 0, i32 1, i8 4, i32 1, i8 0, null}
!39 = !{!40, !41}
!40 = !{i32 0, !"SV_Position", i8 9, i8 3, !28, i8 4, i32 1, i8 4, i32 0, i8 0, null}
!41 = !{i32 1, !"COLOR", i8 9, i8 0, !28, i8 2, i32 1, i8 4, i32 1, i8 0, null}


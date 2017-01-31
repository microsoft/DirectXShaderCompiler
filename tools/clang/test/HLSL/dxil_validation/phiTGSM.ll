; RUN: %dxv %s | FileCheck %s
; CHECK: TGSM pointers must originate from an unambiguous TGSM global variable

;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; no %s
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; no %s
;
; Pipeline Runtime Information:
;
;
;
; Buffer Definitions:
;
; cbuffer $Globals
; {
;
;   struct $Globals
;   {
;
;       float t;                                      ; Offset:    0
;
;   } $Globals                                        ; Offset:    0 Size:     4
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
;
target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%"$Globals" = type { float }
%dx.types.Handle = type { i8* }
%dx.types.CBufRet.f32 = type { float, float, float, float }

@"\01?g_Data@@3PAIA" = addrspace(3) global [32 x i32] zeroinitializer, align 4
@"\01?g_Data2@@3PAIA" = addrspace(3) global [32 x i32] zeroinitializer, align 4
@"\01?t@@3MA" = global float 0.000000e+00, align 4
@dx.typevar.0 = external addrspace(1) constant %"$Globals"
@llvm.used = appending global [1 x i8*] [i8* addrspacecast (i8 addrspace(1)* bitcast (%"$Globals" addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: alwaysinline nounwind
define void @main(i32 %idx) #0 {
entry:
  %0 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 58, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 60, %dx.types.Handle %1, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %3 = extractvalue %dx.types.CBufRet.f32 %2, 0
  %cmp = fcmp fast ogt float %3, 1.000000e+00
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %arrayidx = getelementptr inbounds [32 x i32], [32 x i32] addrspace(3)* @"\01?g_Data@@3PAIA", i32 0, i32 %0
  br label %if.end

if.else:                                          ; preds = %entry
  %arrayidx2 = getelementptr inbounds [32 x i32], [32 x i32] addrspace(3)* @"\01?g_Data2@@3PAIA", i32 0, i32 %0
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %arrayPhi = phi i32 addrspace(3)* [ %arrayidx, %if.then ], [ %arrayidx2, %if.else ]
  %4 = atomicrmw add i32 addrspace(3)* %arrayPhi, i32 1 seq_cst
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind readnone
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #1

attributes #0 = { alwaysinline nounwind "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-realign-stack" "stack-protector-buffer-size"="0" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.typeAnnotations = !{!6, !9}
!dx.entryPoints = !{!16}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 6}
!2 = !{!"cs", i32 6, i32 0}
!3 = !{null, null, !4, null}
!4 = !{!5}
!5 = !{i32 0, %"$Globals"* undef, !"$Globals", i32 0, i32 0, i32 1, i32 4, null}
!6 = !{i32 0, %"$Globals" addrspace(1)* @dx.typevar.0, !7}
!7 = !{i32 0, !8}
!8 = !{i32 3, i32 0, i32 6, !"t", i32 7, i32 9}
!9 = !{i32 1, void (i32)* @main, !10}
!10 = !{!11, !13}
!11 = !{i32 1, !12, !12}
!12 = !{}
!13 = !{i32 0, !14, !15}
!14 = !{i32 4, !"SV_DispatchThreadId", i32 7, i32 5}
!15 = !{i32 0}
!16 = !{void (i32)* @main, !"", null, !3, !17}
!17 = !{i32 4, !18}
!18 = !{i32 64, i32 1, i32 1}


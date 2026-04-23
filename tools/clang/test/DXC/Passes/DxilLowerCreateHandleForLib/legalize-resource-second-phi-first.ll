; RUN: %dxopt %s -hlsl-passes-resume -hlsl-dxil-lower-handle-for-lib -hlsl-dxilemit -S | FileCheck %s

; CHECK: define void @main()
; should be only one createHandle call
; CHECK: %[[CH:[^ ]+]] = call %dx.types.Handle @dx.op.createHandle
; CHECK-NOT: call %dx.types.Handle @dx.op.createHandle
; CHECK-NOT: phi
; CHECK-LABEL: exit:
; CHECK-NOT: phi
; CHECK: %[[AH:[^ ]+]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %[[CH]],
; CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %[[AH]], i32 16,

; Make sure unused resources have been removed from the metadata:
; CHECK: !dx.resources = !{![[RESOURCES:[0-9]+]]}
; CHECK: ![[RESOURCES]] = !{null, ![[UAVS:[0-9]+]], null, null}
; CHECK: ![[UAVS]] = !{![[UAV0:[0-9]+]]}
; Make sure previously unbound u1 is now bound at space=0, u0
; CHECK: ![[UAV0]] = !{i32 0, %"class.RWBuffer<unsigned int>"* undef, !"u1", i32 0, i32 0, i32 1, i32 10, i1 false, i1 false, i1 false,

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%"class.RWBuffer<unsigned int>" = type { i32 }
%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }

@u0 = external global %"class.RWBuffer<unsigned int>", align 4
@u1 = external global %"class.RWBuffer<unsigned int>", align 4
@u2 = external global %"class.RWBuffer<unsigned int>", align 4

; Function Attrs: nounwind
define void @main() #0 {
entry:
  %ld_u0 = load %"class.RWBuffer<unsigned int>", %"class.RWBuffer<unsigned int>"* @u0, align 4
  %ld_u1 = load %"class.RWBuffer<unsigned int>", %"class.RWBuffer<unsigned int>"* @u1, align 4
  %ld_u2 = load %"class.RWBuffer<unsigned int>", %"class.RWBuffer<unsigned int>"* @u2, align 4
  %ch_u1 = call %dx.types.Handle @"dx.op.createHandleForLib.class.RWBuffer<unsigned int>"(i32 160, %"class.RWBuffer<unsigned int>" %ld_u1)
  %ah_u1 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %ch_u1, %dx.types.ResourceProperties { i32 4106, i32 261 })
  %BufferLoad = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %ah_u1, i32 8, i32 undef)
  %ev = extractvalue %dx.types.ResRet.i32 %BufferLoad, 0
  %cmp_ev = icmp eq i32 %ev, 0
  br i1 %cmp_ev, label %case1, label %loopback ; unknown branch

case1:                                        ; preds = loopback, %entry
  ; This phi is resolved second - since phi1 resolves to %ld_u1,
  ; both incoming values are the same, so this resolves to %ld_u1
  %phi0 = phi %"class.RWBuffer<unsigned int>" [ %phi1, %loopback ], [ %ld_u1, %entry ]
  %cmp_case1 = icmp eq i32 1, 1
  br i1 %cmp_case1, label %exit, label %loopback ; always goes to exit

loopback:                                      ; preds = %case1, %entry
  ; This phi must be resolved first - since case1 never goes to loopback,
  ; it can resolve this to %ld_u1
  %phi1 = phi %"class.RWBuffer<unsigned int>" [ %ld_u2, %case1 ], [ %ld_u1, %entry ]
  %cmp_loopback = icmp eq i32 1, 1
  br i1 %cmp_loopback, label %case1, label %exit ; always goes to case1

exit:           ; preds = %case1, %loopback
  ; Resolved third: pred always %case1, %phi0 resolved to %ld_u1,
  ; so %res resolves to %ld_u1
  %res = phi %"class.RWBuffer<unsigned int>" [ %ld_u0, %loopback ], [ %phi0, %case1 ]
  %ch_res = call %dx.types.Handle @"dx.op.createHandleForLib.class.RWBuffer<unsigned int>"(i32 160, %"class.RWBuffer<unsigned int>" %res)
  %ah_res = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %ch_res, %dx.types.ResourceProperties { i32 4106, i32 261 })
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %ah_res, i32 16, i32 undef, i32 %ev, i32 %ev, i32 %ev, i32 %ev, i8 15)
  ret void
}

; Function Attrs: nounwind
declare void @dx.op.bufferStore.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i8) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32, %dx.types.Handle, i32, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.Handle @"dx.op.createHandleForLib.class.RWBuffer<unsigned int>"(i32, %"class.RWBuffer<unsigned int>") #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #2

attributes #0 = { nounwind }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind readnone }

!pauseresume = !{!0}
!llvm.ident = !{!1}
!dx.version = !{!2}
!dx.valver = !{!3}
!dx.shaderModel = !{!4}
!dx.resources = !{!5}
!dx.typeAnnotations = !{!11}
!dx.entryPoints = !{!15}

!0 = !{!"hlsl-dxilemit", !"hlsl-dxilload"}
!1 = !{!"custom IR"}
!2 = !{i32 1, i32 0}
!3 = !{i32 1, i32 10}
!4 = !{!"cs", i32 6, i32 0}
!5 = !{null, !6, null, null}
!6 = !{!7, !9, !10}
!7 = !{i32 0, %"class.RWBuffer<unsigned int>"* @u0, !"u0", i32 -1, i32 -1, i32 1, i32 10, i1 false, i1 false, i1 false, !8}
!8 = !{i32 0, i32 5}
!9 = !{i32 1, %"class.RWBuffer<unsigned int>"* @u1, !"u1", i32 -1, i32 -1, i32 1, i32 10, i1 false, i1 false, i1 false, !8}
!10 = !{i32 2, %"class.RWBuffer<unsigned int>"* @u2, !"u2", i32 -1, i32 -1, i32 1, i32 10, i1 false, i1 false, i1 false, !8}
!11 = !{i32 1, void ()* @main, !12}
!12 = !{!13}
!13 = !{i32 1, !14, !14}
!14 = !{}
!15 = !{void ()* @main, !"main", null, !5, !16}
!16 = !{i32 4, !17}
!17 = !{i32 1, i32 1, i32 1}

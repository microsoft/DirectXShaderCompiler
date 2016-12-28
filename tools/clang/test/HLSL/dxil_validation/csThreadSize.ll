; RUN: %dxv %s | FileCheck %s

; CHECK: Declared Thread Group X size 1025 outside valid range [1..1024]
; CHECK: Declared Thread Group Y size 1025 outside valid range [1..1024]
; CHECK: Declared Thread Group Z size 1025 outside valid range [1..64]
; CHECK: Declared Thread Group Count 1076890625 (X*Y*Z) is beyond the valid maximum of 1024
; CHECK: Total Thread Group Shared Memory storage is 1024000000, exceeded 32768

target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%dx.alignment.legacy.class.RWStructuredBuffer = type { [2 x <2 x float>] }
%dx.alignment.legacy.class.StructuredBuffer = type { %dx.alignment.legacy.struct.mat }
%dx.alignment.legacy.struct.mat = type { [2 x <2 x float>] }
%dx.alignment.legacy.class.StructuredBuffer.0 = type { [2 x <2 x float>] }
%class.RWStructuredBuffer = type { %class.matrix.float.2.2 }
%class.matrix.float.2.2 = type { [2 x <2 x float>] }
%class.StructuredBuffer = type { %struct.mat }
%struct.mat = type { %class.matrix.float.2.2 }
%class.StructuredBuffer.0 = type { %class.matrix.float.2.2 }
%dx.types.Handle = type { i8* }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }

@"\01?dataC@@3PAV?$matrix@M$01$01@@A.v" = addrspace(3) global [64000000 x <4 x float>] undef
@"\01?fA@@3V?$RWStructuredBuffer@V?$matrix@M$01$01@@@@A_legacy" = external global %dx.alignment.legacy.class.RWStructuredBuffer
@"\01?mats@@3V?$StructuredBuffer@Umat@@@@A_legacy" = external global %dx.alignment.legacy.class.StructuredBuffer
@"\01?mats2@@3V?$StructuredBuffer@V?$matrix@M$01$01@@@@A_legacy" = external global %dx.alignment.legacy.class.StructuredBuffer.0
@dx.typevar.0 = external addrspace(1) constant %class.RWStructuredBuffer
@dx.typevar.1 = external addrspace(1) constant %class.StructuredBuffer
@dx.typevar.2 = external addrspace(1) constant %struct.mat
@dx.typevar.3 = external addrspace(1) constant %class.StructuredBuffer.0
@dx.typevar.4 = external addrspace(1) constant %dx.alignment.legacy.class.RWStructuredBuffer
@dx.typevar.5 = external addrspace(1) constant %dx.alignment.legacy.struct.mat
@dx.typevar.6 = external addrspace(1) constant %dx.alignment.legacy.class.StructuredBuffer
@dx.typevar.7 = external addrspace(1) constant %dx.alignment.legacy.class.StructuredBuffer.0
@llvm.used = appending global [11 x i8*] [i8* bitcast (%dx.alignment.legacy.class.RWStructuredBuffer* @"\01?fA@@3V?$RWStructuredBuffer@V?$matrix@M$01$01@@@@A_legacy" to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.StructuredBuffer.0 addrspace(1)* @dx.typevar.3 to i8 addrspace(1)*) to i8*), i8* bitcast (%dx.alignment.legacy.class.StructuredBuffer* @"\01?mats@@3V?$StructuredBuffer@Umat@@@@A_legacy" to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%dx.alignment.legacy.class.StructuredBuffer addrspace(1)* @dx.typevar.6 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.RWStructuredBuffer addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%dx.alignment.legacy.class.RWStructuredBuffer addrspace(1)* @dx.typevar.4 to i8 addrspace(1)*) to i8*), i8* bitcast (%dx.alignment.legacy.class.StructuredBuffer.0* @"\01?mats2@@3V?$StructuredBuffer@V?$matrix@M$01$01@@@@A_legacy" to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast 
(%struct.mat addrspace(1)* @dx.typevar.2 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.StructuredBuffer addrspace(1)* @dx.typevar.1 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%dx.alignment.legacy.class.StructuredBuffer.0 addrspace(1)* @dx.typevar.7 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%dx.alignment.legacy.struct.mat addrspace(1)* @dx.typevar.5 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: alwaysinline nounwind
define void @main(<2 x i32> %tid, <2 x i32> %gid, <2 x i32> %gtid, i32 %gidx) #0 {
entry:
  %fA_UAV_structbuf = tail call %dx.types.Handle @dx.op.createHandle(i32 58, i8 1, i32 0, i32 0, i1 false)
  %mats2_texture_structbuf = tail call %dx.types.Handle @dx.op.createHandle(i32 58, i8 0, i32 1, i32 0, i1 false)
  %mats_texture_structbuf = tail call %dx.types.Handle @dx.op.createHandle(i32 58, i8 0, i32 0, i32 0, i1 false)
  %0 = tail call i32 @dx.op.threadId.i32(i32 93, i32 0)
  %1 = tail call i32 @dx.op.threadId.i32(i32 93, i32 1)
  %2 = tail call i32 @dx.op.groupId.i32(i32 94, i32 0)
  %3 = tail call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 1)
  %4 = tail call i32 @dx.op.flattenedThreadIdInGroup.i32(i32 96)
  %rem = and i32 %0, 63
  %5 = getelementptr inbounds [64000000 x <4 x float>], [64000000 x <4 x float>] addrspace(3)* @"\01?dataC@@3PAV?$matrix@M$01$01@@A.v", i32 0, i32 %rem, i32 0
  %6 = getelementptr inbounds [64000000 x <4 x float>], [64000000 x <4 x float>] addrspace(3)* @"\01?dataC@@3PAV?$matrix@M$01$01@@A.v", i32 0, i32 %rem, i32 1
  %7 = getelementptr inbounds [64000000 x <4 x float>], [64000000 x <4 x float>] addrspace(3)* @"\01?dataC@@3PAV?$matrix@M$01$01@@A.v", i32 0, i32 %rem, i32 2
  %8 = getelementptr inbounds [64000000 x <4 x float>], [64000000 x <4 x float>] addrspace(3)* @"\01?dataC@@3PAV?$matrix@M$01$01@@A.v", i32 0, i32 %rem, i32 3
  %BufferLoad = tail call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 69, %dx.types.Handle %mats_texture_structbuf, i32 %2, i32 0)
  %9 = extractvalue %dx.types.ResRet.f32 %BufferLoad, 0
  %10 = extractvalue %dx.types.ResRet.f32 %BufferLoad, 1
  %11 = extractvalue %dx.types.ResRet.f32 %BufferLoad, 2
  %12 = extractvalue %dx.types.ResRet.f32 %BufferLoad, 3
  %BufferLoad7 = tail call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 69, %dx.types.Handle %mats2_texture_structbuf, i32 %3, i32 0)
  %13 = extractvalue %dx.types.ResRet.f32 %BufferLoad7, 0
  %14 = extractvalue %dx.types.ResRet.f32 %BufferLoad7, 1
  %15 = extractvalue %dx.types.ResRet.f32 %BufferLoad7, 2
  %16 = extractvalue %dx.types.ResRet.f32 %BufferLoad7, 3
  %.i0 = fadd fast float %13, %9
  %.i1 = fadd fast float %14, %11
  %.i2 = fadd fast float %15, %10
  %.i3 = fadd fast float %16, %12
  store float %.i0, float addrspace(3)* %5, align 16
  store float %.i1, float addrspace(3)* %6, align 4
  store float %.i2, float addrspace(3)* %7, align 8
  store float %.i3, float addrspace(3)* %8, align 4
  tail call void @dx.op.barrier(i32 83, i32 9)
  %rem3 = and i32 %1, 63
  %sub = xor i32 %rem3, 63
  %17 = getelementptr inbounds [64000000 x <4 x float>], [64000000 x <4 x float>] addrspace(3)* @"\01?dataC@@3PAV?$matrix@M$01$01@@A.v", i32 0, i32 %sub, i32 0
  %18 = getelementptr inbounds [64000000 x <4 x float>], [64000000 x <4 x float>] addrspace(3)* @"\01?dataC@@3PAV?$matrix@M$01$01@@A.v", i32 0, i32 %sub, i32 1
  %19 = getelementptr inbounds [64000000 x <4 x float>], [64000000 x <4 x float>] addrspace(3)* @"\01?dataC@@3PAV?$matrix@M$01$01@@A.v", i32 0, i32 %sub, i32 2
  %20 = getelementptr inbounds [64000000 x <4 x float>], [64000000 x <4 x float>] addrspace(3)* @"\01?dataC@@3PAV?$matrix@M$01$01@@A.v", i32 0, i32 %sub, i32 3
  %21 = load float, float addrspace(3)* %17, align 16
  %22 = load float, float addrspace(3)* %18, align 4
  %23 = load float, float addrspace(3)* %19, align 8
  %24 = load float, float addrspace(3)* %20, align 4
  tail call void @dx.op.bufferStore.f32(i32 70, %dx.types.Handle %fA_UAV_structbuf, i32 %4, i32 0, float %21, float %22, float %23, float %24, i8 15)
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #1

; Function Attrs: nounwind readnone
declare i32 @dx.op.groupId.i32(i32, i32) #1

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadIdInGroup.i32(i32, i32) #1

; Function Attrs: nounwind readnone
declare i32 @dx.op.flattenedThreadIdInGroup.i32(i32) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind
declare void @dx.op.bufferStore.f32(i32, %dx.types.Handle, i32, i32, float, float, float, float, i8) #2

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32, %dx.types.Handle, i32, i32) #3

; Function Attrs: nounwind
declare void @dx.op.barrier(i32, i32) #2

attributes #0 = { alwaysinline nounwind "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-realign-stack" "stack-protector-buffer-size"="0" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind }
attributes #3 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.typeAnnotations = !{!10, !19}
!dx.entryPoints = !{!32}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 5}
!2 = !{!"cs", i32 5, i32 1}
!3 = !{!4, !8, null, null}
!4 = !{!5, !7}
!5 = !{i32 0, %dx.alignment.legacy.class.StructuredBuffer* @"\01?mats@@3V?$StructuredBuffer@Umat@@@@A_legacy", !"mats", i32 0, i32 0, i32 1, i32 12, i32 0, !6}
!6 = !{i32 1, i32 16}
!7 = !{i32 1, %dx.alignment.legacy.class.StructuredBuffer.0* @"\01?mats2@@3V?$StructuredBuffer@V?$matrix@M$01$01@@@@A_legacy", !"mats2", i32 0, i32 1, i32 1, i32 12, i32 0, !6}
!8 = !{!9}
!9 = !{i32 0, %dx.alignment.legacy.class.RWStructuredBuffer* @"\01?fA@@3V?$RWStructuredBuffer@V?$matrix@M$01$01@@@@A_legacy", !"fA", i32 0, i32 0, i32 1, i32 12, i1 false, i1 false, i1 false, !6}
!10 = !{i32 0, %class.RWStructuredBuffer addrspace(1)* @dx.typevar.0, !11, %class.StructuredBuffer addrspace(1)* @dx.typevar.1, !14, %struct.mat addrspace(1)* @dx.typevar.2, !16, %class.StructuredBuffer.0 addrspace(1)* @dx.typevar.3, !11, %dx.alignment.legacy.class.RWStructuredBuffer addrspace(1)* @dx.typevar.4, !11, %dx.alignment.legacy.struct.mat addrspace(1)* @dx.typevar.5, !16, %dx.alignment.legacy.class.StructuredBuffer addrspace(1)* @dx.typevar.6, !14, %dx.alignment.legacy.class.StructuredBuffer.0 addrspace(1)* @dx.typevar.7, !11}
!11 = !{i32 24, !12}
!12 = !{i32 2, !13, i32 3, i32 0, i32 6, !"h", i32 7, i32 9}
!13 = !{i32 2, i32 2, i32 2}
!14 = !{i32 24, !15}
!15 = !{i32 3, i32 0, i32 6, !"h"}
!16 = !{i32 24, !17}
!17 = !{i32 2, !18, i32 3, i32 0, i32 6, !"f2x2", i32 7, i32 9}
!18 = !{i32 2, i32 2, i32 1}
!19 = !{i32 1, void (<2 x i32>, <2 x i32>, <2 x i32>, i32)* @main, !20}
!20 = !{!21, !23, !26, !28, !30}
!21 = !{i32 1, !22, !22}
!22 = !{}
!23 = !{i32 0, !24, !25}
!24 = !{i32 4, !"SV_DispatchThreadID", i32 7, i32 5}
!25 = !{i32 0}
!26 = !{i32 0, !27, !25}
!27 = !{i32 4, !"SV_GroupID", i32 7, i32 5}
!28 = !{i32 0, !29, !25}
!29 = !{i32 4, !"SV_GroupThreadID", i32 7, i32 5}
!30 = !{i32 0, !31, !25}
!31 = !{i32 4, !"SV_GroupIndex", i32 7, i32 5}
!32 = !{void (<2 x i32>, <2 x i32>, <2 x i32>, i32)* @main, !"", null, !3, !33}
!33 = !{i32 0, i64 16, i32 4, !34}
!34 = !{i32 1025, i32 1025, i32 1025}


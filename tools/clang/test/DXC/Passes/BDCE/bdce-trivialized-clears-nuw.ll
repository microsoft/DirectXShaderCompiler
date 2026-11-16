; RUN: %dxopt %s -hlsl-passes-resume -bdce -S | FileCheck %s

; BDCE must drop nsw/nuw/exact from users of a value that it replaces with
; zero, because the flags might only hold for the original value.
;
; Generated with this, then debug info and unused declarations manually
; stripped:
; utils/hct/ExtractIRForPassTest.py -p bdce -o bdce-trivialized-clears-nuw.ll bdce-trivialized-clears-nuw.hlsl -- -T cs_6_6 -E main
;
; Source HLSL:
; RWByteAddressBuffer out_buf : register(u0);
; groupshared uint wg_arr[16];
;
; [numthreads(2, 1, 1)]
; void main(uint lid : SV_GroupIndex) {
;   uint shr = lid >> 2u;
;   uint x = (((shr << 31u) >> 17u) + (shr << 16u)) + 32768u;
;   uint rhs = (shr << 1u) + 1u;
;   uint sub1 = (x << 16u) - rhs;
;   uint sub2 = (x << 15u) - rhs;
;   uint idx = 98302u - ((sub1 % 65536u) + (sub1 % 32768u));
;   wg_arr[idx] = 1u;
;   out_buf.Store(0u, idx);
;   out_buf.Store(4u, (sub2 % 32768u) + (sub2 % 16384u));
; }
;
; %add4 is 'x'. %sub and %sub9 ('sub1' and 'sub2') are only used through masks
; of their low 16 and 15 bits, and shifting %add4 left by 16 and 15 leaves no
; bits of %add4 in those positions. BDCE therefore replaces %add4 with 0. The
; 'nuw' on %sub holds for the original %add4: its bit 15 is always set, so
; %shl7 is always greater than %add6 ('rhs'). It does not hold for 0, because
; %add6 is never 0, so 'sub nuw 0, %add6' is poison. If the flag were kept,
; InstCombine would fold %sub to 0 and %sub12 ('idx') to the wrong constant
; 98302.
;
; BDCE stops at users whose bits are all live (%rem, %rem10, %rem13 and
; %rem14), so %add11.neg and %add15 keep their flags.

; CHECK-LABEL: define void @main(
; CHECK:      %shl7 = shl i32 0, 16
; CHECK-NEXT: %sub = sub i32 %shl7, %add6
; CHECK-NEXT: %shl8 = shl i32 0, 15
; CHECK-NEXT: %sub9 = sub i32 %shl8, %add6
; CHECK:      %add11.neg = sub nsw i32 98302, %rem10
; CHECK:      %add15 = add nuw nsw i32 %rem13, %rem14

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.RWByteAddressBuffer = type { i32 }
%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }

@"\01?out_buf@@3URWByteAddressBuffer@@A" = external global %struct.RWByteAddressBuffer, align 4
@"\01?wg_arr@@3PAIA" = external addrspace(3) global [16 x i32], align 4
@llvm.used = appending global [1 x i8*] [i8* bitcast (%struct.RWByteAddressBuffer* @"\01?out_buf@@3URWByteAddressBuffer@@A" to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @main(i32 %lid) #0 {
entry:
  %0 = load %struct.RWByteAddressBuffer, %struct.RWByteAddressBuffer* @"\01?out_buf@@3URWByteAddressBuffer@@A", align 4
  %1 = call i32 @dx.op.flattenedThreadIdInGroup.i32(i32 96)
  %shr1 = lshr i32 %1, 2
  %2 = shl i32 %shr1, 14
  %shr2 = and i32 %2, 16384
  %shl3 = shl i32 %shr1, 16
  %add = or i32 %shl3, 32768
  %add4 = or i32 %add, %shr2
  %shl5 = shl nuw nsw i32 %shr1, 1
  %add6 = or i32 %shl5, 1
  %shl7 = shl i32 %add4, 16
  %sub = sub nuw i32 %shl7, %add6
  %shl8 = shl i32 %add4, 15
  %sub9 = sub i32 %shl8, %add6
  %rem = and i32 %sub, 65535
  %rem10 = and i32 %sub, 32767
  %add11.neg = sub nsw i32 98302, %rem10
  %rem.neg = sub i32 0, %rem
  %sub12 = add i32 %add11.neg, %rem.neg
  %arrayidx = getelementptr inbounds [16 x i32], [16 x i32] addrspace(3)* @"\01?wg_arr@@3PAIA", i32 0, i32 %sub12
  store i32 1, i32 addrspace(3)* %arrayidx, align 4, !tbaa !17
  %3 = call %dx.types.Handle @dx.op.createHandleForLib.struct.RWByteAddressBuffer(i32 160, %struct.RWByteAddressBuffer %0)
  %4 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %3, %dx.types.ResourceProperties { i32 4107, i32 0 })
  call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %4, i32 0, i32 undef, i32 %sub12, i32 undef, i32 undef, i32 undef, i8 1, i32 4)
  %rem13 = and i32 %sub9, 32767
  %rem14 = and i32 %sub9, 16383
  %add15 = add nuw nsw i32 %rem13, %rem14
  %5 = call %dx.types.Handle @dx.op.createHandleForLib.struct.RWByteAddressBuffer(i32 160, %struct.RWByteAddressBuffer %0)
  %6 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %5, %dx.types.ResourceProperties { i32 4107, i32 0 })
  call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %6, i32 4, i32 undef, i32 %add15, i32 undef, i32 undef, i32 undef, i8 1, i32 4)
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.flattenedThreadIdInGroup.i32(i32) #1

; Function Attrs: nounwind
declare void @dx.op.rawBufferStore.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandleForLib.struct.RWByteAddressBuffer(i32, %struct.RWByteAddressBuffer) #2

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readonly }

!pauseresume = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.typeAnnotations = !{!7}
!dx.entryPoints = !{!14}

!0 = !{!"hlsl-dxilemit", !"hlsl-dxilload"}
!1 = !{i32 1, i32 6}
!2 = !{i32 1, i32 11}
!3 = !{!"cs", i32 6, i32 6}
!4 = !{null, !5, null, null}
!5 = !{!6}
!6 = !{i32 0, %struct.RWByteAddressBuffer* @"\01?out_buf@@3URWByteAddressBuffer@@A", !"out_buf", i32 0, i32 0, i32 1, i32 11, i1 false, i1 false, i1 false, null}
!7 = !{i32 1, void (i32)* @main, !8}
!8 = !{!9, !11}
!9 = !{i32 1, !10, !10}
!10 = !{}
!11 = !{i32 0, !12, !13}
!12 = !{i32 4, !"SV_GroupIndex", i32 7, i32 5}
!13 = !{i32 0}
!14 = !{void (i32)* @main, !"main", null, !4, !15}
!15 = !{i32 4, !16}
!16 = !{i32 2, i32 1, i32 1}
!17 = !{!18, !18, i64 0}
!18 = !{!"int", !19, i64 0}
!19 = !{!"omnipotent char", !20, i64 0}
!20 = !{!"Simple C/C++ TBAA"}

; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResBind = type { i32, i32, i32, i8 }
%dx.types.LinAlgMatrixC4M16N16U0S2 = type { i8* }
%dx.types.LinAlgMatrixC8M1025N1025U2S0 = type { i8* }
%dx.types.LinAlgMatrixC8M16N129U0S0 = type { i8* }
%dx.types.LinAlgMatrixC8M129N16U0S0 = type { i8* }
%dx.types.LinAlgMatrixC8M16N3U0S0 = type { i8* }
%dx.types.LinAlgMatrixC8M3N16U0S0 = type { i8* }
%dx.types.LinAlgMatrixC8M16N129U1S0 = type { i8* }
%dx.types.LinAlgMatrixC8M129N16U1S0 = type { i8* }
%dx.types.LinAlgMatrixC8M16N3U1S0 = type { i8* }
%dx.types.LinAlgMatrixC8M3N16U1S0 = type { i8* }
%dx.types.LinAlgMatrixC8M16N1025U0S2 = type { i8* }
%dx.types.LinAlgMatrixC8M1025N16U0S2 = type { i8* }
%dx.types.LinAlgMatrixC8M16N0U0S2 = type { i8* }
%dx.types.LinAlgMatrixC8M3N16U0S2 = type { i8* }
%dx.types.LinAlgMatrixC8M129N1025U1S2 = type { i8* }
%dx.types.LinAlgMatrixC8M1025N129U1S2 = type { i8* }
%dx.types.LinAlgMatrixC8M129N3U1S2 = type { i8* }
%dx.types.LinAlgMatrixC8M0N129U1S2 = type { i8* }
%dx.types.LinAlgMatrixC8M128N128U0S0 = type { i8* }
%dx.types.LinAlgMatrixC8M128N128U1S0 = type { i8* }
%dx.types.LinAlgMatrixC8M1024N1024U0S2 = type { i8* }
%dx.types.LinAlgMatrixC8M1024N1024U1S2 = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }
%struct.RWByteAddressBuffer = type { i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 0, i32 0, i32 0, i8 1 }, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %handle = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer

  ; Matrix<F16, 1025, 1025, Accumulator, Thread> - its not possible to statically determine which dim is K on Accumulator so no validation occurs
  %mC8M1025N1025U2S0 = call %dx.types.LinAlgMatrixC8M1025N1025U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M1025N1025U2S0(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)

  ; CHECK: Function: main: error: Metadata must be well-formed in operand count and types.
  ; CHECK-NEXT: note: at '%mC4M16N16U0S2
  ; Matrix<I32, 16, 16, A, ThreadGroup> - missing metadata
  %mC4M16N16U0S2 = call %dx.types.LinAlgMatrixC4M16N16U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC4M16N16U0S2(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)

  ; CHECK-NEXT: Function: main: error: Matrix K Dimension out of bounds. K=129 must be >= 4 and <= 128.
  ; CHECK-NEXT: note: at '%mC8M16N129U0S0
  ; Matrix<F16, 129, 16, A, Thread> - N is K so pass
  %mC8M129N16U0S0 = call %dx.types.LinAlgMatrixC8M129N16U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M129N16U0S0(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)
  ; Matrix<F16, 16, 129, A, Thread> - N is K so fail
  %mC8M16N129U0S0 = call %dx.types.LinAlgMatrixC8M16N129U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N129U0S0(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)

  ; CHECK-NEXT: Function: main: error: Matrix K Dimension out of bounds. K=3 must be >= 4 and <= 128.
  ; CHECK-NEXT: note: at '%mC8M16N3U0S0
  ; Matrix<F16, 3, 16, A, Thread> - N is K so pass
  %mC8M3N16U0S0 = call %dx.types.LinAlgMatrixC8M3N16U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M3N16U0S0(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)
  ; Matrix<F16, 16, 3, A, Thread> - N is K so fail
  %mC8M16N3U0S0 = call %dx.types.LinAlgMatrixC8M16N3U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N3U0S0(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)

  ; CHECK-NEXT: Function: main: error: Matrix K Dimension out of bounds. K=129 must be >= 4 and <= 128.
  ; CHECK-NEXT: note: at '%mC8M129N16U1S0
  ; Matrix<F16, 129, 16, B, Thread> - M is K so fail
  %mC8M129N16U1S0 = call %dx.types.LinAlgMatrixC8M129N16U1S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M129N16U1S0(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)
  ; Matrix<F16, 16, 129, B, Thread> - M is K so pass
  %mC8M16N129U1S0 = call %dx.types.LinAlgMatrixC8M16N129U1S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N129U1S0(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)

  ; CHECK-NEXT: Function: main: error: Matrix K Dimension out of bounds. K=3 must be >= 4 and <= 128.
  ; CHECK-NEXT: note: at '%mC8M3N16U1S0
  ; Matrix<F16, 3, 16, B, Thread> - M is K so fail
  %mC8M3N16U1S0 = call %dx.types.LinAlgMatrixC8M3N16U1S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M3N16U1S0(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)
  ; Matrix<F16, 16, 3, B, Thread> - M is K so pass
  %mC8M16N3U1S0 = call %dx.types.LinAlgMatrixC8M16N3U1S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N3U1S0(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)

  ; CHECK-NEXT: Function: main: error: Matrix K Dimension out of bounds. K=1025 must be >= 1 and <= 1024.
  ; CHECK-NEXT: note: at '%mC8M16N1025U0S2
  ; Matrix<F16, 1025, 16, A, ThreadGroup> - N is K so pass
  %mC8M1025N16U0S2 = call %dx.types.LinAlgMatrixC8M1025N16U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M1025N16U0S2(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)
  ; Matrix<F16, 16, 1025, A, ThreadGroup> - N is K so fail
  %mC8M16N1025U0S2 = call %dx.types.LinAlgMatrixC8M16N1025U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N1025U0S2(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)

  ; CHECK-NEXT: Function: main: error: Matrix K Dimension out of bounds. K=0 must be >= 1 and <= 1024.
  ; CHECK-NEXT: note: at '%mC8M16N0U0S2
  ; Matrix<F16, 16, 3, A, ThreadGroup> - N is K so fail
  %mC8M16N0U0S2 = call %dx.types.LinAlgMatrixC8M16N0U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N0U0S2(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)
  ; Matrix<F16, 3, 16, A, ThreadGroup> - N is K so pass
  %mC8M3N16U0S2 = call %dx.types.LinAlgMatrixC8M3N16U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M3N16U0S2(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)

  ; CHECK-NEXT: Function: main: error: Matrix K Dimension out of bounds. K=1025 must be >= 1 and <= 1024.
  ; CHECK-NEXT: note: at '%mC8M1025N129U1S2
  ; Matrix<F16, 1025, 129, B, ThreadGroup> - M is K so fail
  %mC8M1025N129U1S2 = call %dx.types.LinAlgMatrixC8M1025N129U1S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M1025N129U1S2(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)
  ; Matrix<F16, 129, 1025, B, ThreadGroup> - M is K so pass
  %mC8M129N1025U1S2 = call %dx.types.LinAlgMatrixC8M129N1025U1S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M129N1025U1S2(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)

  ; CHECK-NEXT: Function: main: error: Matrix K Dimension out of bounds. K=0 must be >= 1 and <= 1024.
  ; CHECK-NEXT: note: at '%mC8M0N129U1S2
  ; Matrix<F16, 3, 129, B, ThreadGroup> - M is K so fail
  %mC8M0N129U1S2 = call %dx.types.LinAlgMatrixC8M0N129U1S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M0N129U1S2(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)
  ; Matrix<F16, 129, 3, B, ThreadGroup> - M is K so pass
  %mC8M129N3U1S2 = call %dx.types.LinAlgMatrixC8M129N3U1S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M129N3U1S2(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)


  ; Below are just barely in bounds. No validation errors should be emitted.

  ; Matrix<F16, 128, 128, A, Thread>
  %mC8M128N128U0S0 = call %dx.types.LinAlgMatrixC8M128N128U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M128N128U0S0(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)
  ; Matrix<F16, 128, 128, B, Thread>
  %mC8M128N128U1S0 = call %dx.types.LinAlgMatrixC8M128N128U1S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M128N128U1S0(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)
  ; Matrix<F16, 1024, 1024, A, ThreadGroup>
  %mC8M1024N1024U0S2 = call %dx.types.LinAlgMatrixC8M1024N1024U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M1024N1024U0S2(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)
  ; Matrix<F16, 1024, 1024, B, ThreadGroup>
  %mC8M1024N1024U1S2 = call %dx.types.LinAlgMatrixC8M1024N1024U1S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M1024N1024U1S2(i32 -2147483634, %dx.types.Handle %handle, i32 0, i32 0, i32 0, i32 0)

  ; CHECK-NEXT: Validation failed.

  ret void
}

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC4M16N16U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC4M16N16U0S2(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M1025N1025U2S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M1025N1025U2S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M16N129U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N129U0S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M129N16U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M129N16U0S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M16N3U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N3U0S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M3N16U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M3N16U0S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M16N129U1S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N129U1S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M129N16U1S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M129N16U1S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M16N3U1S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N3U1S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M3N16U1S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M3N16U1S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M16N1025U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N1025U0S2(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M1025N16U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M1025N16U0S2(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M16N0U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M16N0U0S2(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M3N16U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M3N16U0S2(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M129N1025U1S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M129N1025U1S2(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M1025N129U1S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M1025N129U1S2(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M129N3U1S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M129N3U1S2(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M0N129U1S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M0N129U1S2(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M128N128U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M128N128U0S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M128N128U1S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M128N128U1S0(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M1024N1024U0S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M1024N1024U0S2(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC8M1024N1024U1S2 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M1024N1024U1S2(i32, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandleFromBinding(i32, %dx.types.ResBind, i32, i1) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!dx.targetTypes = !{!0,!1,!2,!3,!4,!5,!6,!7,!8,!9,!10,!11,!12,!13,!14,!15,!16,!26,!27,!28,!29}
!llvm.ident = !{!17}
!dx.version = !{!18}
!dx.valver = !{!18}
!dx.shaderModel = !{!19}
!dx.resources = !{!20}
!dx.entryPoints = !{!23}

!0 = !{%dx.types.LinAlgMatrixC8M1025N1025U2S0 undef, i32 8, i32 1025, i32 1025, i32 2, i32 0}
!1 = !{%dx.types.LinAlgMatrixC8M16N129U0S0 undef, i32 8, i32 16, i32 129, i32 0, i32 0}
!2 = !{%dx.types.LinAlgMatrixC8M129N16U0S0 undef, i32 8, i32 129, i32 16, i32 0, i32 0}
!3 = !{%dx.types.LinAlgMatrixC8M16N3U0S0 undef, i32 8, i32 16, i32 3, i32 0, i32 0}
!4 = !{%dx.types.LinAlgMatrixC8M3N16U0S0 undef, i32 8, i32 3, i32 16, i32 0, i32 0}
!5 = !{%dx.types.LinAlgMatrixC8M16N129U1S0 undef, i32 8, i32 16, i32 129, i32 1, i32 0}
!6 = !{%dx.types.LinAlgMatrixC8M129N16U1S0 undef, i32 8, i32 129, i32 16, i32 1, i32 0}
!7 = !{%dx.types.LinAlgMatrixC8M16N3U1S0 undef, i32 8, i32 16, i32 3, i32 1, i32 0}
!8 = !{%dx.types.LinAlgMatrixC8M3N16U1S0 undef, i32 8, i32 3, i32 16, i32 1, i32 0}
!9 = !{%dx.types.LinAlgMatrixC8M16N1025U0S2 undef, i32 8, i32 16, i32 1025, i32 0, i32 2}
!10 = !{%dx.types.LinAlgMatrixC8M1025N16U0S2 undef, i32 8, i32 1025, i32 16, i32 0, i32 2}
!11 = !{%dx.types.LinAlgMatrixC8M16N0U0S2 undef, i32 8, i32 16, i32 0, i32 0, i32 2}
!12 = !{%dx.types.LinAlgMatrixC8M3N16U0S2 undef, i32 8, i32 3, i32 16, i32 0, i32 2}
!13 = !{%dx.types.LinAlgMatrixC8M129N1025U1S2 undef, i32 8, i32 129, i32 1025, i32 1, i32 2}
!14 = !{%dx.types.LinAlgMatrixC8M1025N129U1S2 undef, i32 8, i32 1025, i32 129, i32 1, i32 2}
!15 = !{%dx.types.LinAlgMatrixC8M129N3U1S2 undef, i32 8, i32 129, i32 3, i32 1, i32 2}
!16 = !{%dx.types.LinAlgMatrixC8M0N129U1S2 undef, i32 8, i32 0, i32 129, i32 1, i32 2}
!17 = !{!"dxc(private) 1.9.0.15241 (main, 1f63535ae)"}
!18 = !{i32 1, i32 10}
!19 = !{!"cs", i32 6, i32 10}
!20 = !{null, !21, null, null}
!21 = !{!22}
!22 = !{i32 0, %struct.RWByteAddressBuffer* undef, !"", i32 0, i32 0, i32 1, i32 11, i1 false, i1 false, i1 false, null}
!23 = !{void ()* @main, !"main", null, !20, !24}
!24 = !{i32 0, i64 8589934608, i32 4, !25}
!25 = !{i32 4, i32 4, i32 4}
!26 = !{%dx.types.LinAlgMatrixC8M128N128U0S0 undef, i32 8, i32 128, i32 128, i32 0, i32 0}
!27 = !{%dx.types.LinAlgMatrixC8M128N128U1S0 undef, i32 8, i32 128, i32 128, i32 1, i32 0}
!28 = !{%dx.types.LinAlgMatrixC8M1024N1024U0S2 undef, i32 8, i32 1024, i32 1024, i32 0, i32 2}
!29 = !{%dx.types.LinAlgMatrixC8M1024N1024U1S2 undef, i32 8, i32 1024, i32 1024, i32 1, i32 2}

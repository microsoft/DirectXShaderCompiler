; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s

target datalayout = "e-m:e-p:32:32-i1:32-i8:8-i16:16-i32:32-i64:64-f16:16-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResBind = type { i32, i32, i32, i8 }
%dx.types.ResourceProperties = type { i32, i32 }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%struct.ByteAddressBuffer = type { i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind zeroinitializer, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %3 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %2, i32 0, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %4 = extractvalue %dx.types.ResRet.i32 %3, 0

  ; okay
  %5 = call <8 x i32> @dx.op.linAlgConvert.v8i32.v8i32(i32 -2147483618, <8 x i32> <i32 9, i32 8, i32 7, i32 6, i32 5, i32 4, i32 3, i32 2>, i32 4, i32 4)  ; LinAlgConvert(inputVector,inputInterpretation,outputInterpretation)

  ; CHECK: Function: main: error: InputInterpretation of LinAlgConvert must be an immediate constant.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgConvert.v8i32.v8i32
  %6 = call <8 x i32> @dx.op.linAlgConvert.v8i32.v8i32(i32 -2147483618, <8 x i32> <i32 9, i32 8, i32 7, i32 6, i32 5, i32 4, i32 3, i32 2>, i32 %4, i32 4)  ; LinAlgConvert(inputVector,inputInterpretation,outputInterpretation)

  ; CHECK-NEXT: Function: main: error: OutputInterpretation of LinAlgConvert must be an immediate constant.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgConvert.v8i32.v8i32
  %7 = call <8 x i32> @dx.op.linAlgConvert.v8i32.v8i32(i32 -2147483618, <8 x i32> <i32 9, i32 8, i32 7, i32 6, i32 5, i32 4, i32 3, i32 2>, i32 4, i32 %4)  ; LinAlgConvert(inputVector,inputInterpretation,outputInterpretation)

  ; CHECK-NEXT: Function: main: error: Component type 'Invalid' from InputInterpretation not allowed in LinAlg Matrix operations.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgConvert.v8i32.v8i32
  %8 = call <8 x i32> @dx.op.linAlgConvert.v8i32.v8i32(i32 -2147483618, <8 x i32> <i32 9, i32 8, i32 7, i32 6, i32 5, i32 4, i32 3, i32 2>, i32 0, i32 4)  ; LinAlgConvert(inputVector,inputInterpretation,outputInterpretation)

  ; CHECK-NEXT: Function: main: error: Component type 'Invalid' from OutputInterpretation not allowed in LinAlg Matrix operations.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgConvert.v8i32.v8i32
  %9 = call <8 x i32> @dx.op.linAlgConvert.v8i32.v8i32(i32 -2147483618, <8 x i32> <i32 9, i32 8, i32 7, i32 6, i32 5, i32 4, i32 3, i32 2>, i32 4, i32 0)  ; LinAlgConvert(inputVector,inputInterpretation,outputInterpretation)

  ; CHECK-NEXT: Function: main: error: Input vector element type 'float' must match InputInterpretation matrix element type 'I32'.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgConvert.v8i32.v8f32
  %10 = call <8 x i32> @dx.op.linAlgConvert.v8i32.v8f32(i32 -2147483618, <8 x float> <float 9.000000e+00, float 8.000000e+00, float 7.000000e+00, float 6.000000e+00, float 5.000000e+00, float 4.000000e+00, float 3.000000e+00, float 2.000000e+00>, i32 4, i32 4)  ; LinAlgConvert(inputVector,inputInterpretation,outputInterpretation)

  ; CHECK-NEXT: Function: main: error: Output vector element type 'i32' must match OutputInterpretation matrix element type 'F32'.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgConvert.v8i32.v8f32
  %11 = call <8 x i32> @dx.op.linAlgConvert.v8i32.v8f32(i32 -2147483618, <8 x float> <float 9.000000e+00, float 8.000000e+00, float 7.000000e+00, float 6.000000e+00, float 5.000000e+00, float 4.000000e+00, float 3.000000e+00, float 2.000000e+00>, i32 9, i32 9)  ; LinAlgConvert(inputVector,inputInterpretation,outputInterpretation)

  ; CHECK-NEXT: Function: main: error: Input vector element type 'float' must be i32 for InputInterpretation matrix with non-native element type 'F8_E4M3FN'.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgConvert.v32i32.v8f32
  %12 = call <32 x i32> @dx.op.linAlgConvert.v32i32.v8f32(i32 -2147483618, <8 x float> <float 9.000000e+00, float 8.000000e+00, float 7.000000e+00, float 6.000000e+00, float 5.000000e+00, float 4.000000e+00, float 3.000000e+00, float 2.000000e+00>, i32 21, i32 4)  ; LinAlgConvert(inputVector,inputInterpretation,outputInterpretation)

  ; CHECK-NEXT: Function: main: error: Return vector size '32' must match size '2' derived from input vector size and type.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgConvert.v32i32.v8f32
  %13 = call <32 x i32> @dx.op.linAlgConvert.v32i32.v8f32(i32 -2147483618, <8 x float> <float 9.000000e+00, float 8.000000e+00, float 7.000000e+00, float 6.000000e+00, float 5.000000e+00, float 4.000000e+00, float 3.000000e+00, float 2.000000e+00>, i32 9, i32 21)  ; LinAlgConvert(inputVector,inputInterpretation,outputInterpretation)

  ; CHECK-NEXT: Function: main: error: Output vector element type 'float' must be i32 for OutputInterpretation matrix with non-native element type 'F8_E4M3FN'.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgConvert.v2f32.v8f32
  %14 = call <2 x float> @dx.op.linAlgConvert.v2f32.v8f32(i32 -2147483618, <8 x float> <float 9.000000e+00, float 8.000000e+00, float 7.000000e+00, float 6.000000e+00, float 5.000000e+00, float 4.000000e+00, float 3.000000e+00, float 2.000000e+00>, i32 9, i32 21)  ; LinAlgConvert(inputVector,inputInterpretation,outputInterpretation)

  ; CHECK-NEXT: Validation failed.
  ret void
}

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32, %dx.types.Handle, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare <8 x i32> @dx.op.linAlgConvert.v8i32.v8i32(i32, <8 x i32>, i32, i32) #1

; Function Attrs: nounwind
declare <8 x i32> @dx.op.linAlgConvert.v8i32.v8f32(i32, <8 x float>, i32, i32) #1

; Function Attrs: nounwind
declare <32 x i32> @dx.op.linAlgConvert.v32i32.v8f32(i32, <8 x float>, i32, i32) #1

; Function Attrs: nounwind
declare <2 x float> @dx.op.linAlgConvert.v2f32.v8f32(i32, <8 x float>, i32, i32) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #2

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandleFromBinding(i32, %dx.types.ResBind, i32, i1) #2

attributes #0 = { nounwind readonly }
attributes #1 = { nounwind }
attributes #2 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.entryPoints = !{!6}

!0 = !{!"dxc(private) 1.9.0.5484 (linalg-vali-convert, 2832abc07)"}
!1 = !{i32 1, i32 10}
!2 = !{!"cs", i32 6, i32 10}
!3 = !{!4, null, null, null}
!4 = !{!5}
!5 = !{i32 0, %struct.ByteAddressBuffer* undef, !"", i32 0, i32 0, i32 1, i32 11, i32 0, null}
!6 = !{void ()* @main, !"main", null, !3, !7}
!7 = !{i32 0, i64 2199031644176, i32 4, !8}
!8 = !{i32 1, i32 1, i32 1}

; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s

target datalayout = "e-m:e-p:32:32-i1:32-i8:8-i16:16-i32:32-i64:64-f16:16-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.LinAlgMatrixC9M4N4U0S1 = type { i8* }
%dx.types.LinAlgMatrixC9M4N4U0S0 = type { i8* }
%dx.types.LinAlgMatrixC6M4N4U0S2 = type { i8* }
%dx.types.LinAlgMatrixC9M8N8U0S1 = type { i8* }
%dx.types.LinAlgMatrixC9M9N8U0S1 = type { i8* }
%dx.types.LinAlgMatrixC9M8N9U0S1 = type { i8* }

@"\01?SharedArr@@3PAMA" = external addrspace(3) global [64 x float], align 4
@"\01?SharedVecArr@@3PAV?$vector@M$03@@A" = external addrspace(3) global [16 x <4 x float>], align 4

define void @main() {
  ; okay
  %1 = call %dx.types.LinAlgMatrixC9M4N4U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U0S1.f32(i32 -2147483633, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)

  ; CHECK: Function: main: error: Parameter 'Offset' in bytes must be a multiple of 128, got 516 (129 elements * 4 bytes per element).
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U0S1.f32
  %2 = call %dx.types.LinAlgMatrixC9M4N4U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U0S1.f32(i32 -2147483633, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 129, i32 16, i32 0)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)

  ; okay only if offset accounts for byte width of component type
  %3 = call %dx.types.LinAlgMatrixC9M4N4U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U0S1.f32(i32 -2147483633, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 32, i32 16, i32 0)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Parameter 'Stride' in bytes must be a multiple of 16, got 68 (17 elements * 4 bytes per element).
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U0S1.f32
  %4 = call %dx.types.LinAlgMatrixC9M4N4U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U0S1.f32(i32 -2147483633, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 17, i32 0)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)

  ; okay only if stride accounts for byte width of component type
  %5 = call %dx.types.LinAlgMatrixC9M4N4U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U0S1.f32(i32 -2147483633, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 4, i32 0)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Return matrix scope 'Thread' does not match expected scope Wave or ThreadGroup.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U0S0.f32
  %6 = call %dx.types.LinAlgMatrixC9M4N4U0S0 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U0S0.f32(i32 -2147483633, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory inner type 'float' must match return matrix type 'I64'.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromMemory.mC6M4N4U0S2.f32
  %7 = call %dx.types.LinAlgMatrixC6M4N4U0S2 @dx.op.linAlgMatrixLoadFromMemory.mC6M4N4U0S2.f32(i32 -2147483633, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)

  ; okay
  %8 = call %dx.types.LinAlgMatrixC9M8N8U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M8N8U0S1.f32(i32 -2147483633, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory holds '64' scalars but must hold at least '72' scalars.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromMemory.mC9M9N8U0S1.f32
  %9 = call %dx.types.LinAlgMatrixC9M9N8U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M9N8U0S1.f32(i32 -2147483633, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory holds '64' scalars but must hold at least '72' scalars.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromMemory.mC9M8N9U0S1.f32
  %10 = call %dx.types.LinAlgMatrixC9M8N9U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M8N9U0S1.f32(i32 -2147483633, float addrspace(3)* getelementptr inbounds ([64 x float], [64 x float] addrspace(3)* @"\01?SharedArr@@3PAMA", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)

  ; okay
  %11 = call %dx.types.LinAlgMatrixC9M8N8U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M8N8U0S1.v4f32(i32 -2147483633, <4 x float> addrspace(3)* getelementptr inbounds ([16 x <4 x float>], [16 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory holds '64' scalars but must hold at least '72' scalars.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromMemory.mC9M9N8U0S1.v4f32
  %12 = call %dx.types.LinAlgMatrixC9M9N8U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M9N8U0S1.v4f32(i32 -2147483633, <4 x float> addrspace(3)* getelementptr inbounds ([16 x <4 x float>], [16 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)

  ; CHECK-NEXT: Function: main: error: Groupshared memory holds '64' scalars but must hold at least '72' scalars.
  ; CHECK-NEXT: note: at {{.*}} @dx.op.linAlgMatrixLoadFromMemory.mC9M8N9U0S1.v4f32
  %13 = call %dx.types.LinAlgMatrixC9M8N9U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M8N9U0S1.v4f32(i32 -2147483633, <4 x float> addrspace(3)* getelementptr inbounds ([16 x <4 x float>], [16 x <4 x float>] addrspace(3)* @"\01?SharedVecArr@@3PAV?$vector@M$03@@A", i32 0, i32 0), i32 128, i32 16, i32 0)  ; LinAlgMatrixLoadFromMemory(memory,offset,stride,layout)

  ; CHECK-NEXT: Validation failed.
  ret void
}

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M4N4U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U0S1.f32(i32, float addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M4N4U0S0 @dx.op.linAlgMatrixLoadFromMemory.mC9M4N4U0S0.f32(i32, float addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC6M4N4U0S2 @dx.op.linAlgMatrixLoadFromMemory.mC6M4N4U0S2.f32(i32, float addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M8N8U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M8N8U0S1.f32(i32, float addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M9N8U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M9N8U0S1.f32(i32, float addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M8N9U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M8N9U0S1.f32(i32, float addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M8N8U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M8N8U0S1.v4f32(i32, <4 x float> addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M9N8U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M9N8U0S1.v4f32(i32, <4 x float> addrspace(3)*, i32, i32, i32) #0

; Function Attrs: nounwind
declare %dx.types.LinAlgMatrixC9M8N9U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M8N9U0S1.v4f32(i32, <4 x float> addrspace(3)*, i32, i32, i32) #0

attributes #0 = { nounwind }

!dx.targetTypes = !{!0, !1, !2, !3, !4, !5}
!llvm.ident = !{!6}
!dx.version = !{!7}
!dx.valver = !{!7}
!dx.shaderModel = !{!8}
!dx.entryPoints = !{!9}

!0 = !{%dx.types.LinAlgMatrixC9M4N4U0S0 undef, i32 9, i32 4, i32 4, i32 0, i32 0}
!1 = !{%dx.types.LinAlgMatrixC9M4N4U0S1 undef, i32 9, i32 4, i32 4, i32 0, i32 1}
!2 = !{%dx.types.LinAlgMatrixC6M4N4U0S2 undef, i32 6, i32 4, i32 4, i32 0, i32 2}
!3 = !{%dx.types.LinAlgMatrixC9M8N8U0S1 undef, i32 9, i32 8, i32 8, i32 0, i32 1}
!4 = !{%dx.types.LinAlgMatrixC9M9N8U0S1 undef, i32 9, i32 9, i32 8, i32 0, i32 1}
!5 = !{%dx.types.LinAlgMatrixC9M8N9U0S1 undef, i32 9, i32 8, i32 9, i32 0, i32 1}
!6 = !{!"dxc(private) 1.9.0.5493 (linalg-vali-convert, 0787c0d7b-dirty)"}
!7 = !{i32 1, i32 10}
!8 = !{!"cs", i32 6, i32 10}
!9 = !{void ()* @main, !"main", null, null, !10}
!10 = !{i32 0, i64 2199031644160, i32 4, !11}
!11 = !{i32 1, i32 1, i32 1}

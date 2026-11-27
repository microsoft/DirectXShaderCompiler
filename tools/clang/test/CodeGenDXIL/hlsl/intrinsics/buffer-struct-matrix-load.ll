; RUN: %dxopt %s -hlsl-passes-resume -dxilgen -S | FileCheck %s

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.ByteAddressBuffer  = type { i32 }
%struct.RWByteAddressBuffer = type { i32 }
%struct.Box                = type { %class.matrix.float.4.4 }
%class.matrix.float.4.4   = type { [4 x <4 x float>] }
%dx.types.Handle           = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }

@"\01?src@@3UByteAddressBuffer@@A"   = external global %struct.ByteAddressBuffer,   align 4
@"\01?dst@@3URWByteAddressBuffer@@A" = external global %struct.RWByteAddressBuffer, align 4

; Function Attrs: nounwind
define void @main() #0 {
  %src = load %struct.ByteAddressBuffer, %struct.ByteAddressBuffer* @"\01?src@@3UByteAddressBuffer@@A"

  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @dx.op.createHandleForLib.struct.ByteAddressBuffer(i32 160, %struct.ByteAddressBuffer
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 11, i32 0 })
  ; Column 0 of a column-major float4x4: elements at byte offsets 0, 4, 8, 12.
  ; The element-offset operand (4th arg) must be undef for raw-buffer loads.
  ; CHECK: @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 0,  i32 undef, i8 1, i32 4)
  ; CHECK: @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 4,  i32 undef, i8 1, i32 4)
  ; CHECK: @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 8,  i32 undef, i8 1, i32 4)
  ; CHECK: @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 12, i32 undef, i8 1, i32 4)

  %src.hdl = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.ByteAddressBuffer)"(i32 0, %struct.ByteAddressBuffer %src)
  %src.ann = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.ByteAddressBuffer)"(i32 14, %dx.types.Handle %src.hdl, %dx.types.ResourceProperties { i32 11, i32 0 }, %struct.ByteAddressBuffer zeroinitializer)

  ; Load<Box>(0): returns a virtual pointer at raw-buffer byte offset 0.
  %box.ptr = call %struct.Box* @"dx.hl.op.ro.%struct.Box* (i32, %dx.types.Handle, i32)"(i32 231, %dx.types.Handle %src.ann, i32 0)

  ; GEP to field 0 (the matrix m) inside Box -- byte offset within the struct = 0.
  %mat.ptr = getelementptr inbounds %struct.Box, %struct.Box* %box.ptr, i32 0, i32 0

  ; ColMatSubscript (opcode 1): extract rows 0-3 of column 0 from the 4x4 matrix.
  ; Row indices 0-3 map to flat element indices 0-3 (column-major storage),
  ; i.e. byte offsets 0, 4, 8, 12 relative to the matrix base.
  %col0.ptr = call <4 x float>* @"dx.hl.subscript.colMajor[].rn.<4 x float>* (i32, %class.matrix.float.4.4*, i32, i32, i32, i32)"(i32 1, %class.matrix.float.4.4* %mat.ptr, i32 0, i32 1, i32 2, i32 3)
  %col0 = load <4 x float>, <4 x float>* %col0.ptr

  ; Write the result to a UAV so the loads have an observable use.
  %dst = load %struct.RWByteAddressBuffer, %struct.RWByteAddressBuffer* @"\01?dst@@3URWByteAddressBuffer@@A"
  %dst.hdl = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RWByteAddressBuffer)"(i32 0, %struct.RWByteAddressBuffer %dst)
  %dst.ann = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer)"(i32 14, %dx.types.Handle %dst.hdl, %dx.types.ResourceProperties { i32 4107, i32 0 }, %struct.RWByteAddressBuffer zeroinitializer)
  %col0.x = extractelement <4 x float> %col0, i32 0
  %col0.y = extractelement <4 x float> %col0, i32 1
  %col0.z = extractelement <4 x float> %col0, i32 2
  %col0.w = extractelement <4 x float> %col0, i32 3
  call void @"dx.hl.op..void (i32, %dx.types.Handle, i32, float)"(i32 277, %dx.types.Handle %dst.ann, i32 0,  float %col0.x)
  call void @"dx.hl.op..void (i32, %dx.types.Handle, i32, float)"(i32 277, %dx.types.Handle %dst.ann, i32 4,  float %col0.y)
  call void @"dx.hl.op..void (i32, %dx.types.Handle, i32, float)"(i32 277, %dx.types.Handle %dst.ann, i32 8,  float %col0.z)
  call void @"dx.hl.op..void (i32, %dx.types.Handle, i32, float)"(i32 277, %dx.types.Handle %dst.ann, i32 12, float %col0.w)

  ret void
}

declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.ByteAddressBuffer)"(i32, %struct.ByteAddressBuffer) #1
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.ByteAddressBuffer)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.ByteAddressBuffer) #1
declare %struct.Box* @"dx.hl.op.ro.%struct.Box* (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #2
declare <4 x float>* @"dx.hl.subscript.colMajor[].rn.<4 x float>* (i32, %class.matrix.float.4.4*, i32, i32, i32, i32)"(i32, %class.matrix.float.4.4*, i32, i32, i32, i32) #1
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RWByteAddressBuffer)"(i32, %struct.RWByteAddressBuffer) #1
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer) #1
declare void @"dx.hl.op..void (i32, %dx.types.Handle, i32, float)"(i32, %dx.types.Handle, i32, float) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readonly }

!pauseresume        = !{!1}
!dx.version         = !{!0}
!dx.valver          = !{!2}
!dx.shaderModel     = !{!3}
!dx.typeAnnotations = !{!4}
!dx.entryPoints     = !{!8}
!dx.fnprops         = !{!15}
!dx.options         = !{!16, !17}

!0  = !{i32 1, i32 6}
!1  = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!2  = !{i32 1, i32 9}
!3  = !{!"cs", i32 6, i32 6}
; dx.typeAnnotations: function annotations only (no user struct needed for this pass).
!4  = !{i32 1, void ()* @main, !5}
!5  = !{!6}
!6  = !{i32 1, !7, !7}
!7  = !{}
; dx.entryPoints: entry = main, no signatures, resources = {SRV=!10, UAV=!12}.
!8  = !{void ()* @main, !"main", null, !9, null}
!9  = !{!10, !12, null, null}
!10 = !{!11}
!11 = !{i32 0, %struct.ByteAddressBuffer* @"\01?src@@3UByteAddressBuffer@@A", !"src", i32 0, i32 0, i32 1, i32 11, i32 0, null}
!12 = !{!13}
!13 = !{i32 0, %struct.RWByteAddressBuffer* @"\01?dst@@3URWByteAddressBuffer@@A", !"dst", i32 0, i32 0, i32 1, i32 11, i1 false, i1 false, i1 false, null}
; dx.fnprops: CS kind=5, numthreads(1,1,1).
!15 = !{void ()* @main, i32 5, i32 1, i32 1, i32 1}
!16 = !{i32 64}
!17 = !{i32 -1}

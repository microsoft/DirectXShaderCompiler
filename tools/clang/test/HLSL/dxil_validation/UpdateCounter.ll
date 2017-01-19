; RUN: %dxv %s | FileCheck %s

; CHECK: BufferUpdateCounter valid only on UAV
; CHECK: BufferUpdateCounter valid only on structured buffers
; CHECK: inc of BufferUpdateCounter must be an immediate constant
; CHECK: RWStructuredBuffers may increment or decrement their counters, but not both.


target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%class.Buffer = type { <2 x float> }
%class.RWStructuredBuffer = type { %struct.Foo }
%struct.Foo = type { <2 x float>, <3 x float>, [4 x <2 x i32>] }
%dx.types.Handle = type { i8* }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }

@"\01?buf1@@3V?$Buffer@V?$vector@M$01@@@@A" = available_externally global %class.Buffer zeroinitializer, align 4
@"\01?buf2@@3V?$RWStructuredBuffer@UFoo@@@@A" = available_externally global %class.RWStructuredBuffer zeroinitializer, align 4
@dx.typevar.0 = external addrspace(1) constant %class.Buffer
@dx.typevar.1 = external addrspace(1) constant %class.RWStructuredBuffer
@dx.typevar.2 = external addrspace(1) constant %struct.Foo
@llvm.used = appending global [7 x i8*] [i8* bitcast (%class.RWStructuredBuffer* @"\01?buf2@@3V?$RWStructuredBuffer@UFoo@@@@A" to i8*), i8* bitcast (%class.Buffer* @"\01?buf1@@3V?$Buffer@V?$vector@M$01@@@@A" to i8*), i8* bitcast (%class.Buffer* @"\01?buf1@@3V?$Buffer@V?$vector@M$01@@@@A" to i8*), i8* bitcast (%class.RWStructuredBuffer* @"\01?buf2@@3V?$RWStructuredBuffer@UFoo@@@@A" to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.Buffer addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.RWStructuredBuffer addrspace(1)* @dx.typevar.1 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.Foo addrspace(1)* @dx.typevar.2 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @main.flat(float, float, <4 x float>* nocapture readnone) #0 {
entry:
  %buf2_UAV_structbuf = call %dx.types.Handle @dx.op.createHandle(i32 58, i8 1, i32 0, i32 0, i1 false)
  %buf1_texture_buf = call %dx.types.Handle @dx.op.createHandle(i32 58, i8 0, i32 0, i32 0, i1 false)
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)
  %4 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %5 = call i32 @dx.op.bufferUpdateCounter(i32 71, %dx.types.Handle %buf2_UAV_structbuf, i8 1)
  call void @dx.op.bufferStore.f32(i32 70, %dx.types.Handle %buf2_UAV_structbuf, i32 %5, i32 0, float %4, float %3, float undef, float undef, i8 3)
  %6 = call i32 @dx.op.bufferUpdateCounter(i32 71, %dx.types.Handle %buf2_UAV_structbuf, i8 -1)
  %BufferLoad1 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 69, %dx.types.Handle %buf2_UAV_structbuf, i32 %6, i32 0)
  %7 = extractvalue %dx.types.ResRet.f32 %BufferLoad1, 0
  %8 = extractvalue %dx.types.ResRet.f32 %BufferLoad1, 1
  %9 = call i32 @dx.op.bufferUpdateCounter(i32 71, %dx.types.Handle %buf1_texture_buf, i8 undef)
  %BufferLoad = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 69, %dx.types.Handle %buf1_texture_buf, i32 %6, i32 undef)
  %10 = extractvalue %dx.types.ResRet.f32 %BufferLoad, 0
  %11 = extractvalue %dx.types.ResRet.f32 %BufferLoad, 1
  %add.i0 = fadd fast float %10, %7
  %add.i1 = fadd fast float %11, %8
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %add.i0)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %add.i1)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float 0.000000e+00)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float 0.000000e+00)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind
declare i32 @dx.op.bufferUpdateCounter(i32, %dx.types.Handle, i8) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32, %dx.types.Handle, i32, i32) #2

; Function Attrs: nounwind
declare void @dx.op.bufferStore.f32(i32, %dx.types.Handle, i32, i32, float, float, float, float, i8) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.typeAnnotations = !{!10, !19}
!dx.entryPoints = !{!32}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 7}
!2 = !{!"ps", i32 6, i32 0}
!3 = !{!4, !7, null, null}
!4 = !{!5}
!5 = !{i32 0, %class.Buffer* @"\01?buf1@@3V?$Buffer@V?$vector@M$01@@@@A", !"buf1", i32 0, i32 0, i32 1, i32 10, i32 0, !6}
!6 = !{i32 0, i32 9}
!7 = !{!8}
!8 = !{i32 0, %class.RWStructuredBuffer* @"\01?buf2@@3V?$RWStructuredBuffer@UFoo@@@@A", !"buf2", i32 0, i32 0, i32 1, i32 12, i1 false, i1 false, i1 false, !9}
!9 = !{i32 1, i32 52}
!10 = !{i32 0, %class.Buffer addrspace(1)* @dx.typevar.0, !11, %class.RWStructuredBuffer addrspace(1)* @dx.typevar.1, !13, %struct.Foo addrspace(1)* @dx.typevar.2, !15}
!11 = !{i32 8, !12}
!12 = !{i32 3, i32 0, i32 6, !"h", i32 7, i32 9}
!13 = !{i32 88, !14}
!14 = !{i32 3, i32 0, i32 6, !"h"}
!15 = !{i32 88, !16, !17, !18}
!16 = !{i32 3, i32 0, i32 6, !"a", i32 7, i32 9}
!17 = !{i32 3, i32 16, i32 6, !"b", i32 7, i32 9}
!18 = !{i32 3, i32 32, i32 6, !"c", i32 7, i32 4}
!19 = !{i32 1, void (float, float, <4 x float>*)* @main.flat, !20}
!20 = !{!21, !23, !26, !29}
!21 = !{i32 0, !22, !22}
!22 = !{}
!23 = !{i32 0, !24, !25}
!24 = !{i32 4, !"Idx1", i32 7, i32 9}
!25 = !{i32 1}
!26 = !{i32 0, !27, !28}
!27 = !{i32 4, !"Idx2", i32 7, i32 9}
!28 = !{i32 2}
!29 = !{i32 1, !30, !31}
!30 = !{i32 4, !"SV_Target", i32 7, i32 9}
!31 = !{i32 0}
!32 = !{void (float, float, <4 x float>*)* @main.flat, !"", !33, !3, !39}
!33 = !{!34, !37, null}
!34 = !{!35, !36}
!35 = !{i32 0, !"Idx", i8 9, i8 0, !25, i8 2, i32 1, i8 1, i32 0, i8 0, null}
!36 = !{i32 1, !"Idx", i8 9, i8 0, !28, i8 2, i32 1, i8 1, i32 1, i8 0, null}
!37 = !{!38}
!38 = !{i32 0, !"SV_Target", i8 9, i8 16, !31, i8 0, i32 1, i8 4, i32 0, i8 0, null}
!39 = !{i32 0, i64 8208}

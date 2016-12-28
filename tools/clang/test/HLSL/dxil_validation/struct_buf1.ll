; RUN: %dxv %s | FileCheck %s

; CHECK: globallycoherent cannot be used with append/consume buffers'buf2'
; CHECK:Structured buffer stride should 4 byte align'buf2'
; CHECK:Structured buffer stride should 4 byte align'buf1'
; CHECK: structured buffer require 2 coordinates




target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%class.StructuredBuffer = type { %struct.Foo }
%struct.Foo = type { <2 x float>, <3 x float>, [4 x <2 x i32>] }
%class.RWStructuredBuffer = type { %struct.Foo }
%dx.types.Handle = type { i8* }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }

@"\01?buf1@@3V?$StructuredBuffer@UFoo@@@@A" = available_externally global %class.StructuredBuffer zeroinitializer, align 4
@"\01?buf2@@3V?$RWStructuredBuffer@UFoo@@@@A" = available_externally global %class.RWStructuredBuffer zeroinitializer, align 4
@dx.typevar.0 = external addrspace(1) constant %class.StructuredBuffer
@dx.typevar.1 = external addrspace(1) constant %struct.Foo
@dx.typevar.2 = external addrspace(1) constant %class.RWStructuredBuffer
@llvm.used = appending global [5 x i8*] [i8* bitcast (%class.RWStructuredBuffer* @"\01?buf2@@3V?$RWStructuredBuffer@UFoo@@@@A" to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.StructuredBuffer addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.Foo addrspace(1)* @dx.typevar.1 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.RWStructuredBuffer addrspace(1)* @dx.typevar.2 to i8 addrspace(1)*) to i8*), i8* bitcast (%class.StructuredBuffer* @"\01?buf1@@3V?$StructuredBuffer@UFoo@@@@A" to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @main.flat(float, float, <4 x float>* nocapture readnone) #0 {
entry:
  %buf2_UAV_structbuf = tail call %dx.types.Handle @dx.op.createHandle(i32 58, i8 1, i32 0, i32 0, i1 false)
  %buf1_texture_structbuf = tail call %dx.types.Handle @dx.op.createHandle(i32 58, i8 0, i32 0, i32 0, i1 false)
  %3 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)
  %4 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %conv = fptosi float %4 to i32
  %BufferLoad = tail call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 69, %dx.types.Handle %buf1_texture_structbuf, i32 %conv, i32 0)
  %5 = extractvalue %dx.types.ResRet.f32 %BufferLoad, 0
  %6 = extractvalue %dx.types.ResRet.f32 %BufferLoad, 1
  %BufferLoad1 = tail call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 69, %dx.types.Handle %buf1_texture_structbuf, i32 %conv, i32 undef)
  %7 = extractvalue %dx.types.ResRet.f32 %BufferLoad1, 0
  %8 = extractvalue %dx.types.ResRet.f32 %BufferLoad1, 1
  %9 = extractvalue %dx.types.ResRet.f32 %BufferLoad1, 2
  %add3.i0 = fadd fast float %7, %5
  %add3.i1 = fadd fast float %8, %6
  %conv4 = fptoui float %3 to i32
  %10 = shl i32 %conv4, 3
  %11 = add i32 %10, 20
  %BufferLoad2 = tail call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 69, %dx.types.Handle %buf1_texture_structbuf, i32 %conv, i32 %11)
  %12 = extractvalue %dx.types.ResRet.i32 %BufferLoad2, 0
  %13 = extractvalue %dx.types.ResRet.i32 %BufferLoad2, 1
  %conv7.i0 = sitofp i32 %12 to float
  %conv7.i1 = sitofp i32 %13 to float
  %add8.i1 = fadd fast float %add3.i1, %conv7.i1
  %conv9 = fptosi float %3 to i32
  %BufferLoad3 = tail call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 69, %dx.types.Handle %buf1_texture_structbuf, i32 %conv9, i32 0)
  %14 = extractvalue %dx.types.ResRet.f32 %BufferLoad3, 0
  %15 = extractvalue %dx.types.ResRet.f32 %BufferLoad3, 1
  %add12.i0 = fadd fast float %add3.i0, %14
  %add12.i1 = fadd fast float %add8.i1, %15
  %BufferLoad4 = tail call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 69, %dx.types.Handle %buf1_texture_structbuf, i32 %conv9, i32 8)
  %16 = extractvalue %dx.types.ResRet.f32 %BufferLoad4, 0
  %17 = extractvalue %dx.types.ResRet.f32 %BufferLoad4, 1
  %18 = extractvalue %dx.types.ResRet.f32 %BufferLoad4, 2
  %add18.i0 = fadd fast float %add12.i0, %16
  %add18.i1 = fadd fast float %add12.i1, %17
  %add18.i2 = fadd fast float %18, %9
  %BufferLoad5 = tail call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 69, %dx.types.Handle %buf1_texture_structbuf, i32 %conv9, i32 %11)
  %19 = extractvalue %dx.types.ResRet.i32 %BufferLoad5, 0
  %20 = extractvalue %dx.types.ResRet.i32 %BufferLoad5, 1
  %conv28.i0 = sitofp i32 %19 to float
  %conv28.i1 = sitofp i32 %20 to float
  %add29.i0 = fadd fast float %conv28.i0, %conv7.i0
  %add29.i1 = fadd fast float %add18.i1, %conv28.i1
  %add34 = fadd fast float %4, 2.000000e+02
  %conv35 = fptosi float %add34 to i32
  %BufferLoad6 = tail call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 69, %dx.types.Handle %buf2_UAV_structbuf, i32 %conv35, i32 0)
  %21 = extractvalue %dx.types.ResRet.f32 %BufferLoad6, 0
  %22 = extractvalue %dx.types.ResRet.f32 %BufferLoad6, 1
  %add38.i0 = fadd fast float %add18.i0, %21
  %add38.i1 = fadd fast float %add29.i1, %22
  %BufferLoad7 = tail call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 69, %dx.types.Handle %buf2_UAV_structbuf, i32 %conv35, i32 8)
  %23 = extractvalue %dx.types.ResRet.f32 %BufferLoad7, 0
  %24 = extractvalue %dx.types.ResRet.f32 %BufferLoad7, 1
  %25 = extractvalue %dx.types.ResRet.f32 %BufferLoad7, 2
  %add43.i0 = fadd fast float %add38.i0, %23
  %add43.i1 = fadd fast float %add38.i1, %24
  %add43.i2 = fadd fast float %add18.i2, %25
  %BufferLoad8 = tail call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 69, %dx.types.Handle %buf2_UAV_structbuf, i32 %conv35, i32 %11)
  %26 = extractvalue %dx.types.ResRet.i32 %BufferLoad8, 0
  %27 = extractvalue %dx.types.ResRet.i32 %BufferLoad8, 1
  %conv50.i0 = sitofp i32 %26 to float
  %conv50.i1 = sitofp i32 %27 to float
  %add51.i0 = fadd fast float %add29.i0, %conv50.i0
  %add51.i1 = fadd fast float %add43.i1, %conv50.i1
  %add52 = fadd fast float %3, 2.000000e+02
  %conv53 = fptosi float %add52 to i32
  %BufferLoad9 = tail call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 69, %dx.types.Handle %buf2_UAV_structbuf, i32 %conv53, i32 0)
  %28 = extractvalue %dx.types.ResRet.f32 %BufferLoad9, 0
  %29 = extractvalue %dx.types.ResRet.f32 %BufferLoad9, 1
  %add56.i0 = fadd fast float %add43.i0, %28
  %add56.i1 = fadd fast float %add51.i1, %29
  %BufferLoad10 = tail call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 69, %dx.types.Handle %buf2_UAV_structbuf, i32 %conv53, i32 8)
  %30 = extractvalue %dx.types.ResRet.f32 %BufferLoad10, 0
  %31 = extractvalue %dx.types.ResRet.f32 %BufferLoad10, 1
  %32 = extractvalue %dx.types.ResRet.f32 %BufferLoad10, 2
  %add65.i0 = fadd fast float %add56.i0, %30
  %add65.i1 = fadd fast float %add56.i1, %31
  %add65.i2 = fadd fast float %add43.i2, %32
  %BufferLoad11 = tail call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 69, %dx.types.Handle %buf2_UAV_structbuf, i32 %conv53, i32 %11)
  %33 = extractvalue %dx.types.ResRet.i32 %BufferLoad11, 0
  %34 = extractvalue %dx.types.ResRet.i32 %BufferLoad11, 1
  %conv76.i0 = sitofp i32 %33 to float
  %conv76.i1 = sitofp i32 %34 to float
  %add77.i0 = fadd fast float %add51.i0, %conv76.i0
  %add77.i1 = fadd fast float %add65.i1, %conv76.i1
  %mul = fmul fast float %4, 3.000000e+00
  %conv82 = fptoui float %mul to i32
  tail call void @dx.op.bufferStore.f32(i32 70, %dx.types.Handle %buf2_UAV_structbuf, i32 %conv82, i32 0, float %add65.i0, float %add77.i1, float undef, float undef, i8 3)
  tail call void @dx.op.bufferStore.f32(i32 70, %dx.types.Handle %buf2_UAV_structbuf, i32 %conv82, i32 8, float %add65.i0, float %add77.i1, float %add65.i2, float undef, i8 7)
  %conv89.i0 = fptosi float %add77.i1 to i32
  %conv89.i1 = fptosi float %add77.i0 to i32
  tail call void @dx.op.bufferStore.i32(i32 70, %dx.types.Handle %buf2_UAV_structbuf, i32 %conv82, i32 %11, i32 %conv89.i0, i32 %conv89.i1, i32 undef, i32 undef, i8 3)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %add65.i0)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %add77.i1)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %add65.i2)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %add77.i0)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32, %dx.types.Handle, i32, i32) #2

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32, %dx.types.Handle, i32, i32) #2

; Function Attrs: nounwind
declare void @dx.op.bufferStore.f32(i32, %dx.types.Handle, i32, i32, float, float, float, float, i8) #0

; Function Attrs: nounwind
declare void @dx.op.bufferStore.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i8) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.typeAnnotations = !{!9, !16}
!dx.entryPoints = !{!29}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 7}
!2 = !{!"ps", i32 5, i32 1}
!3 = !{!4, !7, null, null}
!4 = !{!5}
!5 = !{i32 0, %class.StructuredBuffer* @"\01?buf1@@3V?$StructuredBuffer@UFoo@@@@A", !"buf1", i32 0, i32 0, i32 1, i32 12, i32 0, !6}
!6 = !{i32 1, i32 50}
!7 = !{!8}
!8 = !{i32 0, %class.RWStructuredBuffer* @"\01?buf2@@3V?$RWStructuredBuffer@UFoo@@@@A", !"buf2", i32 0, i32 0, i32 1, i32 12, i1 true, i1 true, i1 false, !6}
!9 = !{i32 0, %class.StructuredBuffer addrspace(1)* @dx.typevar.0, !10, %struct.Foo addrspace(1)* @dx.typevar.1, !12, %class.RWStructuredBuffer addrspace(1)* @dx.typevar.2, !10}
!10 = !{i32 88, !11}
!11 = !{i32 3, i32 0, i32 6, !"h"}
!12 = !{i32 88, !13, !14, !15}
!13 = !{i32 3, i32 0, i32 6, !"a", i32 7, i32 9}
!14 = !{i32 3, i32 16, i32 6, !"b", i32 7, i32 9}
!15 = !{i32 3, i32 32, i32 6, !"c", i32 7, i32 4}
!16 = !{i32 1, void (float, float, <4 x float>*)* @main.flat, !17}
!17 = !{!18, !20, !23, !26}
!18 = !{i32 0, !19, !19}
!19 = !{}
!20 = !{i32 0, !21, !22}
!21 = !{i32 4, !"Idx1", i32 7, i32 9}
!22 = !{i32 1}
!23 = !{i32 0, !24, !25}
!24 = !{i32 4, !"Idx2", i32 7, i32 9}
!25 = !{i32 2}
!26 = !{i32 1, !27, !28}
!27 = !{i32 4, !"SV_Target", i32 7, i32 9}
!28 = !{i32 0}
!29 = !{void (float, float, <4 x float>*)* @main.flat, !"", !30, !3, !36}
!30 = !{!31, !34, null}
!31 = !{!32, !33}
!32 = !{i32 0, !"Idx", i8 9, i8 0, !22, i8 2, i32 1, i8 1, i32 0, i8 0, null}
!33 = !{i32 1, !"Idx", i8 9, i8 0, !25, i8 2, i32 1, i8 1, i32 1, i8 0, null}
!34 = !{!35}
!35 = !{i32 0, !"SV_Target", i8 9, i8 16, !28, i8 0, i32 1, i8 4, i32 0, i8 0, null}
!36 = !{i32 0, i64 8208}

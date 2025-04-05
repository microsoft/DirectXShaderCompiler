; RUN: %dxopt %s -hlsl-passes-resume -dxilgen -S | FileCheck %s

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.ByteAddressBuffer = type { i32 }
%struct.RWByteAddressBuffer = type { i32 }
%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }

@"\01?matrix_buffer@@3UByteAddressBuffer@@A" = external global %struct.ByteAddressBuffer, align 4
@"\01?bias_buffer@@3UByteAddressBuffer@@A" = external global %struct.ByteAddressBuffer, align 4
@"\01?rw_matrix_buffer@@3URWByteAddressBuffer@@A" = external global %struct.RWByteAddressBuffer, align 4

; CHECK-LABEL: define void @cs_main()
; Function Attrs: nounwind
define void @cs_main() #0 {
entry:
  %output_vector = alloca <4 x float>, align 4
  %tmp = bitcast <4 x float>* %output_vector to i8*, !dbg !19 ; line:9 col:2
  call void @llvm.lifetime.start(i64 16, i8* %tmp) #0, !dbg !19 ; line:9 col:2
  %tmp1 = load %struct.ByteAddressBuffer, %struct.ByteAddressBuffer* @"\01?matrix_buffer@@3UByteAddressBuffer@@A", !dbg !23 ; line:24 col:2
  %tmp2 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.ByteAddressBuffer)"(i32 0, %struct.ByteAddressBuffer %tmp1), !dbg !23 ; line:24 col:2
  %tmp3 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.ByteAddressBuffer)"(i32 14, %dx.types.Handle %tmp2, %dx.types.ResourceProperties { i32 11, i32 0 }, %struct.ByteAddressBuffer zeroinitializer), !dbg !23 ; line:24 col:2

  ;CHECK: call <4 x float> @dx.op.matVecMul.v4f32.v4f32(i32 305, <4 x float> undef, i1 false, i32 9, %dx.types.Handle %1, i32 0, i32 9, i32 4, i32 4, i32 0, i1 false, i32 64, i1 false)

  call void @"dx.hl.op..void (i32, <4 x float>*, i1, <4 x float>, i1, i32, %dx.types.Handle, i32, i32, i32, i32, i32, i1, i32)"(i32 360, <4 x float>* %output_vector, i1 false, <4 x float> undef, i1 false, i32 9, %dx.types.Handle %tmp3, i32 0, i32 9, i32 4, i32 4, i32 0, i1 false, i32 64), !dbg !23 ; line:24 col:2
  %tmp4 = load %struct.ByteAddressBuffer, %struct.ByteAddressBuffer* @"\01?matrix_buffer@@3UByteAddressBuffer@@A", !dbg !24 ; line:32 col:2
  %tmp5 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.ByteAddressBuffer)"(i32 0, %struct.ByteAddressBuffer %tmp4), !dbg !24 ; line:32 col:2
  %tmp6 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.ByteAddressBuffer)"(i32 14, %dx.types.Handle %tmp5, %dx.types.ResourceProperties { i32 11, i32 0 }, %struct.ByteAddressBuffer zeroinitializer), !dbg !24 ; line:32 col:2
  %tmp7 = load %struct.ByteAddressBuffer, %struct.ByteAddressBuffer* @"\01?bias_buffer@@3UByteAddressBuffer@@A", !dbg !24 ; line:32 col:2
  %tmp8 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.ByteAddressBuffer)"(i32 0, %struct.ByteAddressBuffer %tmp7), !dbg !24 ; line:32 col:2
  %tmp9 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.ByteAddressBuffer)"(i32 14, %dx.types.Handle %tmp8, %dx.types.ResourceProperties { i32 11, i32 0 }, %struct.ByteAddressBuffer zeroinitializer), !dbg !24 ; line:32 col:2

  ;CHECK: call <4 x float> @dx.op.matVecMulAdd.v4f32.v4f32(i32 306, <4 x float> undef, i1 false, i32 9, %dx.types.Handle %4, i32 0, i32 9, i32 4, i32 4, i32 0, i1 false, i32 64, %dx.types.Handle %6, i32 0, i32 9, i1 false)

  call void @"dx.hl.op..void (i32, <4 x float>*, i1, <4 x float>, i1, i32, %dx.types.Handle, i32, i32, i32, i32, i32, i1, i32, %dx.types.Handle, i32, i32)"(i32 361, <4 x float>* %output_vector, i1 false, <4 x float> undef, i1 false, i32 9, %dx.types.Handle %tmp6, i32 0, i32 9, i32 4, i32 4, i32 0, i1 false, i32 64, %dx.types.Handle %tmp9, i32 0, i32 9), !dbg !24 ; line:32 col:2
  %tmp10 = load %struct.RWByteAddressBuffer, %struct.RWByteAddressBuffer* @"\01?rw_matrix_buffer@@3URWByteAddressBuffer@@A", !dbg !25 ; line:45 col:2
  %tmp11 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RWByteAddressBuffer)"(i32 0, %struct.RWByteAddressBuffer %tmp10), !dbg !25 ; line:45 col:2
  %tmp12 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer)"(i32 14, %dx.types.Handle %tmp11, %dx.types.ResourceProperties { i32 4107, i32 0 }, %struct.RWByteAddressBuffer zeroinitializer), !dbg !25 ; line:45 col:2
  ;CHECK: call void @dx.op.outerProductAccumulate.v8i32.v8i32(i32 307, <8 x i32> undef, <8 x i32> undef, %dx.types.Handle %9, i32 0, i32 5, i32 3, i32 64)
  call void @"dx.hl.op..void (i32, <8 x i32>, <8 x i32>, %dx.types.Handle, i32, i32, i32, i32)"(i32 362, <8 x i32> undef, <8 x i32> undef, %dx.types.Handle %tmp12, i32 0, i32 5, i32 3, i32 64), !dbg !25 ; line:45 col:2
  %tmp13 = load %struct.RWByteAddressBuffer, %struct.RWByteAddressBuffer* @"\01?rw_matrix_buffer@@3URWByteAddressBuffer@@A", !dbg !26 ; line:51 col:3
  %tmp14 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RWByteAddressBuffer)"(i32 0, %struct.RWByteAddressBuffer %tmp13), !dbg !26 ; line:51 col:3
  %tmp15 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer)"(i32 14, %dx.types.Handle %tmp14, %dx.types.ResourceProperties { i32 4107, i32 0 }, %struct.RWByteAddressBuffer zeroinitializer), !dbg !26 ; line:51 col:3

  ;CHECK: call void @dx.op.vectorAccumulate.v8i32(i32 308, <8 x i32> undef, %dx.types.Handle %11, i32 0)

  call void @"dx.hl.op..void (i32, <8 x i32>, %dx.types.Handle, i32)"(i32 363, <8 x i32> undef, %dx.types.Handle %tmp15, i32 0), !dbg !26 ; line:51 col:3
  %tmp16 = bitcast <4 x float>* %output_vector to i8*, !dbg !27 ; line:53 col:1
  call void @llvm.lifetime.end(i64 16, i8* %tmp16) #0, !dbg !27 ; line:53 col:1
  ret void, !dbg !27 ; line:53 col:1
}

; Function Attrs: nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #0

; Function Attrs: nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #0

; Function Attrs: nounwind
declare void @"dx.hl.op..void (i32, <4 x float>*, i1, <4 x float>, i1, i32, %dx.types.Handle, i32, i32, i32, i32, i32, i1, i32)"(i32, <4 x float>*, i1, <4 x float>, i1, i32, %dx.types.Handle, i32, i32, i32, i32, i32, i1, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.ByteAddressBuffer)"(i32, %struct.ByteAddressBuffer) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.ByteAddressBuffer)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.ByteAddressBuffer) #1

; Function Attrs: nounwind
declare void @"dx.hl.op..void (i32, <4 x float>*, i1, <4 x float>, i1, i32, %dx.types.Handle, i32, i32, i32, i32, i32, i1, i32, %dx.types.Handle, i32, i32)"(i32, <4 x float>*, i1, <4 x float>, i1, i32, %dx.types.Handle, i32, i32, i32, i32, i32, i1, i32, %dx.types.Handle, i32, i32) #0

; Function Attrs: nounwind
declare void @"dx.hl.op..void (i32, <8 x i32>, <8 x i32>, %dx.types.Handle, i32, i32, i32, i32)"(i32, <8 x i32>, <8 x i32>, %dx.types.Handle, i32, i32, i32, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RWByteAddressBuffer)"(i32, %struct.RWByteAddressBuffer) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer) #1

; Function Attrs: nounwind
declare void @"dx.hl.op..void (i32, <8 x i32>, %dx.types.Handle, i32)"(i32, <8 x i32>, %dx.types.Handle, i32) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.module.flags = !{!0}
!pauseresume = !{!1}
!llvm.ident = !{!2}
!dx.version = !{!3}
!dx.valver = !{!3}
!dx.shaderModel = !{!4}
!dx.typeAnnotations = !{!5}
!dx.entryPoints = !{!9}
!dx.fnprops = !{!16}
!dx.options = !{!17, !18}

!0 = !{i32 2, !"Debug Info Version", i32 3}
!1 = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!2 = !{!"dxc(private) 1.8.0.4884 (anupamac/coop-vec-5, df2877762-dirty)"}
!3 = !{i32 1, i32 9}
!4 = !{!"cs", i32 6, i32 9}
!5 = !{i32 1, void ()* @cs_main, !6}
!6 = !{!7}
!7 = !{i32 1, !8, !8}
!8 = !{}
!9 = !{void ()* @cs_main, !"cs_main", null, !10, null}
!10 = !{!11, !14, null, null}
!11 = !{!12, !13}
!12 = !{i32 0, %struct.ByteAddressBuffer* @"\01?matrix_buffer@@3UByteAddressBuffer@@A", !"matrix_buffer", i32 -1, i32 -1, i32 1, i32 11, i32 0, null}
!13 = !{i32 1, %struct.ByteAddressBuffer* @"\01?bias_buffer@@3UByteAddressBuffer@@A", !"bias_buffer", i32 -1, i32 -1, i32 1, i32 11, i32 0, null}
!14 = !{!15}
!15 = !{i32 0, %struct.RWByteAddressBuffer* @"\01?rw_matrix_buffer@@3URWByteAddressBuffer@@A", !"rw_matrix_buffer", i32 -1, i32 -1, i32 1, i32 11, i1 false, i1 false, i1 false, null}
!16 = !{void ()* @cs_main, i32 5, i32 1, i32 1, i32 1}
!17 = !{i32 -2147483584}
!18 = !{i32 -1}
!19 = !DILocation(line: 9, column: 2, scope: !20)
!20 = !DISubprogram(name: "cs_main", scope: !21, file: !21, line: 7, type: !22, isLocal: false, isDefinition: true, scopeLine: 8, flags: DIFlagPrototyped, isOptimized: false, function: void ()* @cs_main)
!21 = !DIFile(filename: "D:\5Cgit\5Cgithub\5CDirectXShaderCompiler-acFork\5CDirectXShaderCompiler\5Ctools\5Cclang\5Ctest\5CCodeGenDXIL\5Chlsl\5Cintrinsics\5Clinang-builins.hlsl", directory: "")
!22 = !DISubroutineType(types: !8)
!23 = !DILocation(line: 24, column: 2, scope: !20)
!24 = !DILocation(line: 32, column: 2, scope: !20)
!25 = !DILocation(line: 45, column: 2, scope: !20)
!26 = !DILocation(line: 51, column: 3, scope: !20)
!27 = !DILocation(line: 53, column: 1, scope: !20)

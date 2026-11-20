; RUN: %dxopt %s -hlsl-passes-resume -scalarrepl-param-hlsl -S | FileCheck %s
; Test that precise native vector allocas are marked with a vector overload call
; to dx.attribute.precise() and not scalar extracted and re-inserted

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%ConstantBuffer = type opaque

@"$Globals" = external constant %ConstantBuffer

; Function Attrs: nounwind
define <4 x float> @main(<4 x float> %pos, <4 x float> %scale, <4 x float> %shift) #0 {
bb:
  %tmp = alloca <4 x float>, align 4, !dx.temp !10
  %tmp1 = alloca <4 x float>, align 4, !dx.temp !10
  %tmp2 = alloca <4 x float>, align 4, !dx.temp !10
  %position = alloca <4 x float>, align 4, !dx.precise !24
  store <4 x float> %shift, <4 x float>* %tmp, align 4, !tbaa !25
  store <4 x float> %scale, <4 x float>* %tmp1, align 4, !tbaa !25
  store <4 x float> %pos, <4 x float>* %tmp2, align 4, !tbaa !25
  %tmp3 = bitcast <4 x float>* %position to i8* ; line:7 col:3
  call void @llvm.lifetime.start(i64 16, i8* %tmp3) #0 ; line:7 col:3
  %tmp4 = load <4 x float>, <4 x float>* %tmp2, align 4, !tbaa !25 ; line:7 col:29
  ; CHECK: %tmp4 = load <4 x float>, <4 x float>* %tmp2
  ; CHECK-NOT: extractelement
  ; CHECK-NOT: dx.attribute.precise.float
  ; CHECK: call void @"dx.attribute.precise.<4 x float>"(<4 x float> %tmp4)

  store <4 x float> %tmp4, <4 x float>* %position, align 4, !tbaa !25 ; line:7 col:18
  %tmp5 = load <4 x float>, <4 x float>* %position, align 4, !tbaa !25 ; line:11 col:14
  %tmp6 = load <4 x float>, <4 x float>* %tmp1, align 4, !tbaa !25 ; line:11 col:25
  %tmp7 = fmul <4 x float> %tmp5, %tmp6 ; line:11 col:23

  ; CHECK: %tmp7 = fmul <4 x float> %tmp5, %tmp6
  ; CHECK-NOT: extractelement
  ; CHECK-NOT: dx.attribute.precise.float
  ; CHECK: call void @"dx.attribute.precise.<4 x float>"(<4 x float> %tmp7)

  store <4 x float> %tmp7, <4 x float>* %position, align 4, !tbaa !25 ; line:11 col:12
  %tmp8 = load <4 x float>, <4 x float>* %position, align 4 ; line:17 col:14
  %tmp9 = extractelement <4 x float> %tmp8, i32 2 ; line:17 col:14
  %tmp10 = fadd float %tmp9, 0x3F847AE140000000 ; line:17 col:14
  %tmp11 = load <4 x float>, <4 x float>* %position, align 4 ; line:17 col:14
  %tmp12 = getelementptr <4 x float>, <4 x float>* %position, i32 0, i32 2 ; line:17 col:14

  ; CHECK: %tmp12 = getelementptr <4 x float>, <4 x float>* %position, i32 0, i32 2
  ; CHECK-NOT: extractelement
  ; CHECK-NOT: dx.attribute.precise.float
  ; CHECK: call void @dx.attribute.precise.float(float %tmp10)

  store float %tmp10, float* %tmp12 ; line:17 col:14
  %tmp13 = load <4 x float>, <4 x float>* %tmp, align 4, !tbaa !25 ; line:20 col:15
  %tmp14 = load <4 x float>, <4 x float>* %position, align 4, !tbaa !25 ; line:20 col:12
  %tmp15 = fadd <4 x float> %tmp14, %tmp13 ; line:20 col:12

  ; CHECK: %tmp15 = fadd <4 x float> %tmp14, %tmp13
  ; CHECK-NOT: extractelement
  ; CHECK-NOT: dx.attribute.precise.float
  ; CHECK: call void @"dx.attribute.precise.<4 x float>"(<4 x float> %tmp15)

  store <4 x float> %tmp15, <4 x float>* %position, align 4, !tbaa !25 ; line:20 col:12
  %tmp16 = load <4 x float>, <4 x float>* %position, align 4, !tbaa !25 ; line:22 col:10
  %tmp17 = bitcast <4 x float>* %position to i8* ; line:23 col:1
  call void @llvm.lifetime.end(i64 16, i8* %tmp17) #0 ; line:23 col:1
  ret <4 x float> %tmp16 ; line:22 col:3
}

; Function Attrs: nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #0

; Function Attrs: nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #0

attributes #0 = { nounwind }

!pauseresume = !{!1}
!dx.version = !{!3}
!dx.valver = !{!4}
!dx.shaderModel = !{!5}
!dx.typeAnnotations = !{!6}
!dx.entryPoints = !{!17}
!dx.fnprops = !{!21}
!dx.options = !{!22, !23}

!1 = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!3 = !{i32 1, i32 9}
!4 = !{i32 1, i32 10}
!5 = !{!"vs", i32 6, i32 9}
!6 = !{i32 1, <4 x float> (<4 x float>, <4 x float>, <4 x float>)* @main, !7}
!7 = !{!8, !11, !13, !15}
!8 = !{i32 1, !9, !10}
!9 = !{i32 8, i1 true, i32 4, !"SV_Position", i32 7, i32 9, i32 13, i32 4}
!10 = !{}
!11 = !{i32 0, !12, !10}
!12 = !{i32 4, !"POSITION", i32 7, i32 9, i32 13, i32 4}
!13 = !{i32 0, !14, !10}
!14 = !{i32 4, !"SCL", i32 7, i32 9, i32 13, i32 4}
!15 = !{i32 0, !16, !10}
!16 = !{i32 4, !"OFF", i32 7, i32 9, i32 13, i32 4}
!17 = !{<4 x float> (<4 x float>, <4 x float>, <4 x float>)* @main, !"main", null, !18, null}
!18 = !{null, null, !19, null}
!19 = !{!20}
!20 = !{i32 0, %ConstantBuffer* @"$Globals", !"$Globals", i32 0, i32 -1, i32 1, i32 0, null}
!21 = !{<4 x float> (<4 x float>, <4 x float>, <4 x float>)* @main, i32 1}
!22 = !{i32 64}
!23 = !{i32 -1}
!24 = !{i32 1}
!25 = !{!26, !26, i64 0}
!26 = !{!"omnipotent char", !27, i64 0}
!27 = !{!"Simple C/C++ TBAA"}

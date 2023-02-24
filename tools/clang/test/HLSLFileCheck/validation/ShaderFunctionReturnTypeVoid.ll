; RUN: %dxv %s | FileCheck %s

; Test based on IR generated from the following HLSL:
; struct Payload {
;   float f;
; };
; 
; struct Attributes {
;   float2 b;
; };
; 
; struct Param {
;   float f;
; };
; 
; [shader("raygeneration")] void RayGenProto() { return; }
; 
; [shader("anyhit")] void AnyHitProto(inout Payload p, in Attributes a) {
;   p.f += a.b.x;
; }
; 
; [shader("closesthit")] void ClosestHitProto(inout Payload p, in Attributes a) {
;   p.f += a.b.y;
; }
; 
; [shader("miss")] void MissProto(inout Payload p) {
;   p.f += 1.0;
; }
; 
; [shader("callable")] void CallableProto(inout Param p) { p.f += 1.0; }
; 
; export float BadRayGen() {
;   return 1;
; }
; 
; export float BadAnyHit(inout Payload p, in Attributes a) { return p.f; }
; 
; export float BadClosestHit(inout Payload p, in Attributes a) { return p.f; }
; 
; export float BadMiss(inout Payload p) { return p.f; }
; 
; export float BadCallable(inout Param p) { return p.f; }

; CHECK: Shader function 'RayGen' must have void return type
; CHECK: Shader function 'AnyHit' must have void return type
; CHECK: Shader function 'ClosestHit' must have void return type
; CHECK: Shader function 'Miss' must have void return type
; CHECK: Shader function 'Callable' must have void return type


target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.Payload = type { float }
%struct.Attributes = type { <2 x float> }
%struct.Param = type { float }

; Function Attrs: nounwind readnone
define float @RayGen() #0 {
  ret float 1.000000e+00
}

; Function Attrs: nounwind readonly
define float @AnyHit(%struct.Payload* noalias nocapture readonly %p, %struct.Attributes* nocapture readnone %a) #1 {
  ret float 1.000000e+00
}

; Function Attrs: nounwind readonly
define float @ClosestHit(%struct.Payload* noalias nocapture readonly %p, %struct.Attributes* nocapture readnone %a) #1 {
  ret float 1.000000e+00
}

; Function Attrs: nounwind readonly
define float @Miss(%struct.Payload* noalias nocapture readonly %p) #1 {
  ret float 1.000000e+00
}

; Function Attrs: nounwind readonly
define float @Callable(%struct.Param* noalias nocapture readonly %p) #1 {
  ret float 1.000000e+00
}

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind readonly }

!dx.version = !{!0}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.entryPoints = !{!3, !4, !7, !9, !11, !13}

!0 = !{i32 1, i32 3}
!1 = !{i32 1, i32 7}
!2 = !{!"lib", i32 6, i32 3}
!3 = !{null, !"", null, null, null}
!4 = !{float (%struct.Payload*, %struct.Attributes*)* @AnyHit, !"AnyHit", null, null, !5}
!5 = !{i32 8, i32 9, i32 6, i32 4, i32 7, i32 8, i32 5, !6}
!6 = !{i32 0}
!7 = !{float (%struct.Param*)* @Callable, !"Callable", null, null, !8}
!8 = !{i32 8, i32 12, i32 6, i32 4, i32 5, !6}
!9 = !{float (%struct.Payload*, %struct.Attributes*)* @ClosestHit, !"ClosestHit", null, null, !10}
!10 = !{i32 8, i32 10, i32 6, i32 4, i32 7, i32 8, i32 5, !6}
!11 = !{float (%struct.Payload*)* @Miss, !"Miss", null, null, !12}
!12 = !{i32 8, i32 11, i32 6, i32 4, i32 5, !6}
!13 = !{float ()* @RayGen, !"RayGen", null, null, !14}
!14 = !{i32 8, i32 7, i32 5, !6}

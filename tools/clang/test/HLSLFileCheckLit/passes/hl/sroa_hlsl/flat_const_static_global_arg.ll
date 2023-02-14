; RUN: opt -scalarrepl-param-hlsl -S %s | FileCheck %s

; Make sure %class.C* @c is replaced with tmp on the call
; CHECK:  %[[TMP:.+]] = alloca %class.C
; CHECK:  %[[TMP_X_PTR:.+]] = getelementptr inbounds %class.C, %class.C* %[[TMP]], i32 0, i32 0
; CHECK:  %[[C_X:.+]] = load float, float* @c.0
; CHECK:  store float %[[C_X]], float* %[[TMP_X_PTR]]
; CHECK:  %call = call float @"\01?foo@C@@QAAMUS@@@Z"(%class.C* %[[TMP]], %struct.S* %s)
; Make sure no copy out on constant global.
; CHECK-NEXT: ret float %call

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%class.C = type { float }
%ConstantBuffer = type opaque
%struct.S = type { float }

@c = internal constant %class.C zeroinitializer, align 4
@"$Globals" = external constant %ConstantBuffer

; Function Attrs: nounwind
define float @"\01?bar@@YAMXZ"() #0 {
entry:
  %s = alloca %struct.S, align 4
  call void @"\01?foo@@YA?AUS@@XZ"(%struct.S* sret %s)
  %call = call float @"\01?foo@C@@QAAMUS@@@Z"(%class.C* @c, %struct.S* %s)
  ret float %call
}

declare void @"\01?foo@@YA?AUS@@XZ"(%struct.S* sret)

declare float @"\01?foo@C@@QAAMUS@@@Z"(%class.C*, %struct.S*)

attributes #0 = { nounwind }

!llvm.module.flags = !{!0}
!pauseresume = !{!1}
!llvm.ident = !{!2}
!dx.version = !{!3}
!dx.valver = !{!4}
!dx.shaderModel = !{!5}
!dx.typeAnnotations = !{!6, !9}
!dx.entryPoints = !{!19}
!dx.fnprops = !{}
!dx.options = !{!23, !24}

!0 = !{i32 2, !"Debug Info Version", i32 3}
!1 = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!2 = !{!"dxc(private) 1.7.0.3820 (lit_by_default, 316176e78-dirty)"}
!3 = !{i32 1, i32 3}
!4 = !{i32 1, i32 7}
!5 = !{!"lib", i32 6, i32 3}
!6 = !{i32 0, %class.C undef, !7, %struct.S undef, !7}
!7 = !{i32 4, !8}
!8 = !{i32 6, !"a", i32 3, i32 0, i32 7, i32 9}
!9 = !{i32 1, float ()* @"\01?bar@@YAMXZ", !10, void (%struct.S*)* @"\01?foo@@YA?AUS@@XZ", !14, float (%class.C*, %struct.S*)* @"\01?foo@C@@QAAMUS@@@Z", !17}
!10 = !{!11}
!11 = !{i32 1, !12, !13}
!12 = !{i32 7, i32 9}
!13 = !{}
!14 = !{!15, !16}
!15 = !{i32 0, !13, !13}
!16 = !{i32 1, !13, !13}
!17 = !{!11, !18, !18}
!18 = !{i32 2, !13, !13}
!19 = !{null, !"", null, !20, null}
!20 = !{null, null, !21, null}
!21 = !{!22}
!22 = !{i32 0, %ConstantBuffer* @"$Globals", !"$Globals", i32 0, i32 -1, i32 1, i32 0, null}
!23 = !{i32 144}
!24 = !{i32 -1}

; RUN: %opt %s -scalarize-vector-allocas -S | FileCheck %s
; Verify scalar alloca decls
;
; CHECK: %user.addr{{[0-9]+}} = alloca float
; CHECK: %user.addr{{[0-9]+}} = alloca float
; CHECK: %user.addr{{[0-9]+}} = alloca float
; CHECK: %user.addr{{[0-9]+}} = alloca float
; CHECK: %pos.addr{{[0-9]+}} = alloca float
; CHECK: %pos.addr{{[0-9]+}} = alloca float
; CHECK: %pos.addr{{[0-9]+}} = alloca float
; CHECK: %pos.addr{{[0-9]+}} = alloca float
; CHECK: %offset{{[0-9]+}} = alloca i32
; CHECK: %offset{{[0-9]+}} = alloca i32
;
; Verify scalar stores
;
; CHECK: extractelement <4 x float> %{{[0-9]+}}, i64 0
; CHECK-NEXT: store float %{{[0-9]+}}, float* %user.addr{{[0-9]+}}
; CHECK-NEXT: extractelement <4 x float> %{{[0-9]+}}, i64 1
; CHECK-NEXT: store float %{{[0-9]+}}, float* %user.addr{{[0-9]+}}
; CHECK-NEXT: extractelement <4 x float> %{{[0-9]+}}, i64 2
; CHECK-NEXT: store float %{{[0-9]+}}, float* %user.addr{{[0-9]+}}
; CHECK-NEXT: extractelement <4 x float> %{{[0-9]+}}, i64 3
; CHECK-NEXT: store float %{{[0-9]+}}, float* %user.addr{{[0-9]+}}
;
; CHECK: extractelement <4 x float> %{{[0-9]+}}, i64 0
; CHECK-NEXT: store float %{{[0-9]+}}, float* %pos.addr{{[0-9]+}}
; CHECK-NEXT: extractelement <4 x float> %{{[0-9]+}}, i64 1
; CHECK-NEXT: store float %{{[0-9]+}}, float* %pos.addr{{[0-9]+}}
; CHECK-NEXT: extractelement <4 x float> %{{[0-9]+}}, i64 2
; CHECK-NEXT: store float %{{[0-9]+}}, float* %pos.addr{{[0-9]+}}
; CHECK-NEXT: extractelement <4 x float> %{{[0-9]+}}, i64 3
; CHECK-NEXT: store float %{{[0-9]+}}, float* %pos.addr{{[0-9]+}}
;
; CHECK: store i32 0, i32* %offset{{[0-9]+}}
; CHECK-NEXT: store i32 -1, i32* %offset{{[0-9]+}}
;
; Verify scalar loads 
;
; CHECK: load i32, i32* %offset{{[0-9]+}}
; CHECK-NEXT: insertelement <2 x i32> undef, i32 %{{[0-9]+}}, i64 0
; CHECK-NEXT: load i32, i32* %offset{{[0-9]+}}
; CHECK-NEXT: insertelement <2 x i32> %{{[0-9]+}}, i32 %{{[0-9]+}}, i64 1
;
; CHECK: load float, float* %pos.addr{{[0-9]+}}
; CHECK-NEXT: insertelement <4 x float> undef, float %{{[0-9]+}}, i64 0
; CHECK-NEXT: load float, float* %pos.addr{{[0-9]+}}
; CHECK-NEXT: insertelement <4 x float> %{{[0-9]+}}, float %{{[0-9]+}}, i64 1
; CHECK-NEXT: load float, float* %pos.addr{{[0-9]+}}
; CHECK-NEXT: insertelement <4 x float> %{{[0-9]+}}, float %{{[0-9]+}}, i64 2
; CHECK-NEXT: load float, float* %pos.addr{{[0-9]+}}
; CHECK-NEXT: insertelement <4 x float> %{{[0-9]+}}, float %{{[0-9]+}}, i64 3
;
; CHECK: load float, float* %user.addr{{[0-9]+}}
; CHECK-NEXT: insertelement <4 x float> undef, float %{{[0-9]+}}, i64 0
; CHECK-NEXT: load float, float* %user.addr{{[0-9]+}}
; CHECK-NEXT: insertelement <4 x float> %{{[0-9]+}}, float %{{[0-9]+}}, i64 1
; CHECK-NEXT: load float, float* %user.addr{{[0-9]+}}
; CHECK-NEXT: insertelement <4 x float> %{{[0-9]+}}, float %{{[0-9]+}}, i64 2
; CHECK-NEXT: load float, float* %user.addr{{[0-9]+}}
; CHECK-NEXT: insertelement <4 x float> %{{[0-9]+}}, float %{{[0-9]+}}, i64 3
;
;---------------------------HLSL start--------------------------------------------
; Texture2D g_Tex;
; SamplerState g_Sampler;
; void unused() { }
; float4 main(float4 pos : SV_Position, float4 user : USER, bool b : B) : SV_Target {
;   unused();
;   if (b) user = g_Tex.Sample(g_Sampler, pos.xy);
;   return user * pos;
; }
;---------------------------HLSL end--------------------------------------------
;
; ModuleID = 'sample-offset-imm-test01.hlsl'
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f:64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%class.Texture2D = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%struct.SamplerState = type { i32 }
%dx.types.Handle = type { i8* }

@"\01?g_Tex@@3V?$Texture2D@V?$vector@M$03@@@@A" = available_externally global %class.Texture2D zeroinitializer, align 4
@"\01?g_Sampler@@3USamplerState@@A" = available_externally global %struct.SamplerState zeroinitializer, align 4

declare <4 x float> @"dx.hl.op..<4 x float> (i32, %dx.types.Handle, %dx.types.Handle, <2 x float>, float, <2 x i32>)"(i32, %dx.types.Handle, %dx.types.Handle, <2 x float>, float, <2 x i32>)

; Function Attrs: noinline nounwind readnone
define internal %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %class.Texture2D)"(i32, %class.Texture2D) #0 !dx.hl.resource.attribute !2 {
Entry:
  ret %dx.types.Handle undef
}

; Function Attrs: noinline nounwind readnone
define internal %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.SamplerState)"(i32, %struct.SamplerState) #0 !dx.hl.resource.attribute !5 {
Entry:
  ret %dx.types.Handle undef
}

; Function Attrs: nounwind
define void @main(<4 x float>, <4 x float>, i1 zeroext, <4 x float>* noalias) #1 {
entry:
  %b.addr = alloca i32, align 1
  %user.addr = alloca <4 x float>, align 4
  %pos.addr = alloca <4 x float>, align 4
  %offset = alloca <2 x i32>, align 4
  %frombool = zext i1 %2 to i32
  store i32 %frombool, i32* %b.addr, align 1
  store <4 x float> %1, <4 x float>* %user.addr, align 4
  store <4 x float> %0, <4 x float>* %pos.addr, align 4
  store <2 x i32> <i32 0, i32 -1>, <2 x i32>* %offset, align 4
  %4 = load i32, i32* %b.addr, align 1
  %tobool = icmp ne i32 %4, 0
  br i1 %tobool, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %5 = load <2 x i32>, <2 x i32>* %offset, align 4
  %6 = shufflevector <2 x i32> %5, <2 x i32> undef, <2 x i32> <i32 0, i32 1>
  %7 = load <4 x float>, <4 x float>* %pos.addr, align 4
  %8 = shufflevector <4 x float> %7, <4 x float> undef, <2 x i32> <i32 0, i32 1>
  %9 = load %class.Texture2D, %class.Texture2D* @"\01?g_Tex@@3V?$Texture2D@V?$vector@M$03@@@@A"
  %10 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %class.Texture2D)"(i32 0, %class.Texture2D %9)
  %11 = load %struct.SamplerState, %struct.SamplerState* @"\01?g_Sampler@@3USamplerState@@A"
  %12 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.SamplerState)"(i32 0, %struct.SamplerState %11)
  %13 = call <4 x float> @"dx.hl.op..<4 x float> (i32, %dx.types.Handle, %dx.types.Handle, <2 x float>, float, <2 x i32>)"(i32 174, %dx.types.Handle %10, %dx.types.Handle %12, <2 x float> %8, float 0.000000e+00, <2 x i32> %6)
  store <4 x float> %13, <4 x float>* %user.addr, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  %14 = load <4 x float>, <4 x float>* %user.addr, align 4
  %15 = load <4 x float>, <4 x float>* %pos.addr, align 4
  %mul = fmul <4 x float> %14, %15
  store <4 x float> %mul, <4 x float>* %3
  ret void
}

attributes #0 = { noinline nounwind readnone }
attributes #1 = { nounwind "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-realign-stack" "stack-protector-buffer-size"="0" "unsafe-fp-math"="false" "use-soft-float"="false" }

!pauseresume = !{!0}
!llvm.ident = !{!1}

!0 = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!1 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!2 = !{i32 0, !3}
!3 = !{i32 1, %class.Texture2D undef, !"", i32 0, i32 0, i32 0, i32 2, i32 0, !4}
!4 = !{i32 0, i32 9}
!5 = !{i32 3, !6}
!6 = !{i32 -1, %struct.SamplerState undef, !"", i32 0, i32 0, i32 0, i32 0, null}
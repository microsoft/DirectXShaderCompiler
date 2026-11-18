; RUN: %dxopt %s -hlsl-passes-resume -dxil-elim-vector -S | FileCheck %s

; Removing the extract should enqueue and remove the newly dead insertelement
; chain without repeatedly scanning every vector instruction.
define i32 @remove_vector_chain(i32 %a, i32 %b, i32 %c, i32 %d) {
entry:
  %v0 = insertelement <4 x i32> undef, i32 %a, i32 0
  %v1 = insertelement <4 x i32> %v0, i32 %b, i32 1
  %v2 = insertelement <4 x i32> %v1, i32 %c, i32 2
  %v3 = insertelement <4 x i32> %v2, i32 %d, i32 3
  %result = extractelement <4 x i32> %v3, i32 0
  ret i32 %result
}

; CHECK-LABEL: define i32 @remove_vector_chain(
; CHECK-NOT: insertelement
; CHECK-NOT: extractelement
; CHECK: ret i32 %a

; The vector must remain alive until both extracts have been replaced. Once the
; second use is gone, enqueue the chain only once and remove it completely.
define i32 @remove_shared_vector(i32 %a, i32 %b) {
entry:
  %v0 = insertelement <4 x i32> undef, i32 %a, i32 0
  %v1 = insertelement <4 x i32> %v0, i32 %b, i32 1
  %first = extractelement <4 x i32> %v1, i32 0
  %second = extractelement <4 x i32> %v1, i32 1
  %sum = add i32 %first, %second
  ret i32 %sum
}

; CHECK-LABEL: define i32 @remove_shared_vector(
; CHECK-NOT: insertelement
; CHECK-NOT: extractelement
; CHECK: %sum = add i32 %a, %b

; User-empty vector instructions should cascade through the worklist even when
; no successful value simplification starts the deletion.
define void @remove_dead_chain(i32 %value) {
entry:
  %v0 = insertelement <4 x i32> undef, i32 %value, i32 0
  %v1 = insertelement <4 x i32> %v0, i32 %value, i32 1
  ret void
}

; CHECK-LABEL: define void @remove_dead_chain(
; CHECK-NEXT: entry:
; CHECK-NEXT: ret void

; An extract from an unknown vector at a variable index cannot be simplified
; and remains live.
define i32 @preserve_live_extract(<4 x i32> %vector, i32 %index) {
entry:
  %result = extractelement <4 x i32> %vector, i32 %index
  ret i32 %result
}

; CHECK-LABEL: define i32 @preserve_live_extract(
; CHECK: %result = extractelement <4 x i32> %vector, i32 %index
; CHECK: ret i32 %result

; A live insert with a variable index is likewise not removable.
define <4 x i32> @preserve_live_insert(<4 x i32> %vector, i32 %value,
                                      i32 %index) {
entry:
  %result = insertelement <4 x i32> %vector, i32 %value, i32 %index
  ret <4 x i32> %result
}

; CHECK-LABEL: define <4 x i32> @preserve_live_insert(
; CHECK: %result = insertelement <4 x i32> %vector, i32 %value, i32 %index
; CHECK: ret <4 x i32> %result

; Constant vectors are simplified directly by DxilValueCache.
define i32 @extract_constant_vector() {
entry:
  %result = extractelement <4 x i32> <i32 10, i32 20, i32 30, i32 40>, i32 2
  ret i32 %result
}

; CHECK-LABEL: define i32 @extract_constant_vector(
; CHECK-NOT: extractelement
; CHECK: ret i32 30

; Vector allocas are promoted before processing the worklist. The promoted load
; exposes the insertelement chain to normal extraction and cascading deletion.
define i32 @promote_vector_alloca(i32 %a, i32 %b, i32 %c, i32 %d) {
entry:
  %slot = alloca <4 x i32>
  %v0 = insertelement <4 x i32> undef, i32 %a, i32 0
  %v1 = insertelement <4 x i32> %v0, i32 %b, i32 1
  %v2 = insertelement <4 x i32> %v1, i32 %c, i32 2
  %v3 = insertelement <4 x i32> %v2, i32 %d, i32 3
  store <4 x i32> %v3, <4 x i32>* %slot
  %loaded = load <4 x i32>, <4 x i32>* %slot
  %result = extractelement <4 x i32> %loaded, i32 2
  ret i32 %result
}

; CHECK-LABEL: define i32 @promote_vector_alloca(
; CHECK-NOT: alloca
; CHECK-NOT: load
; CHECK-NOT: store
; CHECK-NOT: insertelement
; CHECK-NOT: extractelement
; CHECK: ret i32 %c

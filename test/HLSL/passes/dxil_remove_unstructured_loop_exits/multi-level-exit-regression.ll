; RUN: opt %s -analyze -loops | FileCheck -check-prefix=LOOPBEFORE %s
; RUN: opt %s -dxil-remove-unstructured-loop-exits -o %t.bc

; This is the test case from  ../../../../tools/clang/test/HLSLFileCheck/hlsl/control_flow/loops/multi_level_exit_regression.hlsl
; but just before the attempt to remove unstructured loop exits.
; There used to be a bug where the algorithm did not handle the branch that goes
; directly from: while.body.7.i.backege (in loop at depth 3)
; to:            while.body.i.loopexit (in loop at depth 1)


; LOOPBEFORE:      Loop at depth 1 containing: %while.body.i<header>,%while.body.7.i.preheader.preheader,%while.body.7.i.preheader,%while.body.i.loopexit.5,%if.end.i.preheader,%if.end.i,%if.end.15.i,%if.then.11.i,%while.body.7.i.backedge,%while.body.i.loopexit,%while.body.2.i.loopexit,%if.end.19.i.loopexit,%if.end.19.i,%while.end.22.i,%while.body.i.backedge<latch>
; LOOPBEFORE-NEXT: Loop at depth 2 containing: %while.body.7.i.preheader<header><exiting>,%if.end.i.preheader,%if.end.i,%if.end.15.i,%if.then.11.i,%while.body.7.i.backedge<exiting>,%while.body.2.i.loopexit<latch><exiting>
; LOOPBEFORE-NEXT: Loop at depth 3 containing: %if.end.i<header>,%if.end.15.i<exiting>,%if.then.11.i<exiting>,%while.body.7.i.backedge<latch><exiting>
; no more loops expected
; LOOPBEFORE-NOT:  Loop at depth

; ModuleID = 'standalone.hlsl'
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"


%struct.RWByteAddressBuffer = type { i32 }
%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }

@"\01?g_1@@3URWByteAddressBuffer@@A" = external global %struct.RWByteAddressBuffer, align 4
@llvm.used = appending global [1 x i8*] [i8* bitcast (%struct.RWByteAddressBuffer* @"\01?g_1@@3URWByteAddressBuffer@@A" to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @main() {
entry:
  %0 = load %struct.RWByteAddressBuffer, %struct.RWByteAddressBuffer* @"\01?g_1@@3URWByteAddressBuffer@@A"
  br label %while.body.i

while.body.i.loopexit:                            ; preds = %while.body.7.i.backedge
  br label %while.body.i.backedge

while.body.i.loopexit.5:                          ; preds = %while.body.7.i.preheader
  br label %while.body.i.backedge

while.body.i:                                     ; preds = %while.body.i.backedge, %entry
  %l.i.0 = phi i32 [ 0, %entry ], [ %l.i.0.be, %while.body.i.backedge ]
  %cmp.i = icmp ult i32 %l.i.0, 512
  %cmp3.i.3 = icmp ne i32 %l.i.0, 9
  br i1 %cmp3.i.3, label %while.body.7.i.preheader.preheader, label %if.end.19.i

while.body.7.i.preheader.preheader:               ; preds = %while.body.i
  br label %while.body.7.i.preheader

while.body.2.i.loopexit:                          ; preds = %if.then.11.i, %if.end.15.i
  %cmp3.i = icmp ne i32 %l.i.0, 9
  br i1 %cmp3.i, label %while.body.7.i.preheader, label %if.end.19.i.loopexit

while.body.7.i.preheader:                         ; preds = %while.body.7.i.preheader.preheader, %while.body.2.i.loopexit
  br i1 %cmp.i, label %if.end.i.preheader, label %while.body.i.loopexit.5

if.end.i.preheader:                               ; preds = %while.body.7.i.preheader
  br label %if.end.i

if.end.i:                                         ; preds = %if.end.i.preheader, %while.body.7.i.backedge
  br i1 true, label %if.then.11.i, label %if.end.15.i

if.then.11.i:                                     ; preds = %if.end.i
  br i1 %cmp.i, label %while.body.2.i.loopexit, label %while.body.7.i.backedge

while.body.7.i.backedge:                          ; preds = %if.then.11.i, %if.end.15.i
  br i1 false, label %if.end.i, label %while.body.i.loopexit

if.end.15.i:                                      ; preds = %if.end.i
  br i1 %cmp.i, label %while.body.2.i.loopexit, label %while.body.7.i.backedge

if.end.19.i.loopexit:                             ; preds = %while.body.2.i.loopexit
  br label %if.end.19.i

if.end.19.i:                                      ; preds = %if.end.19.i.loopexit, %while.body.i
  br i1 %cmp.i, label %while.end.22.i, label %while.body.i.backedge

while.end.22.i:                                   ; preds = %if.end.19.i
  %mul.i = mul i32 4, %l.i.0
  %1 = call %dx.types.Handle @dx.op.createHandleForLib.struct.RWByteAddressBuffer(i32 160, %struct.RWByteAddressBuffer %0)
  %2 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4107, i32 0 })
  call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %2, i32 %mul.i, i32 undef, i32 %l.i.0, i32 undef, i32 undef, i32 undef, i8 1, i32 4)
  %add.i = add i32 %l.i.0, 1
  br label %while.body.i.backedge

while.body.i.backedge:                            ; preds = %while.end.22.i, %if.end.19.i, %while.body.i.loopexit, %while.body.i.loopexit.5
  %l.i.0.be = phi i32 [ %add.i, %while.end.22.i ], [ 0, %if.end.19.i ], [ 0, %while.body.i.loopexit ], [ 0, %while.body.i.loopexit.5 ]
  br label %while.body.i
}

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RWByteAddressBuffer)"(i32, %struct.RWByteAddressBuffer)

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer)

; Function Attrs: nounwind
declare void @dx.op.rawBufferStore.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i8, i32)

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandleForLib.struct.RWByteAddressBuffer(i32, %struct.RWByteAddressBuffer)

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties)

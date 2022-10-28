// RUN: %dxc -T vs_6_0 -E main %s -ast-dump | FileCheck %s -check-prefix=CHECKAST
// RUN: %dxc -T vs_6_0 -E main -HV 2021 %s -ast-dump | FileCheck %s -check-prefix=CHECKAST
// RUN: %dxc -T vs_6_0 -E main %s -fcgl | FileCheck %s -check-prefix=CHECKHL
// RUN: %dxc -T vs_6_0 -E main -HV 2021 %s -fcgl | FileCheck %s -check-prefix=CHECKHL
// RUN: %dxc -T vs_6_0 -E main %s | FileCheck %s -check-prefixes=CHECK
// RUN: %dxc -T vs_6_0 -E main -HV 2021 %s | FileCheck %s -check-prefixes=CHECK

// CHECKAST: ConditionalOperator
// CHECKAST-SAME: 'RWStructuredBuffer<Foo>'
// CHECKAST-NEXT: ImplicitCastExpr
// CHECKAST-SAME: 'bool' <LValueToRValue>
// CHECKAST-NEXT: DeclRefExpr
// CHECKAST-SAME: 'b' 'bool'
// CHECKAST-NEXT: ImplicitCastExpr
// CHECKAST-SAME: 'RWStructuredBuffer<Foo>':'RWStructuredBuffer<Foo>' <LValueToRValue>
// CHECKAST-NEXT: DeclRefExpr
// CHECKAST-SAME: 'BufA' 'RWStructuredBuffer<Foo>':'RWStructuredBuffer<Foo>'
// CHECKAST-NEXT: ImplicitCastExpr
// CHECKAST-SAME: 'RWStructuredBuffer<Foo>':'RWStructuredBuffer<Foo>' <LValueToRValue>
// CHECKAST-NEXT: DeclRefExpr
// CHECKAST-SAME: 'BufB' 'RWStructuredBuffer<Foo>':'RWStructuredBuffer<Foo>'

// CHECKHL: %[[tmp:[^ ]+]] = alloca %"class.RWStructuredBuffer<Foo>"
// CHECKHL: %[[ld:[^ ]+]] = load %"class.RWStructuredBuffer<Foo>", %"class.RWStructuredBuffer<Foo>"* %[[tmp]]
// CHECKHL: %[[ch:[^ ]+]] = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.RWStructuredBuffer<Foo>\22)"(i32 0, %"class.RWStructuredBuffer<Foo>" %[[ld]])
// CHECKHL: %[[ah:[^ ]+]] = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.RWStructuredBuffer<Foo>\22)"(i32 11, %dx.types.Handle %[[ch]], %dx.types.ResourceProperties { i32 4620, i32 4 }, %"class.RWStructuredBuffer<Foo>" undef)
// CHECKHL: call %struct.Foo* @"dx.hl.subscript.[].%struct.Foo* (i32, %dx.types.Handle, i32)"(i32 0, %dx.types.Handle %[[ah]], i32 42)

// CHECK: %[[ch:[^ ]+]] = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %[[ch]], i32 42, i32 0)  ; BufferLoad(srv,index,wot)

struct Foo {
  uint u;
};

RWStructuredBuffer<Foo> BufA, BufB;

void Inc(bool b) {
  (b ? BufA : BufB)[42].u += 1;
}

void main(bool b : IN) {
  Inc(true);
}

// RUN: %dxc -DTYPE=float -DNUM=4    -T vs_6_9 %s | FileCheck %s
// RUN: %dxc -DTYPE=bool -DNUM=4     -T vs_6_9 %s | FileCheck %s --check-prefixes=CHECK,I1

// 64-bit types require operation/intrinsic support to convert the values to/from the i32 memory representations.
// RUN: %dxc -DTYPE=uint64_t -DNUM=2 -T vs_6_9 %s | FileCheck %s --check-prefixes=CHECK,I64
// RUN: %dxc -DTYPE=double -DNUM=2   -T vs_6_9 %s | FileCheck %s --check-prefixes=CHECK,F64

///////////////////////////////////////////////////////////////////////
// Test codegen for various load and store operations and conversions
//  for different scalar/vector buffer types and indices.
///////////////////////////////////////////////////////////////////////

// CHECK-DAG: %dx.types.ResRet.[[VTY:v[0-9]*[a-z][0-9][0-9]]] = type { [[VTYPE:<[a-z 0-9]*>]],
// CHECK-DAG: %dx.types.ResRet.[[TY:[if][0-9][0-9]]] = type
// CHECK: %"class.StructuredBuffer<vector<{{.*}}, [[NUM:[0-9]*]]>

  ByteAddressBuffer RoByBuf : register(t1);
RWByteAddressBuffer RwByBuf : register(u1);

StructuredBuffer< vector<TYPE, NUM> > RoStBuf : register(t2);
RWStructuredBuffer< vector<TYPE, NUM>  > RwStBuf : register(u2);

  Buffer< vector<TYPE, NUM>  > RoTyBuf : register(t3);
RWBuffer< vector<TYPE, NUM>  > RwTyBuf : register(u3);

ConsumeStructuredBuffer<vector<TYPE, NUM> > CnStBuf : register(u4);
AppendStructuredBuffer<vector<TYPE, NUM> > ApStBuf  : register(u5);

void main(uint ix[2] : IX) {
  // ByteAddressBuffer Tests

  // CHECK-DAG: [[HDLROBY:%.*]] = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 1, i32 1, i32 0, i8 0 }, i32 1, i1 false)
  // CHECK-DAG: [[HDLRWBY:%.*]] = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 1, i32 1, i32 0, i8 1 }, i32 1, i1 false)

  // CHECK-DAG: [[HDLROST:%.*]] = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 2, i32 2, i32 0, i8 0 }, i32 2, i1 false)
  // CHECK-DAG: [[HDLRWST:%.*]] = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 2, i32 2, i32 0, i8 1 }, i32 2, i1 false)

  // CHECK-DAG: [[HDLROTY:%.*]] = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 3, i32 3, i32 0, i8 0 }, i32 3, i1 false)
  // CHECK-DAG: [[HDLRWTY:%.*]] = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 3, i32 3, i32 0, i8 1 }, i32 3, i1 false)

  // CHECK-DAG: [[HDLCON:%.*]] = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 4, i32 4, i32 0, i8 1 }, i32 4, i1 false)
  // CHECK-DAG: [[HDLAPP:%.*]] = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 5, i32 5, i32 0, i8 1 }, i32 5, i1 false)

  // CHECK: [[IX0:%.*]] = call i32 @dx.op.loadInput.i32(i32 4,

  // CHECK: [[ANHDLRWBY:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDLRWBY]]
  // CHECK: call %dx.types.ResRet.[[VTY]] @dx.op.rawBufferVectorLoad.[[VTY]](i32 303, %dx.types.Handle [[ANHDLRWBY]], i32 [[IX0]]
  // I1: icmp ne [[VTYPE]] %{{.*}}, zeroinitializer
  vector<TYPE, NUM>  babElt1 = RwByBuf.Load< vector<TYPE, NUM>  >(ix[0]);

  // CHECK: [[ANHDLROBY:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDLROBY]]
  // CHECK: call %dx.types.ResRet.[[VTY]] @dx.op.rawBufferVectorLoad.[[VTY]](i32 303, %dx.types.Handle [[ANHDLROBY]], i32 [[IX0]]
  // I1: icmp ne [[VTYPE]] %{{.*}}, zeroinitializer
  vector<TYPE, NUM>  babElt2 = RoByBuf.Load< vector<TYPE, NUM>  >(ix[0]);

  // I1: zext <[[NUM]] x i1> %{{.*}} to [[VTYPE]]
  // CHoCK: all void @dx.op.rawBufferStore.[[TY]](i32 140, %dx.types.Handle [[ANHDLRWBY]], i32 [[IX0]]
  RwByBuf.Store< vector<TYPE, NUM>  >(ix[0], babElt1 + babElt2);

  // StructuredBuffer Tests
  // CHECK: [[ANHDLRWST:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDLRWST]]
  // CHECK: call %dx.types.ResRet.[[VTY]] @dx.op.rawBufferVectorLoad.[[VTY]](i32 303, %dx.types.Handle [[ANHDLRWST]], i32 [[IX0]]
  // I1: icmp ne [[VTYPE]] %{{.*}}, zeroinitializer
  vector<TYPE, NUM>  stbElt1 = RwStBuf.Load(ix[0]);
  // CHECK: [[IX1:%.*]] = call i32 @dx.op.loadInput.i32(i32 4,
  // CHECK: call %dx.types.ResRet.[[VTY]] @dx.op.rawBufferVectorLoad.[[VTY]](i32 303, %dx.types.Handle [[ANHDLRWST]], i32 [[IX1]]
  // I1: icmp ne [[VTYPE]] %{{.*}}, zeroinitializer
  vector<TYPE, NUM>  stbElt2 = RwStBuf[ix[1]];

  // CHECK: [[ANHDLROST:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDLROST]]
  // CHECK: call %dx.types.ResRet.[[VTY]] @dx.op.rawBufferVectorLoad.[[VTY]](i32 303, %dx.types.Handle [[ANHDLROST]], i32 [[IX0]]
  // I1: icmp ne [[VTYPE]] %{{.*}}, zeroinitializer
  vector<TYPE, NUM>  stbElt3 = RoStBuf.Load(ix[0]);
  // CHECK: call %dx.types.ResRet.[[VTY]] @dx.op.rawBufferVectorLoad.[[VTY]](i32 303, %dx.types.Handle [[ANHDLROST]], i32 [[IX1]]
  // I1: icmp ne [[VTYPE]] %{{.*}}, zeroinitializer
  vector<TYPE, NUM>  stbElt4 = RoStBuf[ix[1]];

  // I1: zext <[[NUM]] x i1> %{{.*}} to [[VTYPE]]
  // CHoCK: all void @dx.op.rawBufferStore.[[TY]](i32 140, %dx.types.Handle [[ANHDLRWST]], i32 [[IX0]]
  RwStBuf[ix[0]] = stbElt1 + stbElt2 + stbElt3 + stbElt4;

  // {Append/Consume}StructuredBuffer Tests
  // CHECK: [[ANHDLCON:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDLCON]]
  // CHECK: [[CONIX:%.*]] = call i32 @dx.op.bufferUpdateCounter(i32 70, %dx.types.Handle [[ANHDLCON]], i8 -1) 
  // CHECK: call %dx.types.ResRet.[[VTY]] @dx.op.rawBufferVectorLoad.[[VTY]](i32 303, %dx.types.Handle [[ANHDLCON]], i32 [[CONIX]]
  // I1: icmp ne [[VTYPE]] %{{.*}}, zeroinitializer
  vector<TYPE, NUM>  cnElt = CnStBuf.Consume();

  // CHECK: [[ANHDLAPP:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDLAPP]]
  // CHECK: [[APPIX:%.*]] = call i32 @dx.op.bufferUpdateCounter(i32 70, %dx.types.Handle [[ANHDLAPP]], i8 1) 
  // I1: zext <[[NUM]] x i1> %{{.*}} to [[VTYPE]]
  // CHoCK: all void @dx.op.rawBufferStore.[[TY]](i32 140, %dx.types.Handle [[ANHDLAPP]], i32 [[APPIX]]
  ApStBuf.Append(cnElt);

  // TypedBuffer Tests
  // CHECK: [[ANHDLRWTY:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDLRWTY]]
  // CHECK: call %dx.types.ResRet.[[TY]] @dx.op.bufferLoad.[[TY]](i32 68, %dx.types.Handle [[ANHDLRWTY]], i32 [[IX0]]
  // F64: call double @dx.op.makeDouble.f64(i32 101
  // I64: zext i32 %{{.*}} to i64
  // I64: zext i32 %{{.*}} to i64
  // I64: shl nuw i64
  // I64: or i64
  // I1: icmp ne [[VTYPE]] %{{.*}}, zeroinitializer
  vector<TYPE, NUM>  typElt1 = RwTyBuf.Load(ix[0]);
  // CHECK: call %dx.types.ResRet.[[TY]] @dx.op.bufferLoad.[[TY]](i32 68, %dx.types.Handle [[ANHDLRWTY]], i32 [[IX1]]
  // F64: call double @dx.op.makeDouble.f64(i32 101
  // I64: zext i32 %{{.*}} to i64
  // I64: zext i32 %{{.*}} to i64
  // I64: shl nuw i64
  // I64: or i64
  // I1: icmp ne [[VTYPE]] %{{.*}}, zeroinitializer
  vector<TYPE, NUM>  typElt2 = RwTyBuf[ix[1]];
  // CHECK: [[ANHDLROTY:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDLROTY]]
  // CHECK: call %dx.types.ResRet.[[TY]] @dx.op.bufferLoad.[[TY]](i32 68, %dx.types.Handle [[ANHDLROTY]], i32 [[IX0]]
  // F64: call double @dx.op.makeDouble.f64(i32 101
  // I64: zext i32 %{{.*}} to i64
  // I64: zext i32 %{{.*}} to i64
  // I64: shl nuw i64
  // I64: or i64
  // I1: icmp ne [[VTYPE]] %{{.*}}, zeroinitializer
  vector<TYPE, NUM>  typElt3 = RoTyBuf.Load(ix[0]);
  // CHECK: call %dx.types.ResRet.[[TY]] @dx.op.bufferLoad.[[TY]](i32 68, %dx.types.Handle [[ANHDLROTY]], i32 [[IX1]]
  // F64: call double @dx.op.makeDouble.f64(i32 101
  // I64: zext i32 %{{.*}} to i64
  // I64: zext i32 %{{.*}} to i64
  // I64: shl nuw i64
  // I64: or i64
  // I1: icmp ne [[VTYPE]] %{{.*}}, zeroinitializer
  vector<TYPE, NUM>  typElt4 = RoTyBuf[ix[1]];

  // F64: call %dx.types.splitdouble @dx.op.splitDouble.f64(i32 102
  // I64: trunc i64 %{{.*}} to i32
  // lshr i64  %{{.*}}, 32
  // I64: trunc i64 %{{.*}} to i32
  // I1: zext <[[NUM]] x i1> %{{.*}} to [[VTYPE]]
  // CHECK: all void @dx.op.bufferStore.[[TY]](i32 69, %dx.types.Handle [[ANHDLRWTY]], i32 [[IX0]]
  RwTyBuf[ix[0]] = typElt1 + typElt2 + typElt3 + typElt4;
}

// RUN: %dxc -DARR -DTYPE=float    -DNUM=4 -DIX=SIx -T vs_6_6 %s | FileCheck %s
// RUN: %dxc -DARR -DTYPE=bool     -DNUM=4 -DIX=SIx -T vs_6_6 %s | FileCheck %s
// RUN: %dxc -DARR -DTYPE=uint64_t -DNUM=2 -DIX=SIx -T vs_6_6 %s | FileCheck %s
// RUN: %dxc -DARR -DTYPE=double   -DNUM=2 -DIX=SIx -T vs_6_6 %s | FileCheck %s

// RUN: %dxc -DMAT -DTYPE=float    -DNUM=2 -DIX=SIx -T vs_6_6 %s | FileCheck %s
// RUN: %dxc -DMAT -DTYPE=uint64_t -DNUM=2 -DIX=SIx -T vs_6_6 %s | FileCheck %s
// RUN: %dxc -DMAT -DTYPE=double   -DNUM=2 -DIX=SIx -T vs_6_6 %s | FileCheck %s
// RUN: %dxc -DMAT -DTYPE=float    -DNUM=3 -DIX=SIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,MAT
// RUN: %dxc -DMAT -DTYPE=bool     -DNUM=3 -DIX=SIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,MAT
// RUN: %dxc -DMAT -DTYPE=uint64_t -DNUM=3 -DIX=SIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,MAT
// RUN: %dxc -DMAT -DTYPE=double   -DNUM=3 -DIX=SIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,MAT

// RUN: %dxc -DTYPE=float    -DNUM=4 -DIX=SIx -T vs_6_6 %s | FileCheck %s
// RUN: %dxc -DTYPE=bool     -DNUM=4 -DIX=SIx -T vs_6_6 %s | FileCheck %s
// RUN: %dxc -DTYPE=uint64_t -DNUM=2 -DIX=SIx -T vs_6_6 %s | FileCheck %s
// RUN: %dxc -DTYPE=double   -DNUM=2 -DIX=SIx -T vs_6_6 %s | FileCheck %s

// RUN: %dxc -DOFF -DTYPE=float    -DNUM=4 -DIX=SIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,OFF
// RUN: %dxc -DOFF -DTYPE=bool     -DNUM=4 -DIX=SIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,OFF
// RUN: %dxc -DOFF -DTYPE=uint64_t -DNUM=2 -DIX=SIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,OFF
// RUN: %dxc -DOFF -DTYPE=double   -DNUM=2 -DIX=SIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,OFF

// RUiN: %dxc -DARR -DTYPE=float    -DNUM=4 -DIX=VIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,VIX
// RUiN: %dxc -DARR -DTYPE=bool     -DNUM=4 -DIX=VIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,VIX
// RUiN: %dxc -DARR -DTYPE=uint64_t -DNUM=2 -DIX=VIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,VIX
// RUiN: %dxc -DARR -DTYPE=double   -DNUM=2 -DIX=VIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,VIX

// RUiN: %dxc -DTYPE=float    -DNUM=4 -DIX=VIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,VIX
// RUiN: %dxc -DTYPE=bool     -DNUM=4 -DIX=VIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,VIX
// RUiN: %dxc -DTYPE=uint64_t -DNUM=2 -DIX=VIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,VIX
// RUiN: %dxc -DTYPE=double   -DNUM=2 -DIX=VIx -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,VIX

///////////////////////////////////////////////////////////////////////
// Test codegen for various load and store operations and conversions
//  for different aggregate buffer types and indices.
///////////////////////////////////////////////////////////////////////



// CHECK: %dx.types.ResRet.[[TY:[a-z][0-9][0-9]]] = type { [[TYPE:[a-z0-9]*]],

#if defined(ARR)
#define TYNAME(T,N) T[N]
#define DECL(T,N,V) T V[N]
#elif defined(MAT)
#define TYNAME(T,N) matrix< T, N, N >
#define DECL(T,N,V) matrix< T, N, N > V
#elif defined(OFF)
#define TYNAME(T,N) OffVector< T, N >
#define DECL(T,N,V) OffVector< T, N > V
#else
#define TYNAME(T,N) Vector< T, N >
#define DECL(T,N,V) Vector< T, N > V
#endif

template<typename T, int N>
struct Vector {
  vector<T, N> v;
  Vector operator+(Vector vec) {
    Vector ret;
    ret.v = v + vec.v;
    return ret;
  }
};

template<typename T, int N>
struct OffVector {
  float4 pad1;
  double pad2;
  vector<T, N> v;
  OffVector operator+(OffVector vec) {
    OffVector ret;
    ret.pad1 = 0.0;
    ret.pad2 = 0.0;
    ret.v = v + vec.v;
    return ret;
  }
};

  ByteAddressBuffer RoByBuf : register(t1);
RWByteAddressBuffer RwByBuf : register(u1);

  StructuredBuffer< TYNAME(TYPE,NUM) > RoStBuf : register(t2);
RWStructuredBuffer< TYNAME(TYPE,NUM) > RwStBuf : register(u2);

ConsumeStructuredBuffer< TYNAME(TYPE,NUM) > CnStBuf : register(u4);
AppendStructuredBuffer< TYNAME(TYPE,NUM) > ApStBuf  : register(u5);

TYPE Add(TYPE f1[NUM], TYPE f2[NUM])[NUM] {
  TYPE ret[NUM];
  for (int i = 0; i < NUM; i++)
    ret[i] = f1[i] + f2[i];
  return ret;
}

template<typename T>
T Add(T v1, T v2) { return v1 + v2; }

TYPE Add(TYPE f1[NUM], TYPE f2[NUM], TYPE f3[NUM], TYPE f4[NUM])[NUM] {
  TYPE ret[NUM];
  for (int i = 0; i < NUM; i++)
    ret[i] = f1[i] + f2[i] + f3[i] + f4[i];
  return ret;
}

template<typename T>
T Add(T v1, T v2, T v3, T v4) { return v1 + v2 + v3 + v4; }

void main(uint SIx[2] : SIX, uint1 VIx[2] : VIX) {
  // ByteAddressBuffer Tests

  // CHECK-DAG: [[HDLROBY:%.*]] = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 1, i32 1, i32 0, i8 0 }, i32 1, i1 false)
  // CHECK-DAG: [[HDLRWBY:%.*]] = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 1, i32 1, i32 0, i8 1 }, i32 1, i1 false)

  // CHECK-DAG: [[HDLROST:%.*]] = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 2, i32 2, i32 0, i8 0 }, i32 2, i1 false)
  // CHECK-DAG: [[HDLRWST:%.*]] = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 2, i32 2, i32 0, i8 1 }, i32 2, i1 false)

  // CHECK-DAG: [[HDLCON:%.*]] = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 4, i32 4, i32 0, i8 1 }, i32 4, i1 false)
  // CHECK-DAG: [[HDLAPP:%.*]] = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 5, i32 5, i32 0, i8 1 }, i32 5, i1 false)

  // CHECK-DAG: [[IX0:%.*]] = call i32 @dx.op.loadInput.i32(i32 4, i32 {{[0-9]*}}, i32 [[BOFF:0]]
  // CHECK-DAG: [[RIX0:%.*]] = call i32 @dx.op.loadInput.i32(i32 4, i32 {{[0-9]*}}, i32 [[BOFF]]
  

  // CHECK: [[ANHDLRWBY:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDLRWBY]]
  // OFF: [[RIX0:%.*]] = add i32 [[IX0]], [[BOFF:[0-9]+]]
  // CHECK: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLRWBY]], i32 [[RIX0]]
  // MAT: [[IX0p4:%.*]] = add i32 [[RIX0]], [[p4:[0-9]+]]
  // MAT: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLRWBY]], i32 [[IX0p4]]
  // MAT: [[IX0p8:%.*]] = add i32 [[RIX0]], [[p8:[0-9]+]]
  // MAT: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLRWBY]], i32 [[IX0p8]]
  // I1: icmp ne i32
  // I1: icmp ne i32
  // I1: icmp ne i32
  // I1: icmp ne i32
  DECL(TYPE,NUM,babElt1) = RwByBuf.Load< TYNAME(TYPE,NUM) >(IX[0]);

  // CHECK: [[ANHDLROBY:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDLROBY]]
  // CHECK: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLROBY]], i32 [[RIX0]]
  // MAT: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLROBY]], i32 [[IX0p4]]
  // MAT: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLROBY]], i32 [[IX0p8]]
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  DECL(TYPE,NUM,babElt2) = RoByBuf.Load< TYNAME(TYPE,NUM) >(IX[0]);

  // I1: zext i1 %{{.*}} to i32
  // I1: zext i1 %{{.*}} to i32
  // I1: zext i1 %{{.*}} to i32
  // I1: zext i1 %{{.*}} to i32
  // OFF: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle [[ANHDLRWBY]], i32 {{%.*}}, i32 undef, float 0.0
  // OFF: call void @dx.op.rawBufferStore.f64(i32 140, %dx.types.Handle [[ANHDLRWBY]], i32 {{%.*}}, i32 undef, double 0.0
  // CHECK: call void @dx.op.rawBufferStore.[[TY]](i32 140, %dx.types.Handle [[ANHDLRWBY]], i32 [[RIX0]]
  // MAT: call void @dx.op.rawBufferStore.[[TY]](i32 140, %dx.types.Handle [[ANHDLRWBY]], i32 [[IX0p4]]
  // MAT: call void @dx.op.rawBufferStore.[[TY]](i32 140, %dx.types.Handle [[ANHDLRWBY]], i32 [[IX0p8]]
  RwByBuf.Store< TYNAME(TYPE,NUM) >(IX[0], Add(babElt1, babElt2));

  // StructuredBuffer Tests
  // CHECK: [[ANHDLRWST:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDLRWST]]
  // CHECK: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLRWST]], i32 [[IX0]], i32 [[BOFF]]
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  DECL(TYPE,NUM,stbElt1) = RwStBuf.Load(IX[0]);
  // CHECK: [[IX1:%.*]] = call i32 @dx.op.loadInput.i32(i32 4,
  // CHECK: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLRWST]], i32 [[IX1]], i32 [[BOFF]]
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  DECL(TYPE,NUM,stbElt2) = RwStBuf[IX[1]];

  // CHECK: [[ANHDLROST:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDLROST]]
  // CHECK: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLROST]], i32 [[IX0]], i32 [[BOFF]]
  // MAT: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLROST]], i32 [[IX0]], i32 [[p4]]
  // MAT: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLROST]], i32 [[IX0]], i32 [[p8]]
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  DECL(TYPE,NUM,stbElt3) = RoStBuf.Load(IX[0]);
  // CHECK: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLROST]], i32 [[IX1]], i32 [[BOFF]]
  // MAT: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLROST]], i32 [[IX1]], i32 [[p4]]
  // MAT: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLROST]], i32 [[IX1]], i32 [[p8]]
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  DECL(TYPE,NUM,stbElt4) = RoStBuf[IX[1]];

  // I1: zext i1 %{{.*}} to i32
  // I1: zext i1 %{{.*}} to i32
  // I1: zext i1 %{{.*}} to i32
  // I1: zext i1 %{{.*}} to i32
  // OFF: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle [[ANHDLRWST]], i32 [[IX0]], i32 0, float 0.0
  // OFF: call void @dx.op.rawBufferStore.f64(i32 140, %dx.types.Handle [[ANHDLRWST]], i32 [[IX0]], i32 16, double 0.0
  // CHECK: call void @dx.op.rawBufferStore.[[TY]](i32 140, %dx.types.Handle [[ANHDLRWST]], i32 [[IX0]], i32 [[BOFF]]
  // MAT: call void @dx.op.rawBufferStore.[[TY]](i32 140, %dx.types.Handle [[ANHDLRWST]], i32 [[IX0]], i32 [[p4]]
  // MAT: call void @dx.op.rawBufferStore.[[TY]](i32 140, %dx.types.Handle [[ANHDLRWST]], i32 [[IX0]], i32 [[p8]]
  RwStBuf[IX[0]] = Add(stbElt1, stbElt2, stbElt3, stbElt4);

  // {Append/Consume}StructuredBuffer Tests
  // CHECK: [[ANHDLCON:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDLCON]]
  // CHECK: [[CONIX:%.*]] = call i32 @dx.op.bufferUpdateCounter(i32 70, %dx.types.Handle [[ANHDLCON]], i8 -1) 
  // OFF: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDLCON]], i32 [[CONIX]], i32 
  // OFF: call %dx.types.ResRet.f64 @dx.op.rawBufferLoad.f64(i32 139, %dx.types.Handle [[ANHDLCON]], i32 [[CONIX]], i32 16
  // CHECK: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLCON]], i32 [[CONIX]], i32 [[BOFF]]
  // MAT: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLCON]], i32 [[CONIX]], i32 [[p4]]
  // MAT: call %dx.types.ResRet.[[TY]] @dx.op.rawBufferLoad.[[TY]](i32 139, %dx.types.Handle [[ANHDLCON]], i32 [[CONIX]], i32 [[p8]]
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  // I1: icmp ne i32 %{{.*}}, 0
  DECL(TYPE,NUM,cnElt) = CnStBuf.Consume();

  // CHECK: [[ANHDLAPP:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDLAPP]]
  // CHECK: [[APPIX:%.*]] = call i32 @dx.op.bufferUpdateCounter(i32 70, %dx.types.Handle [[ANHDLAPP]], i8 1) 
  // I1: zext i1 %{{.*}} to i32
  // I1: zext i1 %{{.*}} to i32
  // I1: zext i1 %{{.*}} to i32
  // I1: zext i1 %{{.*}} to i32
  // OFF: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle [[ANHDLAPP]], i32 [[APPIX]], i32 0
  // OFF: call void @dx.op.rawBufferStore.f64(i32 140, %dx.types.Handle [[ANHDLAPP]], i32 [[APPIX]], i32 16
  // CHECK: call void @dx.op.rawBufferStore.[[TY]](i32 140, %dx.types.Handle [[ANHDLAPP]], i32 [[APPIX]], i32 [[BOFF]]
  // MAT: call void @dx.op.rawBufferStore.[[TY]](i32 140, %dx.types.Handle [[ANHDLAPP]], i32 [[APPIX]], i32 [[p4]]
  // MAT: call void @dx.op.rawBufferStore.[[TY]](i32 140, %dx.types.Handle [[ANHDLAPP]], i32 [[APPIX]], i32 [[p8]]
  ApStBuf.Append(cnElt);
}

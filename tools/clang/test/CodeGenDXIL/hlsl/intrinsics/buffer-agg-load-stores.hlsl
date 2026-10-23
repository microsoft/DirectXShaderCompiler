// RUN: %dxc -fcgl -T vs_6_6              -DETY=float     -DCOLS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6              -DETY=bool      -DCOLS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6              -DETY=uint64_t  -DCOLS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6              -DETY=double    -DCOLS=3 %s | FileCheck %s

// RUN: %dxc -fcgl -T vs_6_6              -DETY=float1    -DCOLS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6              -DETY=bool1     -DCOLS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6              -DETY=uint64_t1 -DCOLS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6              -DETY=double1   -DCOLS=3 %s | FileCheck %s

// RUN: %dxc -fcgl -T vs_6_6              -DETY=float4    -DCOLS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6              -DETY=bool4     -DCOLS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6              -DETY=uint64_t4 -DCOLS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6              -DETY=double4   -DCOLS=3 %s | FileCheck %s

// RUN: %dxc -fcgl -T vs_6_6 -DATY=matrix -DETY=float    -DCOLS=2 -DROWS=2 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=matrix -DETY=bool     -DCOLS=2 -DROWS=2 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=matrix -DETY=uint64_t -DCOLS=2 -DROWS=2 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=matrix -DETY=double   -DCOLS=2 -DROWS=2 %s | FileCheck %s

// RUN: %dxc -fcgl -T vs_6_6 -DATY=matrix -DETY=float    -DCOLS=3 -DROWS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=matrix -DETY=bool     -DCOLS=3 -DROWS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=matrix -DETY=uint64_t -DCOLS=3 -DROWS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=matrix -DETY=double   -DCOLS=3 -DROWS=3 %s | FileCheck %s

// RUN: %dxc -fcgl -T vs_6_6 -DATY=Matrix -DETY=float    -DCOLS=2 -DROWS=2 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=Matrix -DETY=uint64_t -DCOLS=2 -DROWS=2 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=Matrix -DETY=double   -DCOLS=2 -DROWS=2 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=Matrix -DETY=float    -DCOLS=3 -DROWS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=Matrix -DETY=bool     -DCOLS=3 -DROWS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=Matrix -DETY=uint64_t -DCOLS=3 -DROWS=3 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=Matrix -DETY=double   -DCOLS=3 -DROWS=3 %s | FileCheck %s

// RUN: %dxc -fcgl -T vs_6_6 -DATY=Vector -DETY=float    -DCOLS=4 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=Vector -DETY=bool     -DCOLS=4 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=Vector -DETY=uint64_t -DCOLS=2 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=Vector -DETY=double   -DCOLS=2 %s | FileCheck %s

// RUN: %dxc -fcgl -T vs_6_6 -DATY=OffVector -DETY=float    -DCOLS=4 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=OffVector -DETY=bool     -DCOLS=4 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=OffVector -DETY=uint64_t -DCOLS=2 %s | FileCheck %s
// RUN: %dxc -fcgl -T vs_6_6 -DATY=OffVector -DETY=double   -DCOLS=2 %s | FileCheck %s

///////////////////////////////////////////////////////////////////////
// Test codegen for various load and store operations and conversions
//  for different aggregate buffer types and indices.
///////////////////////////////////////////////////////////////////////

#if !defined(ATY)
// Arrays have no aggregate typename
#define TYPE ETY
#define SS [COLS]
#elif defined(ROWS)
// Matrices have two dimensions
#define TYPE ATY< ETY, COLS, ROWS>
#define SS
#else
// All else matches this formulation
#define TYPE ATY< ETY, COLS>
#define SS
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

template<typename T, int N, int M>
struct Matrix {
  matrix<T, N, M> m;
  Matrix operator+(Matrix mat) {
    Matrix ret;
    ret.m = m + mat.m;
    return ret;
  }
};

  ByteAddressBuffer RoByBuf : register(t1);
RWByteAddressBuffer RwByBuf : register(u1);

  StructuredBuffer< TYPE SS > RoStBuf : register(t2);
RWStructuredBuffer< TYPE SS > RwStBuf : register(u2);

ConsumeStructuredBuffer< TYPE SS > CnStBuf : register(u4);
AppendStructuredBuffer< TYPE SS > ApStBuf  : register(u5);

TYPE Add(TYPE f1[COLS], TYPE f2[COLS], TYPE f3[COLS], TYPE f4[COLS])[COLS] {
  TYPE ret[COLS];
  for (int i = 0; i < COLS; i++)
    ret[i] = f1[i] + f2[i] + f3[i] + f4[i];
  return ret;
}

template<typename T>
T Add(T v1, T v2, T v3, T v4) { return v1 + v2 + v3 + v4; }

TYPE Add(TYPE f1[COLS], TYPE f2[COLS], TYPE f3[COLS], TYPE f4[COLS], TYPE f5[COLS], TYPE f6[COLS])[COLS] {
  TYPE ret[COLS];
  for (int i = 0; i < COLS; i++)
    ret[i] = f1[i] + f2[i] + f3[i] + f4[i] + f5[i] + f6[i];
  return ret;
}

template<typename T>
T Add(T v1, T v2, T v3, T v4, T v5, T v6) { return v1 + v2 + v3 + v4 + v5 + v6; }

// CHECK-LABEL: define void @main
void main(uint ix[3] : IX) {
  // ByteAddressBuffer Tests
  // CHECK-COUNT-4: call {{.*}}@"dx.hl.op{{.*}}"(i32 231,
  // CHECK-COUNT-2: call void @"dx.hl.op{{.*}}"(i32 277,

  // These -DAGs must match the same line. That is the only reason for the -DAG.
  // The first match will assign [[IX0]] to the actual index value.
  // For most runs, the second match will assign [[RIX0]] to the same thing.
  // For ByteAddressBuffers (Raw Buffers), the index gets offset sometimes to account
  // for lack offset support and a separate variable is needed for this index + offset value.
  // For these cases, the OFF : lines below will match the updated index value with the new offsets.
  // These lines will always match the same line since this shader can only produce one loadInput call.
  TYPE babElt1 SS = RwByBuf.Load< TYPE SS >(ix[0]);

  uint status1;
  TYPE babElt3 SS = RwByBuf.Load< TYPE SS >(ix[1], status1);

  TYPE babElt2 SS = RoByBuf.Load< TYPE SS >(ix[0]);

  uint status2;
  TYPE babElt4 SS = RoByBuf.Load< TYPE SS >(ix[1], status2);

  RwByBuf.Store< TYPE SS >(ix[0], Add(babElt1, babElt2, babElt3, babElt4));
  RwByBuf.Store< uint > (100, status1 && status2);

  // StructuredBuffer Tests
  // StructuredBuffer loads, subscripts, and stores.
  
  // CHECK: call {{.*}}@"dx.hl.op{{.*}}"(i32 231,
  TYPE stbElt1 SS = RwStBuf.Load(ix[0]);

  // CHECK: call {{.*}}@"dx.hl.subscript.{{.*}}"(i32 0,
  TYPE stbElt2 SS = RwStBuf[ix[1]];

  // CHECK: call {{.*}}@"dx.hl.op{{.*}}"(i32 231,
  TYPE stbElt5 SS = RwStBuf.Load(ix[2], status1);

  // CHECK: call {{.*}}@"dx.hl.op{{.*}}"(i32 231,
  TYPE stbElt3 SS = RoStBuf.Load(ix[0]);

  // CHECK: call {{.*}}@"dx.hl.subscript.{{.*}}"(i32 0,
  TYPE stbElt4 SS = RoStBuf[ix[1]];

  // CHECK: call {{.*}}@"dx.hl.op{{.*}}"(i32 231,
  TYPE stbElt6 SS = RoStBuf.Load(ix[2], status2);

  // CHECK: call {{.*}}@"dx.hl.subscript.{{.*}}"(i32 0,
  // CHECK: call void @"dx.hl.op{{.*}}"(i32 277,
  RwStBuf[ix[0]] = Add(stbElt1, stbElt2, stbElt3, stbElt4, stbElt5, stbElt6);
  RwByBuf.Store< uint > (200, status1 && status2);

  // {Append/Consume}StructuredBuffer Tests
  // CHECK: call {{.*}}@"dx.hl.op..consume{{.*}}"(i32 283,
  TYPE cnElt SS = CnStBuf.Consume();

  // CHECK: call void @"dx.hl.op..appendvoid{{.*}}"(i32 226,
  ApStBuf.Append(cnElt);
}

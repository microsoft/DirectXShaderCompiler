// RUN: %dxc -fcgl -DTYPE=float4    -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,FLOAT
// RUN: %dxc -fcgl -DTYPE=bool4     -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,BOOL
// RUN: %dxc -fcgl -DTYPE=uint64_t2 -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,UINT64
// RUN: %dxc -fcgl -DTYPE=double2   -T vs_6_6 %s | FileCheck %s --check-prefixes=CHECK,DOUBLE

///////////////////////////////////////////////////////////////////////
// Test codegen for various load and store operations and conversions
//  for different scalar/vector buffer types and indices.
///////////////////////////////////////////////////////////////////////

// These -DAGs must match the same line. That is the only reason for the -DAG.
// The first match will assign [[TY]] to the native type
// For most runs, the second match will assign [[TY32]] to the same thing.
// For 64-bit types, the memory representation is i32 and a separate variable is needed.
// For these cases, there is another line that will always match i32.
// This line will also force the previous -DAGs to match the same line since the most
// This shader can produce is two ResRet types.

  ByteAddressBuffer RoByBuf : register(t1);
RWByteAddressBuffer RwByBuf : register(u1);

  StructuredBuffer< TYPE > RoStBuf : register(t2);
RWStructuredBuffer< TYPE > RwStBuf : register(u2);

ConsumeStructuredBuffer<TYPE> CnStBuf : register(u3);
AppendStructuredBuffer<TYPE> ApStBuf  : register(u4);

  Buffer< TYPE > RoTyBuf : register(t5);
RWBuffer< TYPE > RwTyBuf : register(u5);

  Texture1D< TYPE > RoTex1d : register(t6);
RWTexture1D< TYPE > RwTex1d : register(u6);
  Texture2D< TYPE > RoTex2d : register(t7);
RWTexture2D< TYPE > RwTex2d : register(u7);
  Texture3D< TYPE > RoTex3d : register(t8);
RWTexture3D< TYPE > RwTex3d : register(u8);

// CHECK-LABEL: define void @main
void main(uint ix0 : IX0, uint ix1 : IX1, uint2 ix2 : IX2, uint3 ix3 : IX3) {
  // ByteAddressBuffer Tests

  // FLOAT: call <4 x float> @"dx.hl.op.ro.<4 x float> (i32, %dx.types.Handle, i32)"(i32 231,
  // BOOL: call <4 x i1> @"dx.hl.op.ro.<4 x i1> (i32, %dx.types.Handle, i32)"(i32 231,
  // UINT64: call <2 x i64> @"dx.hl.op.ro.<2 x i64> (i32, %dx.types.Handle, i32)"(i32 231,
  // DOUBLE: call <2 x double> @"dx.hl.op.ro.<2 x double> (i32, %dx.types.Handle, i32)"(i32 231,
  TYPE babElt1 = RwByBuf.Load< TYPE >(ix0);
  
  // CHECK: call {{.*}}@"dx.hl.op{{.*}}"(i32 231,
  TYPE babElt2 = RoByBuf.Load< TYPE >(ix0);

  // CHECK: call {{.*}}@"dx.hl.op{{.*}}"(i32 231,
  uint status1 = 0;
  TYPE babElt3 = RwByBuf.Load< TYPE >(ix1, status1);

  // CHECK: call {{.*}}@"dx.hl.op{{.*}}"(i32 231,
  uint status2 = 0;
  TYPE babElt4 = RoByBuf.Load< TYPE >(ix1, status2);

  // CHECK-COUNT-2: call void @"dx.hl.op{{.*}}"(i32 277,  
  RwByBuf.Store< TYPE >(ix0, babElt1 + babElt2 + babElt3 + babElt4);
  RwByBuf.Store< uint > (100, status1 && status2);

  // StructuredBuffer Tests
  // CHECK: call {{.*}}@"dx.hl.op{{.*}}"(i32 231,
  TYPE stbElt1 = RwStBuf.Load(ix0);

  // CHECK: call {{.*}}@"dx.hl.subscript.{{.*}}"(i32 0,
  TYPE stbElt2 = RwStBuf[ix1];

  // CHECK: call {{.*}}@"dx.hl.op{{.*}}"(i32 231,
  TYPE stbElt3 = RoStBuf.Load(ix0);

  // CHECK: call {{.*}}@"dx.hl.subscript.{{.*}}"(i32 0,
  TYPE stbElt4 = RoStBuf[ix1];

  // CHECK-COUNT-2: call {{.*}}@"dx.hl.op{{.*}}"(i32 231,
  TYPE stbElt5 = RwStBuf.Load(ix2[0], status1);
  TYPE stbElt6 = RoStBuf.Load(ix2[0], status2);

  // CHECK: call {{.*}}@"dx.hl.subscript.{{.*}}"(i32 0,
  // CHECK: call void @"dx.hl.op{{.*}}"(i32 277,
  RwStBuf[ix0] = stbElt1 + stbElt2 + stbElt3 + stbElt4 + stbElt5 + stbElt6;
  RwByBuf.Store< uint > (200, status1 && status2);

  // {Append/Consume}StructuredBuffer Tests
  // CHECK: call {{.*}}@"dx.hl.op..consume{{.*}}"(i32 283,
  TYPE cnElt = CnStBuf.Consume();
  
  // CHECK: call void @"dx.hl.op..appendvoid{{.*}}"(i32 226,
  ApStBuf.Append(cnElt);

  // TypedBuffer Tests
  // CHECK: call {{.*}}@"dx.hl.op{{.*}}"(i32 231,
  TYPE typElt1 = RwTyBuf.Load(ix0);

  // CHECK: call {{.*}}@"dx.hl.subscript.{{.*}}"(i32 0,
  TYPE typElt2 = RwTyBuf[ix1];

  // CHECK: call {{.*}}@"dx.hl.op{{.*}}"(i32 231,
  TYPE typElt3 = RoTyBuf.Load(ix0);

  // CHECK: call {{.*}}@"dx.hl.subscript.{{.*}}"(i32 0,
  TYPE typElt4 = RoTyBuf[ix1];

  // CHECK-COUNT-2: call {{.*}}@"dx.hl.op{{.*}}"(i32 231,
  TYPE typElt5 = RwTyBuf.Load(ix2[0], status1);
  TYPE typElt6 = RoTyBuf.Load(ix2[0], status2);

  // CHECK: call {{.*}}@"dx.hl.subscript.{{.*}}"(i32 0,
  // CHECK: call void @"dx.hl.op{{.*}}"(i32 277,
  RwTyBuf[ix0] = typElt1 + typElt2 + typElt3 + typElt4 + typElt5 + typElt6;
  RwByBuf.Store< uint > (300, status1 && status2);

  // Texture Tests
  // CHECK-COUNT-7: call {{.*}}@"dx.hl.subscript.{{.*}}"(i32 0,
  TYPE texElt1 = RoTex1d[ix0];
  TYPE texElt2 = RwTex1d[ix0];
  TYPE texElt3 = RoTex2d[ix2];
  TYPE texElt4 = RwTex2d[ix2];
  TYPE texElt5 = RoTex3d[ix3];
  TYPE texElt6 = RwTex3d[ix3];
  RwTex3d[ix3] = texElt1 + texElt2 + texElt3 + texElt4 + texElt5 + texElt6;
}

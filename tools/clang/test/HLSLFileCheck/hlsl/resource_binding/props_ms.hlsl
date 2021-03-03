// RUN: %dxc -E main -T ps_6_6 %s -DCT=float -DCC=1 -DSCALAR=1          | FileCheck %s -D ELTY=F32   -D PROP1=265
// RUN: %dxc -E main -T ps_6_6 %s -DCT=float -DCC=1 -DSCALAR=1 -DSC=,0  | FileCheck %s -D ELTY=F32   -D PROP1=265
// RUN: %dxc -E main -T ps_6_6 %s -DCT=float -DCC=1 -DSCALAR=1 -DSC=,8  | FileCheck %s -D ELTY=F32   -D PROP1=265
// RUN: %dxc -E main -T ps_6_6 %s -DCT=float -DCC=1                     | FileCheck %s -D ELTY=F32   -D PROP1=265
// RUN: %dxc -E main -T ps_6_6 %s -DCT=float -DCC=2                     | FileCheck %s -D ELTY=2xF32 -D PROP1=521
// RUN: %dxc -E main -T ps_6_6 %s -DCT=float -DCC=3                     | FileCheck %s -D ELTY=3xF32 -D PROP1=777
// RUN: %dxc -E main -T ps_6_6 %s -DCT=float                            | FileCheck %s -D ELTY=4xF32 -D PROP1=1033
// RUN: %dxc -E main -T ps_6_6 %s -DCT=float -DSC=,0                    | FileCheck %s -D ELTY=4xF32 -D PROP1=1033
// RUN: %dxc -E main -T ps_6_6 %s -DCT=float -DSC=,8                    | FileCheck %s -D ELTY=4xF32 -D PROP1=1033

// RUN: %dxc -E main -T ps_6_6 %s -DCT=int                              | FileCheck %s -D ELTY=4xI32 -D PROP1=1028
// RUN: %dxc -E main -T ps_6_6 %s -DCT=uint                             | FileCheck %s -D ELTY=4xU32 -D PROP1=1029

// half is float for shader type unless -enable-16bit-types is specified
// RUN: %dxc -E main -T ps_6_6 %s -DCT=half                             | FileCheck %s -D ELTY=4xF32 -D PROP1=1033
// RUN: %dxc -E main -T ps_6_6 %s -DCT=half       -enable-16bit-types   | FileCheck %s -D ELTY=4xF32 -D PROP1=1032

// component type is shader type, not storage type,
// so it's 16-bit for min-precision with or without -enable-16bit-types
// RUN: %dxc -E main -T ps_6_6 %s -DCT=min16float                       | FileCheck %s -D ELTY=4xF32 -D PROP1=1032
// RUN: %dxc -E main -T ps_6_6 %s -DCT=min16float -enable-16bit-types   | FileCheck %s -D ELTY=4xF32 -D PROP1=1032
// RUN: %dxc -E main -T ps_6_6 %s -DCT=min16int                         | FileCheck %s -D ELTY=4xF32 -D PROP1=1026
// RUN: %dxc -E main -T ps_6_6 %s -DCT=min16int   -enable-16bit-types   | FileCheck %s -D ELTY=4xF32 -D PROP1=1026

// Native 16-bit type looks the same in props as min16
// RUN: %dxc -E main -T ps_6_6 %s -DCT=float16_t  -enable-16bit-types   | FileCheck %s -D ELTY=4xF32 -D PROP1=1032
// RUN: %dxc -E main -T ps_6_6 %s -DCT=int16_t    -enable-16bit-types   | FileCheck %s -D ELTY=4xF32 -D PROP1=1026

// Ensure that MS textures from heap have expected properties

// CHECK: 
// CHECK: call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %{{.*}}, %dx.types.ResourceProperties { i32 3, i32 [[PROP1]] })
// CHECK-SAME: resource: Texture2DMS<[[ELTY]]>
// CHECK: call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %{{.*}}, %dx.types.ResourceProperties { i32 8, i32 [[PROP1]] })
// CHECK-SAME: resource: Texture2DMSArray<[[ELTY]]>

// CT = ComponentType
// CC = ComponentCount
#ifndef CC
#define CC 4
#endif
// SC = SampleCount
#ifndef SC
#define SC
#endif

#ifdef SCALAR
Texture2DMS<CT SC> TexMS_scalar : register(t0);
Texture2DMSArray<CT SC> TexMSA_scalar : register(t1);
#else
Texture2DMS<vector<CT, CC> SC> TexMS_vector : register(t0);
Texture2DMSArray<vector<CT, CC> SC> TexMSA_vector : register(t1);
#endif

vector<CT, CC> main(int4 a : A, float4 coord : TEXCOORD) : SV_TARGET
{
  return (vector<CT, CC>)(0)
#ifdef SCALAR
    + TexMS_scalar.Load(a.xy, a.w)
    + TexMSA_scalar.Load(a.xyz, a.w)
#else
    + TexMS_vector.Load(a.xy, a.w)
    + TexMSA_vector.Load(a.xyz, a.w)
#endif
    ;
}

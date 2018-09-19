// RUN: %dxc -E main -T ps_6_0 %s  | FileCheck %s

// CHECK: ; cbuffer MyCB
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct MyCB
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       float4 f;                                     ; Offset:   64
// CHECK: ;       float4 f2;                                    ; Offset:  112
// CHECK: ;       float fa[15];                                 ; Offset:  128
// CHECK: ;
// CHECK: ;   } MyCB                                            ; Offset:    0 Size:   356
// CHECK: ;
// CHECK: ; }

// CHECK: ; cbuffer buf2
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct buf2
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       struct struct.Foo
// CHECK: ;       {
// CHECK: ;
// CHECK: ;           float4 f;                                 ; Offset:    0
// CHECK: ;           int4 i;                                   ; Offset:   16
// CHECK: ;
// CHECK: ;       } buf2                                        ; Offset:    0
// CHECK: ;
// CHECK: ;
// CHECK: ;   } buf2                                            ; Offset:    0 Size:    32
// CHECK: ;
// CHECK: ; }

// CHECK: ; tbuffer MyTB
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct MyTB
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       float f3;                                     ; Offset:    0
// CHECK: ;       float4 f4;                                    ; Offset:   16
// CHECK: ;
// CHECK: ;   } MyTB                                            ; Offset:    0 Size:    32
// CHECK: ;
// CHECK: ; }

// CHECK: ; tbuffer tbuf1
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct tbuf1
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       struct struct.Foo
// CHECK: ;       {
// CHECK: ;
// CHECK: ;           float4 f;                                 ; Offset:    0
// CHECK: ;           int4 i;                                   ; Offset:   16
// CHECK: ;
// CHECK: ;       } tbuf1                                       ; Offset:    0
// CHECK: ;
// CHECK: ;
// CHECK: ;   } tbuf1                                           ; Offset:    0 Size:    32
// CHECK: ;
// CHECK: ; }

// CHECK: ; Resource Bindings:
// CHECK: ;
// CHECK: ; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
// CHECK: ; ------------------------------ ---------- ------- ----------- ------- -------------- ------
// CHECK: ; MyCB                              cbuffer      NA          NA     CB0           cb11     1
// CHECK: ; buf3                              cbuffer      NA          NA     CB1           cb82    15
// CHECK: ; buf4                              cbuffer      NA          NA     CB2            cb0     3
// CHECK: ; buf1                              cbuffer      NA          NA     CB3    cb77,space3    32
// CHECK: ; buf2                              cbuffer      NA          NA     CB4           cb18    64
// CHECK: ; Samp2                             sampler      NA          NA      S0             s0     1
// CHECK: ; Samp3                             sampler      NA          NA      S1            s25     6
// CHECK: ; Samp4                             sampler      NA          NA      S2             s4    21
// CHECK: ; Samp1                             sampler      NA          NA      S3             s1     3
// CHECK: ; Tex1                              texture     f32          2d      T0             t0     1
// CHECK: ; Tex2                              texture     f32          2d      T1            t11    21
// CHECK: ; Tex3                              texture     f32          2d      T2            t36    65
// CHECK: ; Tex4                              texture     f32          2d      T3             t7     4
// CHECK: ; MyTB                              texture     u32     tbuffer      T4     t11,space3     1
// CHECK: ; tbuf1                             texture     u32     tbuffer      T5            t32     4
// CHECK: ; tbuf3                             texture     u32     tbuffer      T6             t3     4
// CHECK: ; tbuf2                             texture     u32     tbuffer      T7             t1     2
// CHECK: ; tbuf4                             texture     u32     tbuffer      T8      t2,space3     4
// CHECK: ; RWTex2                                UAV     f32          2d      U0      u7,space7     1
// CHECK: ; RWTex3                                UAV     f32          2d      U1             u5    12
// CHECK: ; RWTex4                                UAV     f32          2d      U2            u17     6
// CHECK: ; RWTex1                                UAV     f32          2d      U3             u0     4

// CHECK: %struct.Resources = type { %"class.Texture2D<float>", %"class.Texture2D<vector<float, 4> >", %"class.Texture2D<float>", %"class.Texture2D<vector<float, 4> >", %"class.RWTexture2D<vector<float, 4> >", %"class.RWTexture2D<vector<float, 4> >", %"class.RWTexture2D<vector<float, 4> >", %"class.RWTexture2D<vector<float, 4> >", %struct.SamplerComparisonState, %struct.SamplerState, %struct.SamplerComparisonState, %struct.SamplerState, <4 x float> }

// CHECK: %RWTex2_UAV_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 7, i1 false)
// CHECK: %MyTB_texture_tbuffer = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 4, i32 11, i1 false)

// CHECK: %Tex1_texture_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 0, i1 false)
// CHECK: %Samp2_sampler = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 0, i32 0, i1 false)

// CHECK: %MyCB_cbuffer = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 11, i1 false)

// CHECK: %tbuf4_texture_tbuffer = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 8, i32 4, i1 false)
// CHECK: %tbuf2_texture_tbuffer = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 7, i32 2, i1 false)
// CHECK: %tbuf3_texture_tbuffer = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 6, i32 6, i1 false)
// CHECK: %tbuf1_texture_tbuffer = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 5, i32 35, i1 false)

// CHECK: %buf2_cbuffer = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 4, i32 55, i1 false)
// CHECK: %buf1_cbuffer = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 3, i32 104, i1 false)
// CHECK: %buf4_cbuffer = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 2, i32 1, i1 false)

// CHECK: %Tex2_texture_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 1, i32 30, i1 false)
// CHECK: %Tex3_texture_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 2, i32 94, i1 false)
// CHECK: %Tex4_texture_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 3, i32 10, i1 false)
// CHECK: %RWTex1_UAV_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 3, i32 2, i1 false)
// CHECK: %RWTex3_UAV_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 1, i32 14, i1 false)
// CHECK: %RWTex4_UAV_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 2, i32 22, i1 false)
// CHECK: %Samp1_sampler = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 3, i32 3, i1 false)
// CHECK: %Samp3_sampler = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 1, i32 29, i1 false)
// CHECK: %Samp4_sampler = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 2, i32 23, i1 false)

// check packoffset:
// CHECK: @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %MyCB_cbuffer, i32 4)
// CHECK: @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %MyCB_cbuffer, i32 7)
// CHECK: @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %MyCB_cbuffer, i32 21)

// check element index:
// CHECK: @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %tbuf1_texture_tbuffer, i32 1, i32 undef)



cbuffer MyCB : register(b11)
{
  float4 f : packoffset(c4);
  float4 f2 : packoffset(c7);
  float fa[3][5] : packoffset(c8);

  tbuffer MyTB : register(t11, space3)
  {
    float f3;
    float4 f4;
  };
};

Texture2D<float> Tex1; // t0
Texture2D Tex2[3][7] : register(t11);
Texture2D<float> Tex3[13][5] : register(t36);

RWTexture2D<float4> RWTex2 : register(u7, space7);
RWTexture2D<float4> RWTex3[2][6] : register(u5);
RWTexture2D<float4> RWTex4[3][2]; // u17
RWTexture2D<float4> RWTex1[4];  // u0

// Ensure unused explicitly bound does not reserve space:
RWTexture2D<float4> RWUnused[6] : register(u17);

struct Foo
{
  float4 f;
  int4 i;
};

SamplerState Samp2; // s0
SamplerComparisonState Samp3[2][3]; // s25
SamplerState Samp4[7][3] : register(s4);
SamplerComparisonState Samp1[3]; // s1

struct Resources
{
  Texture2D<float> Tex1;
  Texture2D Tex2;
  Texture2D<float> Tex3;
  Texture2D Tex4;
  RWTexture2D<float4> RWTex1;
  RWTexture2D<float4> RWTex2;
  RWTexture2D<float4> RWTex3;
  RWTexture2D<float4> RWTex4;
  SamplerComparisonState Samp1;
  SamplerState Samp2;
  SamplerComparisonState Samp3;
  SamplerState Samp4;
  float4 foo;
};

ConstantBuffer<Foo> buf5[4]; // unallocated
ConstantBuffer<Foo> buf3[15]; // cb82
ConstantBuffer<Foo> buf4[3]; // cb0
ConstantBuffer<Foo> buf1[32] : register(b77, space3);
ConstantBuffer<Foo> buf2[4][16] : register(b18);

TextureBuffer<Foo> tbuf1[2][2]; // t32
TextureBuffer<Foo> tbuf3[4] : register(t3);
TextureBuffer<Foo> tbuf2[2]; // t1
TextureBuffer<Foo> tbuf4[4] : register(t2, space3);

Texture2D Tex4[4]; // t7

float4 main(int4 a : A, float4 coord : TEXCOORD) : SV_TARGET
{
  Resources res;
  res.Tex1 = Tex1; // t0
  res.Tex2 = Tex2[2][5]; // t11 + (2 * 7) + 5 = 30
  res.Tex3 = Tex3[11][3]; // t36 + (11 * 5) + 3 = 94
  res.Tex4 = Tex4[3]; // t7 + 3 = 10
  res.RWTex1 = RWTex1[2]; // u0 + 2 = 2
  res.RWTex2 = RWTex2; // u7
  res.RWTex3 = RWTex3[1][3]; // u5 + (1 * 6) + 3 = 14
  res.RWTex4 = RWTex4[2][1]; // u17 + (2 * 2) + 1 = 22
  res.Samp1 = Samp1[2]; // s1 + 2 = 3
  res.Samp2 = Samp2; // s0
  res.Samp3 = Samp3[1][1]; // s25 + (1 * 3) + 1 = 29
  res.Samp4 = Samp4[6][1]; // s4 + (6 * 3) + 1 = 23
  return (float4)1.0
    * f   // c4
    * f2  // c7
    * fa[2][3] // c8 + (2 * 5) + 3 = 21
    * f4.z
    * buf1[27].f // cb77 + 27 = 104
    * buf2[2][5].f // cb18 + (2 * 16) + 5 = 55
    * buf4[1].f // cb0 + 1 = 1
    * buf3[a.y].f
    // * Tex1.Sample(Samp2, coord.xy)
    // * res.Tex1.SampleCmp(Samp1[2], coord.xy, coord.z)
    * res.Tex1.SampleCmp(res.Samp1, coord.xy, coord.z)
    * res.Tex2.Sample(res.Samp2, coord.xy)
    * res.Tex4.Sample(res.Samp4, coord.xy)
    * res.Tex3.SampleCmp(res.Samp3, coord.xy, coord.z)
    // * res.Tex4.Sample(Samp4[6][1], coord.xy)
    * res.Tex4.Sample(res.Samp4, coord.xy)
    * res.RWTex1.Load(a.xy)
    * res.RWTex2.Load(a.xy)
    * res.RWTex3.Load(a.xy)
    * res.RWTex4.Load(a.xy)
    * tbuf1[1][1].i // t32 + (1 * 2) + 1 = 35
    * tbuf2[1].f // t1 + 1 = 2
    * tbuf3[3].f // t3 + 3 = 6
    * tbuf4[2].f // t2 + 2 = 4
    ;
}

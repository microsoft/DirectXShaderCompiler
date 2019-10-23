// RUN: %dxilver 1.2 | %dxc -E main -T ps_6_2 -enable-16bit-types -HV 2018 -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: Use native low precision
// CHECK: ; cbuffer f
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct f
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       struct struct.Foo
// CHECK: ;       {
// CHECK: ;
// CHECK: ;           int16_t h1;                               ; Offset:    0
// CHECK: ;           int3 f3;                                  ; Offset:    2
// CHECK: ;           int16_t2 h2;                              ; Offset:   14
// CHECK: ;           int3 f3_1;                                ; Offset:   18
// CHECK: ;           int2 f2;                                  ; Offset:   30
// CHECK: ;           int16_t4 h4;                              ; Offset:   38
// CHECK: ;           int16_t2 h2_1;                            ; Offset:   46
// CHECK: ;           int16_t3 h3;                              ; Offset:   50
// CHECK: ;           double d1;                                ; Offset:   56
// CHECK: ;           int16_t3 h3_1;                            ; Offset:   64
// CHECK: ;           int i1;                                   ; Offset:   70
// CHECK: ;           double d2;                                ; Offset:   74
// CHECK: ;
// CHECK: ;       } f;                                          ; Offset:    0
// CHECK: ;
// CHECK: ;
// CHECK: ;   } f;                                              ; Offset:    0 Size:    82
// CHECK: ;
// CHECK: ; }

struct Foo {
  int16_t h1;
  int3 f3;

  int16_t2 h2;
  int3 f3_1;

  int2 f2;
  int16_t4 h4;

  int16_t2 h2_1;
  int16_t3 h3;

  double d1;
  int16_t3 h3_1;

  int    i1;
  double d2;
};


// CHECK: ; cbuffer b
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct b
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       struct struct.Bar
// CHECK: ;       {
// CHECK: ;
// CHECK: ;           int16_t h1;                               ; Offset:    0
// CHECK: ;           uint16_t h2;                              ; Offset:    2
// CHECK: ;           int16_t h3;                               ; Offset:    4
// CHECK: ;           uint16_t2 h4;                             ; Offset:    6
// CHECK: ;           int16_t3 h5;                              ; Offset:   10
// CHECK: ;           uint16_t3 h6;                             ; Offset:   16
// CHECK: ;           int16_t4 h7;                              ; Offset:   22
// CHECK: ;           int16_t h8;                               ; Offset:   30
// CHECK: ;           uint16_t4 h9;                             ; Offset:   32
// CHECK: ;           int16_t3 h10;                             ; Offset:   40
// CHECK: ;           uint16_t2 h11;                            ; Offset:   46
// CHECK: ;           int16_t3 h12;                             ; Offset:   50
// CHECK: ;           int16_t2 h13;                             ; Offset:   56
// CHECK: ;           uint16_t h14;                             ; Offset:   60
// CHECK: ;           int16_t h16;                              ; Offset:   62
// CHECK: ;           int16_t h17;                              ; Offset:   64
// CHECK: ;           uint16_t h18;                             ; Offset:   66
// CHECK: ;           int16_t h19;                              ; Offset:   68
// CHECK: ;           int16_t h20;                              ; Offset:   70
// CHECK: ;           int16_t h21;                              ; Offset:   72
// CHECK: ;           uint16_t h22;                             ; Offset:   74
// CHECK: ;           int16_t h23;                              ; Offset:   76
// CHECK: ;
// CHECK: ;       } b;                                          ; Offset:    0
// CHECK: ;
// CHECK: ;
// CHECK: ;   } b;                                              ; Offset:    0 Size:    78
// CHECK: ;
// CHECK: ; }

struct Bar {
  int16_t h1;
  uint16_t h2;
  int16_t h3;
  uint16_t2 h4;
  int16_t3 h5;

  uint16_t3 h6;
  int16_t4 h7;
  int16_t h8;

  uint16_t4 h9;
  int16_t3 h10;

  uint16_t2 h11;
  int16_t3 h12;
  int16_t2 h13;
  uint16_t  h14;

  int16_t h16;
  int16_t h17;
  uint16_t h18;
  int16_t h19;
  int16_t h20;
  int16_t h21;
  uint16_t h22;
  int16_t h23;

};

ConstantBuffer<Foo> f : register(b0);
ConstantBuffer<Bar> b : register(b1);

// CHECK: %dx.types.Handle = type { i8* }

int4 main() : SV_Target  {
  return f.h1 + f.f3.x
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %f_cbuffer, i32 0, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i32 @dx.op.cbufferLoad.i32(i32 58, %dx.types.Handle %f_cbuffer, i32 2, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
  + f.h2.x + f.h2.y + f.f3_1.z
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %f_cbuffer, i32 14, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %f_cbuffer, i32 16, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i32 @dx.op.cbufferLoad.i32(i32 58, %dx.types.Handle %f_cbuffer, i32 26, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
  + f.f2.x + f.h4.x + f.h4.y + f.h4.z + f.h4.w
  // CHECK: call i32 @dx.op.cbufferLoad.i32(i32 58, %dx.types.Handle %f_cbuffer, i32 30, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %f_cbuffer, i32 38, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %f_cbuffer, i32 40, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %f_cbuffer, i32 42, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %f_cbuffer, i32 44, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + f.h2_1.x + f.h2_1.y + f.h3.x + f.h3.y + f.h3.z
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %f_cbuffer, i32 46, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %f_cbuffer, i32 48, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %f_cbuffer, i32 50, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %f_cbuffer, i32 52, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %f_cbuffer, i32 54, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + f.d1 + f.h3_1.x
  // CHECK: call double @dx.op.cbufferLoad.f64(i32 58, %dx.types.Handle %f_cbuffer, i32 56, i32 8)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %f_cbuffer, i32 64, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + f.i1 + f.d2
  // CHECK: call i32 @dx.op.cbufferLoad.i32(i32 58, %dx.types.Handle %f_cbuffer, i32 70, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call double @dx.op.cbufferLoad.f64(i32 58, %dx.types.Handle %f_cbuffer, i32 74, i32 8)  ; CBufferLoad(handle,byteOffset,alignment)
  + b.h1 + b.h2 + b.h3 + b.h4.x + b.h5.y + b.h5.x + b.h5.y + b.h5.z +
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 0, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 2, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 4, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 6, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 12, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 10, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 14, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + b.h6.x + b.h6.y + b.h6.z + b.h7.x + b.h7.y + b.h7.z + b.h7.w + b.h8
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 16, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 18, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 20, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 22, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 24, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 26, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 28, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 30, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + b.h9.x + b.h9.y + b.h9.z + b.h9.w + b.h10.x + b.h10.y + b.h10.z
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 32, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 34, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 36, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 38, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 40, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 42, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 44, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + b.h11.x + b.h11.y + b.h12.x + b.h12.y + b.h12.z + b.h13.x + b.h13.y + b.h14
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 46, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 48, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 50, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 52, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 54, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 56, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 58, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 60, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + b.h16 + b.h17 + b.h18 + b.h19 + b.h20 + b.h21 + b.h22 + b.h23;
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 62, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 64, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 66, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 68, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 70, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 72, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 74, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call i16 @dx.op.cbufferLoad.i16(i32 58, %dx.types.Handle %b_cbuffer, i32 76, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
}
// RUN: %dxc -T vs_6_0 -E main -not_use_legacy_cbuf_load %s | FileCheck %s

// Tests the printed layout of constant buffers.

// CHECK: ; cbuffer _cbl
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct _cbl
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       struct struct.Struct
// CHECK: ;       {
// CHECK: ;
// CHECK: ;           int2 a;                                   ; Offset:    0
// CHECK: ;           struct struct.anon
// CHECK: ;           {
// CHECK: ;
// CHECK: ;               int b[2];                             ; Offset:    8
// CHECK: ;               int2 c;                               ; Offset:   16
// CHECK: ;               int2 d;                               ; Offset:   24
// CHECK: ;
// CHECK: ;           } s;                                      ; Offset:    8
// CHECK: ;
// CHECK: ;           int e;                                    ; Offset:   32
// CHECK: ;
// CHECK: ;       } cbl;                                        ; Offset:    0
// CHECK: ;
// CHECK: ;
// CHECK: ;   } _cbl;                                           ; Offset:    0 Size:    36
// CHECK: ;
// CHECK: ; }
// CHECK: ;
// CHECK: ; cbuffer cb
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct cb
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       struct struct.Struct
// CHECK: ;       {
// CHECK: ;
// CHECK: ;           int2 a;                                   ; Offset:    0
// CHECK: ;           struct struct.anon
// CHECK: ;           {
// CHECK: ;
// CHECK: ;               int b[2];                             ; Offset:    8
// CHECK: ;               int2 c;                               ; Offset:   16
// CHECK: ;               int2 d;                               ; Offset:   24
// CHECK: ;
// CHECK: ;           } s;                                      ; Offset:    8
// CHECK: ;
// CHECK: ;           int e;                                    ; Offset:   32
// CHECK: ;
// CHECK: ;       } cb;                                         ; Offset:    0
// CHECK: ;
// CHECK: ;
// CHECK: ;   } cb;                                             ; Offset:    0 Size:    36
// CHECK: ;
// CHECK: ; }

struct Struct
{
    int2 a;
    struct
    {
        int b[2];
        int2 c;
        int2 d;
    } s;
    int e;
};

cbuffer _cbl { Struct cbl; };
ConstantBuffer<Struct> cb;

int main() : OUT
{
    // CHECK: call i32 @dx.op.cbufferLoad.i32(i32 58, %dx.types.Handle %_cbl_cbuffer, i32 32, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
    // CHECK: call i32 @dx.op.cbufferLoad.i32(i32 58, %dx.types.Handle %cb_cbuffer, i32 32, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
    return cbl.e + cb.e;
}
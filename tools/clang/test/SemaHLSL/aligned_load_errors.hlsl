// RUN: %dxc -E main -T cs_6_2 -DTY=uint -DALIGN=4 %s -verify
// RUN: %dxc -E main -T cs_6_2 -DTY=uint3 -DALIGN=4 %s -verify
// RUN: %dxc -E main -T cs_6_2 -DTY=float4 -DALIGN=4 %s -verify
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=uint16_t -DALIGN=2 %s -verify
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=int64_t -DALIGN=8 %s -verify
// RUN: %dxc -E main -T cs_6_2 -enable-16bit-types -DTY=double -DALIGN=8 %s -verify

// Test alignment validation for AlignedLoad/AlignedStore with various types
// ALIGN is set to match the largest scalar type size for each type

ByteAddressBuffer srcbuf;
RWByteAddressBuffer dstbuf;

[numthreads(1,1,1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint offset = tid.x * ALIGN;
    int dynAlign = ALIGN;
    TY data;
    
    // Error: Non-constant alignment value
    // expected-error@+1 {{Alignment values require compile-time constant}}
    data = srcbuf.AlignedLoad<TY>(offset, dynAlign);
    
    // expected-error@+1 {{Alignment values require compile-time constant}}
    dstbuf.AlignedStore<TY>(offset, dynAlign, data);
    offset += ALIGN;
    
    // Error: Non-power-of-two alignment (ALIGN - 1)
    // When ALIGN=4, this is 3 (not power of 2)
    // When ALIGN=2, this is 1 (power of 2, but less than scalar size)
    // When ALIGN=8, this is 7 (not power of 2)
#if ALIGN == 2
    // For 16-bit types: ALIGN=2, ALIGN-1=1
    // 1 is power-of-two, but less than scalar size
    // expected-error@+1 {{Alignment parameter of 1 bytes must be >= the largest scalar type size 2 bytes}}
    data = srcbuf.AlignedLoad<TY>(offset, ALIGN - 1);
    
    // expected-error@+1 {{Alignment parameter of 1 bytes must be >= the largest scalar type size 2 bytes}}
    dstbuf.AlignedStore<TY>(offset, ALIGN - 1, data);
#else
    // For 32-bit and 64-bit types: ALIGN-1 is not power-of-two
    // expected-error@+1 {{Alignment values require compile-time constant power-of-two}}
    data = srcbuf.AlignedLoad<TY>(offset, ALIGN - 1);
    
    // expected-error@+1 {{Alignment values require compile-time constant power-of-two}}
    dstbuf.AlignedStore<TY>(offset, ALIGN - 1, data);
#endif
    offset += ALIGN;
    
    // Error: Alignment greater than 4096
    // expected-error@+1 {{Alignment values require compile-time constant power-of-two values that are >= largest scalar type size and <= 4096}}
    data = srcbuf.AlignedLoad<TY>(offset, 4096 * 2);
    
    // expected-error@+1 {{Alignment values require compile-time constant power-of-two values that are >= largest scalar type size and <= 4096}}
    dstbuf.AlignedStore<TY>(offset, 4096 * 2, data);
    offset += ALIGN;
    
    // Error: Alignment less than largest scalar type size (ALIGN / 2)
    // For ALIGN=4: ALIGN/2=2, error shows "2 bytes must be >= 4 bytes"
    // For ALIGN=2: ALIGN/2=1, error shows "1 bytes must be >= 2 bytes"
    // For ALIGN=8: ALIGN/2=4, error shows "4 bytes must be >= 8 bytes"
#if ALIGN == 4
    // expected-error@+1 {{Alignment parameter of 2 bytes must be >= the largest scalar type size 4 bytes}}
    data = srcbuf.AlignedLoad<TY>(offset, ALIGN / 2);
    
    // expected-error@+1 {{Alignment parameter of 2 bytes must be >= the largest scalar type size 4 bytes}}
    dstbuf.AlignedStore<TY>(offset, ALIGN / 2, data);
#elif ALIGN == 2
    // expected-error@+1 {{Alignment parameter of 1 bytes must be >= the largest scalar type size 2 bytes}}
    data = srcbuf.AlignedLoad<TY>(offset, ALIGN / 2);
    
    // expected-error@+1 {{Alignment parameter of 1 bytes must be >= the largest scalar type size 2 bytes}}
    dstbuf.AlignedStore<TY>(offset, ALIGN / 2, data);
#elif ALIGN == 8
    // expected-error@+1 {{Alignment parameter of 4 bytes must be >= the largest scalar type size 8 bytes}}
    data = srcbuf.AlignedLoad<TY>(offset, ALIGN / 2);
    
    // expected-error@+1 {{Alignment parameter of 4 bytes must be >= the largest scalar type size 8 bytes}}
    dstbuf.AlignedStore<TY>(offset, ALIGN / 2, data);
#endif
}

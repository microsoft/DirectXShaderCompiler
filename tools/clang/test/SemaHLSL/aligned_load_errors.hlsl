// RUN: %dxc -E main -T cs_6_2 -DTY=uint -DALIGN=4 %s -verify
// RUN: %dxc -E main -T cs_6_2 -DTY=uint -DALIGN=4 -DSRCRW %s -verify

// Test error conditions for AlignedLoad/AlignedStore

#ifdef SRCRW
RWByteAddressBuffer srcbuf;
RWByteAddressBuffer dstbuf;

RWTexture2D srctex;
RWTexture2D dsttex;
#else
ByteAddressBuffer srcbuf;
RWByteAddressBuffer dstbuf;

Texture2D srctex;
RWTexture2D dsttex;
#endif

[numthreads(1,1,1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint offset = tid.x * ALIGN;
    int dynAlign = ALIGN;
    TY data;
    
    // expected-error@+1 {{Alignment values require compile-time constant}}
    data = srcbuf.AlignedLoad<TY>(offset, dynAlign);
    
    // expected-error@+1 {{Alignment values require compile-time constant}}
    dstbuf.AlignedStore<TY>(offset, dynAlign, data);
    offset += ALIGN;
    
    // expected-error@+1 {{Alignment values require compile-time constant power-of-two}}
    data = srcbuf.AlignedLoad<TY>(offset, ALIGN - 1);
    
    // expected-error@+1 {{Alignment values require compile-time constant power-of-two}}
    dstbuf.AlignedStore<TY>(offset, ALIGN - 1, data);
    offset += ALIGN;
    
    // expected-error@+1 {{Alignment values require compile-time constant power-of-two values that are >= largest scalar type size and <= 4096}}
    data = srcbuf.AlignedLoad<TY>(offset, 4096 * 2);
    
    // expected-error@+1 {{Alignment values require compile-time constant power-of-two values that are >= largest scalar type size and <= 4096}}
    dstbuf.AlignedStore<TY>(offset, 4096 * 2, data);
    offset += ALIGN;
    
    // expected-error@+1 {{Alignment parameter must be >= the largest scalar type size}}
    data = srcbuf.AlignedLoad<TY>(offset, ALIGN / 2);
    
    // expected-error@+1 {{Alignment parameter must be >= the largest scalar type size}}
    dstbuf.AlignedStore<TY>(offset, ALIGN / 2, data);
    offset += ALIGN;
    
    // expected-error@+1 {{AlignedLoad functions cannot be used with}}
    data = srctex.AlignedLoad<TY>(offset, ALIGN);
    
    // expected-error@+1 {{AlignedStore functions cannot be used with}}
    dsttex.AlignedStore<TY>(offset, ALIGN, data);
}


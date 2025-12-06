// RUN: %dxc -T cs_6_0 -E main %s -verify

// Test error conditions for AlignedLoad/AlignedStore

ByteAddressBuffer buf;
RWByteAddressBuffer rwbuf;
Texture2D tex;
RWTexture2D<float4> rwtex;

[numthreads(1,1,1)]
void main() {
    // Test E1001: Unsupported buffer type
    // expected-error@+1 {{AlignedLoad/AlignedStore functions cannot be used with}}
    uint data = tex.AlignedLoad<uint>(0, 16);
    
    // Test E1002: Invalid alignment - not a constant
    int dynAlign = 16;
    // expected-error@+1 {{Alignment values require compile-time constant}}
    data += buf.AlignedLoad<uint>(0, dynAlign);
    
    // Test E1002: Invalid alignment - not a power of two
    // expected-error@+1 {{Alignment values require compile-time constant power-of-two}}
    data += buf.AlignedLoad<uint>(0, 3);
    
    // Test E1002: Invalid alignment - greater than 4096
    // expected-error@+1 {{Alignment values require compile-time constant power-of-two values that are >= largest scalar type size and <= 4096}}
    data += buf.AlignedLoad<uint>(0, 8192);
    
    // Test E1003: Alignment less than scalar type size
    // expected-error@+1 {{Alignment parameter of 2 bytes must be >= the largest scalar type size 4 bytes}}
    data += buf.AlignedLoad<uint>(0, 2);
    
    // Test E1003: Alignment less than 64-bit type size
    // expected-error@+1 {{Alignment parameter of 4 bytes must be >= the largest scalar type size 8 bytes}}
    uint64_t data6 = buf.AlignedLoad<uint64_t>(0, 4);
    data += (uint)data6;
    
    // Test E1003: Alignment less than double type size
    // expected-error@+1 {{Alignment parameter of 4 bytes must be >= the largest scalar type size 8 bytes}}
    double data7 = buf.AlignedLoad<double>(0, 4);
    data += (uint)data7;
    
    // Valid cases - no errors expected
    data += buf.AlignedLoad<uint>(0, 4);   // minimum valid alignment
    data += buf.AlignedLoad<uint>(0, 16);  // higher alignment
    data += buf.AlignedLoad<uint>(0, 4096); // maximum alignment
    uint64_t data11 = buf.AlignedLoad<uint64_t>(0, 8); // correct 64-bit alignment
    data += (uint)data11;

    // Use the values to prevent DCE
    rwbuf.Store<uint64_t>(0, data);
}


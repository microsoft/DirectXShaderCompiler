// RUN: %dxc -E main -T cs_6_2 %s -verify

// Test that AlignedLoad/AlignedStore only work with ByteAddressBuffer and RWByteAddressBuffer

ByteAddressBuffer bab;
RWByteAddressBuffer rwbab;

// Invalid buffer types
Texture2D<float4> tex2d;
RWTexture2D<float4> rwtex2d;
Buffer<float4> typedBuffer;
RWBuffer<float4> rwTypedBuffer;
StructuredBuffer<float4> structuredBuffer;
RWStructuredBuffer<float4> rwStructuredBuffer;
AppendStructuredBuffer<float4> appendBuffer;
ConsumeStructuredBuffer<float4> consumeBuffer;

[numthreads(1,1,1)]
void main()
{
    uint offset = 0;
    uint data;
    
    // Valid - these should work
    data = bab.AlignedLoad<uint>(offset, 4);
    rwbab.AlignedStore<uint>(offset, 4, data);
    
    // Invalid buffer types - Texture2D
    // expected-error@+2 {{no member named 'AlignedLoad' in 'Texture2D<vector<float, 4> >'}}
    // expected-error@+1 {{unexpected type name 'uint': expected expression}}
    data = tex2d.AlignedLoad<uint>(offset, 4);
    
    // expected-error@+2 {{no member named 'AlignedStore' in 'RWTexture2D<vector<float, 4> >'}}
    // expected-error@+1 {{unexpected type name 'uint': expected expression}}
    rwtex2d.AlignedStore<uint>(offset, 4, data);
    
    // Invalid buffer types - Buffer (typed)
    // expected-error@+2 {{no member named 'AlignedLoad' in 'Buffer<vector<float, 4> >'}}
    // expected-error@+1 {{unexpected type name 'uint': expected expression}}
    data = typedBuffer.AlignedLoad<uint>(offset, 4);
    
    // expected-error@+2 {{no member named 'AlignedStore' in 'RWBuffer<vector<float, 4> >'}}
    // expected-error@+1 {{unexpected type name 'uint': expected expression}}
    rwTypedBuffer.AlignedStore<uint>(offset, 4, data);
    
    // Invalid buffer types - StructuredBuffer
    // expected-error@+2 {{no member named 'AlignedLoad' in 'StructuredBuffer<vector<float, 4> >'}}
    // expected-error@+1 {{unexpected type name 'uint': expected expression}}
    data = structuredBuffer.AlignedLoad<uint>(offset, 4);
    
    // expected-error@+2 {{no member named 'AlignedStore' in 'RWStructuredBuffer<vector<float, 4> >'}}
    // expected-error@+1 {{unexpected type name 'uint': expected expression}}
    rwStructuredBuffer.AlignedStore<uint>(offset, 4, data);
    
    // Invalid buffer types - AppendStructuredBuffer
    // expected-error@+2 {{no member named 'AlignedStore' in 'AppendStructuredBuffer<vector<float, 4> >'}}
    // expected-error@+1 {{unexpected type name 'uint': expected expression}}
    appendBuffer.AlignedStore<uint>(offset, 4, data);
    
    // Invalid buffer types - ConsumeStructuredBuffer
    // expected-error@+2 {{no member named 'AlignedLoad' in 'ConsumeStructuredBuffer<vector<float, 4> >'}}
    // expected-error@+1 {{unexpected type name 'uint': expected expression}}
    data = consumeBuffer.AlignedLoad<uint>(offset, 4);
}


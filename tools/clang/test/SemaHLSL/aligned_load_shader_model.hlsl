// RUN: %dxc -E main -T cs_6_0 %s -verify
// RUN: %dxc -E main -T cs_6_1 %s -verify

// Test that AlignedLoad/AlignedStore require SM 6.2+

ByteAddressBuffer inputBuffer;
RWByteAddressBuffer outputBuffer;

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    // expected-error@+1 {{intrinsic AlignedLoad potentially used by ''AlignedLoad<uint, unsigned int, unsigned int>'' requires shader model 6.2 or greater}}
    uint value = inputBuffer.AlignedLoad<uint>(0, 4);
    
    // expected-error@+1 {{intrinsic AlignedStore potentially used by ''AlignedStore<void, unsigned int, unsigned int, uint>'' requires shader model 6.2 or greater}}
    outputBuffer.AlignedStore<uint>(0, 4, value);
}


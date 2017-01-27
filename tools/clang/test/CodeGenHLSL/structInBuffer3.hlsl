// RUN: %dxc -E main  -T cs_6_0 %s | FileCheck %s

// CHECK: all template type components must have the same type

struct Foo {
    int a;
    int b;
    float c;
    int d;
};

Buffer<Foo> inputs : register(t1);
RWBuffer< int > g_Intensities : register(u1);

groupshared Foo sharedData;

#ifdef DX12
[RootSignature("DescriptorTable(UAV(u1, numDescriptors=1), SRV(t1), visibility=SHADER_VISIBILITY_ALL)")]
#endif
[ numthreads( 64, 2, 2 ) ]
void main( uint GI : SV_GroupIndex)
{
	sharedData = inputs[GI];
	int rtn;
	InterlockedAdd(sharedData.d, g_Intensities[GI], rtn);
	g_Intensities[GI] = rtn + sharedData.d;
}
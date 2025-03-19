// RUN: %dxc -Tcs_6_6 -verify %s

struct MyStruct
{
	float f;
};

void TestSamplerDescriptorHeap()
{
	SamplerState s1 = SamplerDescriptorHeap[0];
	SamplerComparisonState s2 = SamplerDescriptorHeap[0];
																	    
	Texture1D<float>   t1 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'Texture1D<float>' with an rvalue of type 'const .Sampler'}}
	RWTexture1D<float> t2 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'RWTexture1D<float>' with an rvalue of type 'const .Sampler'}}
	Texture2D<float>   t3 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'Texture2D<float>' with an rvalue of type 'const .Sampler'}}
	RWTexture2D<float> t4 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'RWTexture2D<float>' with an rvalue of type 'const .Sampler'}}
	Texture3D<float>   t5 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'Texture3D<float>' with an rvalue of type 'const .Sampler'}}
	RWTexture3D<float> t6 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'RWTexture3D<float>' with an rvalue of type 'const .Sampler'}}
	
	Texture2DMS<float>   t7 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'Texture2DMS<float>' with an rvalue of type 'const .Sampler'}}
	RWTexture2DMS<float> t8 = SamplerDescriptorHeap[0];	// expected-error{{cannot initialize a variable of type 'RWTexture2DMS<float>' with an rvalue of type 'const .Sampler'}}
	TextureCube<float>   t9 = SamplerDescriptorHeap[0];	// expected-error{{cannot initialize a variable of type 'TextureCube<float>' with an rvalue of type 'const .Sampler'}}
	
	Texture1DArray<float>   t10 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'Texture1DArray<float>' with an rvalue of type 'const .Sampler'}}
	RWTexture1DArray<float> t11 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'RWTexture1DArray<float>' with an rvalue of type 'const .Sampler'}}
	Texture2DArray<float>   t12 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'Texture2DArray<float>' with an rvalue of type 'const .Sampler'}}
	RWTexture2DArray<float> t13 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'RWTexture2DArray<float>' with an rvalue of type 'const .Sampler'}}
	Texture2DMSArray<float> t14 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'Texture2DMSArray<float>' with an rvalue of type 'const .Sampler'}}
	RWTexture2DMSArray<float> t15 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'RWTexture2DMSArray<float>' with an rvalue of type 'const .Sampler'}}
	TextureCubeArray<float>   t16 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'TextureCubeArray<float>' with an rvalue of type 'const .Sampler'}}
	
	FeedbackTexture2D<SAMPLER_FEEDBACK_MIN_MIP> t17	= SamplerDescriptorHeap[0];  // expected-error{{cannot initialize a variable of type 'FeedbackTexture2D<SAMPLER_FEEDBACK_MIN_MIP>' with an rvalue of type 'const .Sampler'}}
	FeedbackTexture2DArray<SAMPLER_FEEDBACK_MIN_MIP> t18 = SamplerDescriptorHeap[0];  // expected-error{{cannot initialize a variable of type 'FeedbackTexture2DArray<SAMPLER_FEEDBACK_MIN_MIP>' with an rvalue of type 'const .Sampler'}}
	
	RasterizerOrderedTexture1D<float> t19 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'RasterizerOrderedTexture1D<float>' with an rvalue of type 'const .Sampler'}}
	RasterizerOrderedTexture2D<float> t20 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'RasterizerOrderedTexture2D<float>' with an rvalue of type 'const .Sampler'}}
	RasterizerOrderedTexture3D<float> t21 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'RasterizerOrderedTexture3D<float>' with an rvalue of type 'const .Sampler'}}
	RasterizerOrderedTexture1DArray<float> t22 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'RasterizerOrderedTexture1DArray<float>' with an rvalue of type 'const .Sampler'}}
	RasterizerOrderedTexture2DArray<float> t23 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'RasterizerOrderedTexture2DArray<float>' with an rvalue of type 'const .Sampler'}}
	
	ByteAddressBuffer b1   = SamplerDescriptorHeap[0];				// expected-error{{cannot initialize a variable of type 'ByteAddressBuffer' with an rvalue of type 'const .Sampler'}}
	RWByteAddressBuffer b2 = SamplerDescriptorHeap[0]; 				// expected-error{{cannot initialize a variable of type 'RWByteAddressBuffer' with an rvalue of type 'const .Sampler'}}
	StructuredBuffer<float>   b3 = SamplerDescriptorHeap[0]; 		// expected-error{{cannot initialize a variable of type 'StructuredBuffer<float>' with an rvalue of type 'const .Sampler'}}
	RWStructuredBuffer<float> b4 = SamplerDescriptorHeap[0]; 		// expected-error{{cannot initialize a variable of type 'RWStructuredBuffer<float>' with an rvalue of type 'const .Sampler'}}
	AppendStructuredBuffer<float> b5 = SamplerDescriptorHeap[0]; 	// expected-error{{cannot initialize a variable of type 'AppendStructuredBuffer<float>' with an rvalue of type 'const .Sampler'}}
	ConsumeStructuredBuffer<float> b6 = SamplerDescriptorHeap[0]; 	// expected-error{{cannot initialize a variable of type 'ConsumeStructuredBuffer<float>' with an rvalue of type 'const .Sampler'}}
	Buffer<float> b7 = SamplerDescriptorHeap[0];					// expected-error{{cannot initialize a variable of type 'Buffer<float>' with an rvalue of type 'const .Sampler'}}
	RWBuffer<float> b8 = SamplerDescriptorHeap[0]; 					// expected-error{{cannot initialize a variable of type 'RWBuffer<float>' with an rvalue of type 'const .Sampler'}}
	RasterizerOrderedBuffer<float> b9 = SamplerDescriptorHeap[0]; 	// expected-error{{cannot initialize a variable of type 'RasterizerOrderedBuffer<float>' with an rvalue of type 'const .Sampler'}}
	RasterizerOrderedByteAddressBuffer b10 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'RasterizerOrderedByteAddressBuffer' with an rvalue of type 'const .Sampler'}}
	RasterizerOrderedStructuredBuffer<float> b11 = SamplerDescriptorHeap[0];  // expected-error{{cannot initialize a variable of type 'RasterizerOrderedStructuredBuffer<float>' with an rvalue of type 'const .Sampler'}}
	
	ConstantBuffer<MyStruct> cb0 = SamplerDescriptorHeap[0];  // expected-error{{cannot initialize a variable of type 'ConstantBuffer<MyStruct>' with an rvalue of type 'const .Sampler'}}
	TextureBuffer<MyStruct>  tb0 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'TextureBuffer<MyStruct>' with an rvalue of type 'const .Sampler'}}
	
	RaytracingAccelerationStructure as0 = SamplerDescriptorHeap[0]; // expected-error{{cannot initialize a variable of type 'RaytracingAccelerationStructure' with an rvalue of type 'const .Sampler'}}
}

void TestResourceDescriptorHeap()
{
	SamplerState s1 = ResourceDescriptorHeap[0];	// expected-error{{cannot initialize a variable of type 'SamplerState' with an rvalue of type 'const .Resource'}}
	SamplerComparisonState s2 = ResourceDescriptorHeap[0];	// expected-error{{cannot initialize a variable of type 'SamplerComparisonState' with an rvalue of type 'const .Resource'}}
	
	Texture1D<float>   t1 = ResourceDescriptorHeap[0]; 
	RWTexture1D<float> t2 = ResourceDescriptorHeap[0]; 
	Texture2D<float>   t3 = ResourceDescriptorHeap[0]; 
	RWTexture2D<float> t4 = ResourceDescriptorHeap[0]; 
	Texture3D<float>   t5 = ResourceDescriptorHeap[0];
	RWTexture3D<float> t6 = ResourceDescriptorHeap[0];
	
	Texture2DMS<float>   t7 = ResourceDescriptorHeap[0];
	RWTexture2DMS<float> t8 = ResourceDescriptorHeap[0];
	TextureCube<float>   t9 = ResourceDescriptorHeap[0];
	
	Texture1DArray<float>   t10 = ResourceDescriptorHeap[0];
	RWTexture1DArray<float> t11 = ResourceDescriptorHeap[0];
	Texture2DArray<float>   t12 = ResourceDescriptorHeap[0];
	RWTexture2DArray<float> t13 = ResourceDescriptorHeap[0];
	Texture2DMSArray<float> t14 = ResourceDescriptorHeap[0];
	RWTexture2DMSArray<float> t15 = ResourceDescriptorHeap[0];
	TextureCubeArray<float>   t16 = ResourceDescriptorHeap[0];
	
	FeedbackTexture2D<SAMPLER_FEEDBACK_MIN_MIP> t17	= ResourceDescriptorHeap[0]; 
	FeedbackTexture2DArray<SAMPLER_FEEDBACK_MIN_MIP> t18 = ResourceDescriptorHeap[0]; 
	
	RasterizerOrderedTexture1D<float> t19 = ResourceDescriptorHeap[0]; 
	RasterizerOrderedTexture2D<float> t20 = ResourceDescriptorHeap[0]; 
	RasterizerOrderedTexture3D<float> t21 = ResourceDescriptorHeap[0]; 
	RasterizerOrderedTexture1DArray<float> t22 = ResourceDescriptorHeap[0]; 
	RasterizerOrderedTexture2DArray<float> t23 = ResourceDescriptorHeap[0]; 
	
	ByteAddressBuffer b1   = ResourceDescriptorHeap[0]; 
	RWByteAddressBuffer b2 = ResourceDescriptorHeap[0]; 
	StructuredBuffer<float>   b3 = ResourceDescriptorHeap[0]; 
	RWStructuredBuffer<float> b4 = ResourceDescriptorHeap[0]; 
	AppendStructuredBuffer<float> b5 = ResourceDescriptorHeap[0]; 
	ConsumeStructuredBuffer<float> b6 = ResourceDescriptorHeap[0]; 
	Buffer<float> b7 = ResourceDescriptorHeap[0];
	RWBuffer<float> b8 = ResourceDescriptorHeap[0]; 
	RasterizerOrderedBuffer<float> b9 = ResourceDescriptorHeap[0]; 
	RasterizerOrderedByteAddressBuffer b10 = ResourceDescriptorHeap[0]; 
	RasterizerOrderedStructuredBuffer<float> b11 = ResourceDescriptorHeap[0]; 
	
	ConstantBuffer<MyStruct> cb0 = ResourceDescriptorHeap[0]; 
	TextureBuffer<MyStruct>  tb0 = ResourceDescriptorHeap[0]; 
	
	RaytracingAccelerationStructure as0 = ResourceDescriptorHeap[0]; 
}

[numthreads(1, 1, 1)]
void main() 
{
	TestSamplerDescriptorHeap();
	TestResourceDescriptorHeap();
}
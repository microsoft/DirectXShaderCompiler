// RUN: %dxc -T lib_6_8 -verify %s
// test that clipplanes emits an error when no shader attribute is present,
// to warn the user that the deprecated behavior of inferring the shader kind
// is no longer taking place.

cbuffer ClipPlaneConstantBuffer 
{
       float4 clipPlane1;
       float4 clipPlane2;
};

struct OutputType{
	float4 f4 : SV_Position;
};


[clipplanes(clipPlane1,clipPlane2)] /* expected-warning{{attribute clipplanes ignored without accompanying shader attribute}} */
OutputType main()
{
	float4 outputData = clipPlane1 + clipPlane2;
	OutputType output = {outputData};
	return output;
}
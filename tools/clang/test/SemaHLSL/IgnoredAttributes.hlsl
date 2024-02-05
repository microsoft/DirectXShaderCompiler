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


[domain("quad")] /* expected-warning{{attribute domain ignored without accompanying shader attribute}} */
[partitioning("integer")] /* expected-warning{{attribute partitioning ignored without accompanying shader attribute}} */
[outputtopology("triangle_cw")] /* expected-warning{{attribute outputtopology ignored without accompanying shader attribute}} */
[outputcontrolpoints(16)] /* expected-warning{{attribute outputcontrolpoints ignored without accompanying shader attribute}} */
[patchconstantfunc("PatchFoo")] /* expected-warning{{attribute patchconstantfunc ignored without accompanying shader attribute}} */
void HSMain( 
              uint i : SV_OutputControlPointID,
              uint PatchID : SV_PrimitiveID )
{
    
}


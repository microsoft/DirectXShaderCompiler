// Note: This file is used for CompilerTest::CompileHsWithCompilerInterface3. Regression test for #2863

/// <summary>	The scene data. </summary>
cbuffer SceneData : register(b0)
{
	/// <summary>	The prior World-View-Projection matrix. </summary>
    float4x4 ViewProjectionPrior;

	/// <summary>	The View-Projection matrix. </summary>
    float4x4 ViewProjection;

	/// <summary>	The prior world matrix. </summary>
    float4x4 WorldPrior;

	/// <summary>	The world matrix. </summary>
    float4x4 World;

	/// <summary>	The view matrix. </summary>
    float4x4 View;

	/// <summary>	The view rotation. </summary>
    float4x4 ViewRotation;

	/// <summary>	The projection matrix. </summary>
    float4x4 Projection;

	/// <summary>	The camera position. </summary>
    float3 CamPos = float3(0.0, 0.0, 0.0);

	/// <summary>	The camera direction (normalized). </summary>
    float3 CamDir = float3(0.0, 0.0, 1.0);

	/// <summary>	The sun light direction (inverse, normalized). </summary>
    float3 LightDir = float3(0.0f, 1.0f, 0.0f);

	/// <summary>	Size of the Render Targets in pixel. </summary>
    float2 TargetSize = float2(1920.0, 1080.0);

	/// <summary>	The motion scale. </summary>
    float2 MotionScale = float2(32.0, 32.0);

	/// <summary>	The frame time in ms. </summary>
    float FrameTime = 3.4;
};

// Contains all the material properties.
cbuffer MaterialProperties : register(b1)
{
	// The Object Matrix.
    float4x4 matObjectMatrix;

	/// <summary>	The matrix for sky environment. </summary>
    float4x4 matEnvironmentMatrix;

	// The Ambient Color (Additive) of the Surface.
	// If given, HAS_AMBIENT_COLOR is defined. Default is black.
    float4 matAmbientColor = float4(0.0, 0.0, 0.0, 1.0);

	// The Diffuse Color (Albedo) of the Surface.
	// If given, HAS_DIFFUSE_COLOR is defined. Default is white. 
    float4 matDiffuseColor = float4(1.0, 1.0, 1.0, 1.0);

	// The Emissive Color (Minimum) of the Surface.
	// If given, HAS_EMISSIVE_COLOR is defined. Default is black.
    float4 matEmissiveColor = float4(0.0, 0.0, 0.0, 1.0);

	// The Reflection of the Surface. Default is 0.
	// If given, HAS_REFLECTION is defined. Default is 0.
    float matReflection = 0.0;
	
	// The Refraction Index of the Surface. Default is 1.
	// If given, HAS_REFRACTION_INDEX is defined. Default is 1.
    float matRefractionIndex = 1.0;

	// The Opacity of the Surface.
	// If given, HAS_OPACITY_SCALE is defined. Default is 1.
    float matOpacity = 1.0;

	// The Bump Scale for Normal Maps.
	// If given, HAS_BUMP_SCALE is defined. Default is 1.
    float matBumpScale = 1.0;

	// The Roughness of the Surface for the PBR-Shader.
	// If given, HAS_ROUGHNESS_SCALE is defined. Default is 1.
    float matRoughness = 1.0;

	// The Metalliness of the Surface for the PBR-Shader.
	// If given, HAS_METALNESS_SCALE is defined. Default is 0.
    float matMetalness = 0.0;

	// The Specular Metalliness of the Surface for the PBR-Shader.
	// If given, HAS_SPECULAR_METALNESS_SCALE is defined. Default is 0.
    float matSpecularMetalness = 1.0;

	// The Depth of Field Scale for the Post-Processing Filter.
	// If given by object, HAS_DEPTH_OF_FIELD_SCALE is defined. Default is 1.
    float matDepthOfField = 1.0;

	// The Motion Blue Scale for the Post-Processing Filter.
	// If given, HAS_MOTION_BLUR_SCALE is defined. Default is 1.
    float matMotionBlur = 1.0;

	// The Ambient Occlusion Scale for the Post-Processing Filter.
	// If given, HAS_AMBIENT_OCCLUSION_SCALE is defined. Default is 1.
    float matAmbientOcclusion = 1.0;

	/// <summary>	The occlusion scale. </summary>
    float matOcclusionScale = 1.0;
	
	// For Relief Mapping and Tessellation, the height scale.
	// If given, HAS_HEIGHT_SCALE is defined. Default is 20cm.
    float matHeightScale = 0.02f;

	// For Relief Mapping, the applied maximum distance.
	// If given, HAS_REFLIEF_MAX_DISTANCE is defined. Default is 300.
    float matMaxReliefDistance = 300.0;

	// For Relief Mapping, the minimum number of samples.
	// If given, HAS_RELIEF_MIN_SAMPLES is defined. Default is 1.
    int matMinReliefSamples = 4;

	// For Relief Mapping, the maximum number of samples.
	// If given, HAS_RELIEF_MAX_SAMPLES is defined. Default is 20.
    int matMaxReliefSamples = 20;

	// For Tessellation, the minimum Tessellation Factor for the inner triangle.
	// If given, HAS_MIN_TESSELLATION_FACTOR is defined. Default is 1.
    float matMinTessFactorInner = 1.0;

	// For Tessellation, the minimum Tessellation Factor for the outer edge.
	// If given, HAS_MIN_TESSELLATION_FACTOR is defined. Default is 1.
    float matMinTessFactorOuter = 1.0;

	// For Tessellation, the maximum Tessellation Factor for the inner triangle.
	// If given, HAS_MAX_TESSELLATION_FACTOR is defined. Default is 24.
    float matMaxTessFactorInner = 24.0;

	// For Tessellation, the maximum Tessellation Factor for the outer edge.
	// If given, HAS_MIN_TESSELLATION_FACTOR is defined. Default is 1.
    float matMaxTessFactorOuter = 24.0;

	// For Tessellation, the applied maximum distance.
	// If given, HAS_TESSELLATION_MAX_DISTANCE is defined. Default is 500.
    float matMaxTessDistance = 150.0;

	// The Specular Power.
	// If given, HAS_SPECULAR_POWER is defined. Default is 0.
    float matSpecularPower = 0.0f;

	// The specular Exponent.
	// If given, HAS_SPECULAR_EXPONENT is defined. Default is 8.
    float matSpecularExponent = 8.0f;

	/// <summary>	True if tessellation is enabled. </summary>
    bool matTessellation = false;

};

struct HULL_INPUT
{
    float4 ScreenPos : SV_POSITION;

	// The object space vertex position.
    float3 Position : WORLDPOS;

#if defined(HAS_NORMALS)

	// The normal in world-space.
	float3		NormalWS : NORMALWS;

#endif

#if defined(HAS_TANGENTS)

	// The tangent in world-space.
	float4		TangentWS : TANGENTWS;

#endif

#if defined(HAS_UV_CHANNEL)

	// The first texture coordinate set.
	float2		TexCoord : TEXCOORD0;

#endif

#if defined(HAS_SECOND_UV_CHANNEL)

	// The second texture coordinate set.
	float2		TexCoord2 : TEXCOORD1;

#endif

	// The transformation of this object (instance data).
    float4x4 Transform : TRANSFORM;
	
};

struct HULL_CONSTANT_OUTPUT
{
    float Edges[3] : SV_TessFactor;

    float Inside : SV_InsideTessFactor;

	// The transformation of this object (instance data).
    float4x4 Transform : TRANSFORM;
};

/// <summary>	The hull shader output. </summary>
struct HULL_OUTPUT
{
	// The position (xyz).
    float3 Position : POSITION0;

	#if defined(HAS_NORMALS)

		// The normal in object-space.
		float3		Normal : NORMAL;

	#endif

	#if defined(HAS_TANGENTS)

		// The tangent in world-space.
		float4		Tangent : TANGENT;

	#endif

	#if defined(HAS_UV_CHANNEL)

		// The first texture coordinate set.
		float2		TexCoord : UV_CHANNEL0;

	#endif

	#if defined(HAS_SECOND_UV_CHANNEL)

		// The second texture coordinate set.
		float2		TexCoord2 : UV_CHANNEL1;

	#endif
};

const HULL_CONSTANT_OUTPUT hs_constant_function(InputPatch<HULL_INPUT, 3> Patch)
{
    HULL_CONSTANT_OUTPUT Output;

#if defined(HAS_INSTANCE_DATA)
	Output.Transform = Patch[0].Transform;
#else
    Output.Transform = mul(World, matObjectMatrix);
    
#endif

    const float3 WorldPosA = mul(Output.Transform, float4(Patch[0].Position.xyz, 1.0f)).xyz;
    const float DistanceToCam = max(20.0f, length(WorldPosA - CamPos)) - 20.0f;

    const float Factor = clamp(0.0f, 1.0f, DistanceToCam / matMaxTessDistance);
    const float TessellationFactorOuter = lerp(matMaxTessFactorOuter, matMinTessFactorOuter, Factor);
    const float TessellationFactorInner = lerp(matMaxTessFactorInner, matMinTessFactorInner, Factor);

    Output.Edges[0] = TessellationFactorOuter;
    Output.Edges[1] = TessellationFactorOuter;
    Output.Edges[2] = TessellationFactorOuter;

    Output.Inside = TessellationFactorOuter;

    return Output;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("hs_constant_function")]
const HULL_OUTPUT hs_main(InputPatch<HULL_INPUT, 3> Patch, uint PointID : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID)
{
    HULL_OUTPUT Output;

    Output.Position = Patch[PointID].Position;

#ifdef HAS_NORMALS
	Output.Normal = Patch[PointID].NormalWS;
#endif

#ifdef HAS_TANGENTS
	Output.Tangent = Patch[PointID].TangentWS;
#endif

#ifdef HAS_UV_CHANNEL
	Output.TexCoord = Patch[PointID].TexCoord;
#endif

#ifdef HAS_SECOND_UV_CHANNEL
	Output.TexCoord2 = Patch[PointID].TexCoord2;
#endif
	
    return Output;
}

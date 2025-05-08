#pragma once
// clang-format off

#if !defined(D3D12_PREVIEW_SDK_VERSION) || D3D12_PREVIEW_SDK_VERSION < 717

#ifdef __ID3D12GraphicsCommandList10_INTERFACE_DEFINED__
#define HAVE_COOPVEC_API 1

// This file contains the definitions of the D3D12 cooperative vector API.
// It is used to test the cooperative vector API on older SDKs.

constexpr int D3D12_FEATURE_D3D12_OPTIONS_EXPERIMENTAL	= 9;
constexpr int D3D12_FEATURE_COOPERATIVE_VECTOR = 11;

// --------------------------------------------------------------------------------------------------------------------------------
// Experimental Feature: D3D12CooperativeVectorExperiment
//
// Use with D3D12CooperativeVectorExperiment to enable cooperative vector experimental feature.
//
// Enabling D3D12CooperativeVectorExperiment needs no configuration struct, pass NULL in the pConfigurationStructs array.
//
// --------------------------------------------------------------------------------------------------------------------------------
static const UUID D3D12CooperativeVectorExperiment = { /* 384748be-cca5-471e-a125-5cc997e04d39 */
    0x384748be,
    0xcca5,
    0x471e,
    {0xa1, 0x25, 0x5c, 0xc9, 0x97, 0xe0, 0x4d, 0x39}
};

/* interface __MIDL_itf_d3d12_0000_0082 */
/* [local] */ 

typedef 
enum D3D12_COOPERATIVE_VECTOR_TIER
    {
        D3D12_COOPERATIVE_VECTOR_TIER_NOT_SUPPORTED	= 0,
        D3D12_COOPERATIVE_VECTOR_TIER_1_0	= 0x10,
        D3D12_COOPERATIVE_VECTOR_TIER_1_1	= 0x11
    } 	D3D12_COOPERATIVE_VECTOR_TIER;

typedef 
enum D3D12_LINEAR_ALGEBRA_DATATYPE
    {
        D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16	= 2,
        D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16	= 3,
        D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32	= 4,
        D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32	= 5,
        D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16	= 7,
        D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32	= 8,
        D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8_T4_PACKED	= 16,
        D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8_T4_PACKED	= 17,
        D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8	= 18,
        D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8	= 19,
        D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3	= 20,
        D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2	= 21
    } 	D3D12_LINEAR_ALGEBRA_DATATYPE;

typedef struct D3D12_FEATURE_DATA_D3D12_OPTIONS_EXPERIMENTAL
    {
    _Out_  D3D12_COOPERATIVE_VECTOR_TIER CooperativeVectorTier;
    } 	D3D12_FEATURE_DATA_D3D12_OPTIONS_EXPERIMENTAL;

typedef struct D3D12_COOPERATIVE_VECTOR_PROPERTIES_MUL
    {
    D3D12_LINEAR_ALGEBRA_DATATYPE InputType;
    D3D12_LINEAR_ALGEBRA_DATATYPE InputInterpretation;
    D3D12_LINEAR_ALGEBRA_DATATYPE MatrixInterpretation;
    D3D12_LINEAR_ALGEBRA_DATATYPE BiasInterpretation;
    D3D12_LINEAR_ALGEBRA_DATATYPE OutputType;
    BOOL TransposeSupported;
    } 	D3D12_COOPERATIVE_VECTOR_PROPERTIES_MUL;

typedef struct D3D12_COOPERATIVE_VECTOR_PROPERTIES_ACCUMULATE
    {
    D3D12_LINEAR_ALGEBRA_DATATYPE InputType;
    D3D12_LINEAR_ALGEBRA_DATATYPE AccumulationType;
    } 	D3D12_COOPERATIVE_VECTOR_PROPERTIES_ACCUMULATE;

typedef struct D3D12_FEATURE_DATA_COOPERATIVE_VECTOR
    {
    _Inout_  UINT MatrixVectorMulAddPropCount;
    _Out_  D3D12_COOPERATIVE_VECTOR_PROPERTIES_MUL *pMatrixVectorMulAddProperties;
    _Inout_  UINT OuterProductAccumulatePropCount;
    _Out_  D3D12_COOPERATIVE_VECTOR_PROPERTIES_ACCUMULATE *pOuterProductAccumulateProperties;
    _Inout_  UINT VectorAccumulatePropCount;
    _Out_  D3D12_COOPERATIVE_VECTOR_PROPERTIES_ACCUMULATE *pVectorAccumulateProperties;
    } 	D3D12_FEATURE_DATA_COOPERATIVE_VECTOR;

typedef 
enum D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT
    {
        D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_ROW_MAJOR	= 0,
        D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_COLUMN_MAJOR	= ( D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_ROW_MAJOR + 1 ) ,
        D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_MUL_OPTIMAL	= ( D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_COLUMN_MAJOR + 1 ) ,
        D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL	= ( D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_MUL_OPTIMAL + 1 ) 
    } 	D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT;

typedef struct D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_DEST_INFO
    {
    _Inout_  UINT DestSize;
    _In_  D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT DestLayout;
    _In_  UINT DestStride;
    _In_  UINT NumRows;
    _In_  UINT NumColumns;
    _In_  D3D12_LINEAR_ALGEBRA_DATATYPE DestDataType;
    } 	D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_DEST_INFO;

typedef struct D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_DATA
    {
    _Inout_  D3D12_GPU_VIRTUAL_ADDRESS DestVA;
    _In_  D3D12_GPU_VIRTUAL_ADDRESS SrcVA;
    } 	D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_DATA;

typedef struct D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_SRC_INFO
    {
    _In_  UINT SrcSize;
    _In_  D3D12_LINEAR_ALGEBRA_DATATYPE SrcDataType;
    _In_  D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT SrcLayout;
    _In_  UINT SrcStride;
    } 	D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_SRC_INFO;

typedef struct D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_INFO
    {
    D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_DEST_INFO DestInfo;
    D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_SRC_INFO SrcInfo;
    D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_DATA DataDesc;
    } 	D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_INFO;



#ifndef __ID3D12DevicePreview_INTERFACE_DEFINED__
#define __ID3D12DevicePreview_INTERFACE_DEFINED__

EXTERN_C const IID IID_ID3D12DevicePreview;

MIDL_INTERFACE("55ea41d3-6bf5-4332-bbf9-905e6b4e2930")
ID3D12DevicePreview : public IUnknown
{
public:
    virtual void STDMETHODCALLTYPE GetLinearAlgebraMatrixConversionDestinationInfo( 
        _Inout_  D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_DEST_INFO *pDesc) = 0;
    
};

#endif 	/* __ID3D12DevicePreview_INTERFACE_DEFINED__ */


#ifndef __ID3D12GraphicsCommandList11_INTERFACE_DEFINED__
#define __ID3D12GraphicsCommandList11_INTERFACE_DEFINED__

EXTERN_C const IID IID_ID3D12GraphicsCommandList11;

MIDL_INTERFACE("f0dcfabc-a84a-4fe3-b3b9-eab26b306c38")
ID3D12GraphicsCommandList11 : public ID3D12GraphicsCommandList10
{
public:
    virtual void STDMETHODCALLTYPE Reserved0() = 0;
    virtual void STDMETHODCALLTYPE Reserved1() = 0;
    virtual void STDMETHODCALLTYPE Reserved2() = 0;

    virtual void STDMETHODCALLTYPE ConvertLinearAlgebraMatrix( 
        _In_  const D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_INFO *pDesc,
        _In_  UINT DescCount) = 0;
    
};

#endif 	/* __ID3D12GraphicsCommandList11_INTERFACE_DEFINED__ */

#else // __ID3D12GraphicsCommandList10_INTERFACE_DEFINED__
// The used d3d12.h header does not support ID3D12GraphicsCommandList10,
// so we cannot define ID3D12GraphicsCommandList11.
#define HAVE_COOPVEC_API 0
#endif // __ID3D12GraphicsCommandList10_INTERFACE_DEFINED__

#else // D3D12_PREVIEW_SDK_VERSION < 717
// Preview header has CoopVec support
#define HAVE_COOPVEC_API 1
#endif // D3D12_PREVIEW_SDK_VERSION < 717

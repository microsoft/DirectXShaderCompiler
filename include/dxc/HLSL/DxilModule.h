///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilModule.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// The main class to work with DXIL, similar to LLVM module.                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/HLSL/DxilMetadataHelper.h"
#include "dxc/HLSL/DxilCBuffer.h"
#include "dxc/HLSL/DxilResource.h"
#include "dxc/HLSL/DxilSampler.h"
#include "dxc/HLSL/DxilSignature.h"
#include "dxc/HLSL/DxilConstants.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include <memory>
#include <string>
#include <vector>

namespace llvm {
class LLVMContext;
class Module;
class Function;
class MDTuple;
class MDOperand;
class DebugInfoFinder;
};

namespace hlsl {

class ShaderModel;
class OP;
class RootSignatureHandle;

/// Use this class to manipulate DXIL of a shader.
class DxilModule {
public:
  DxilModule(llvm::Module *pModule);
  ~DxilModule();

  // Subsystems.
  llvm::LLVMContext &GetCtx() const;
  llvm::Module *GetModule() const;
  OP *GetOP() const;
  void SetShaderModel(const ShaderModel *pSM);
  const ShaderModel *GetShaderModel() const;

  // Entry functions.
  llvm::Function *GetEntryFunction();
  const llvm::Function *GetEntryFunction() const;
  void SetEntryFunction(llvm::Function *pEntryFunc);
  const std::string &GetEntryFunctionName() const;
  void SetEntryFunctionName(const std::string &name);
  llvm::Function *GetPatchConstantFunction();
  const llvm::Function *GetPatchConstantFunction() const;
  void SetPatchConstantFunction(llvm::Function *pFunc);

  // Flags.
  unsigned GetGlobalFlags() const;
  // TODO: move out of DxilModule as a util.
  void CollectShaderFlags();

  // Resources.
  unsigned AddCBuffer(std::unique_ptr<DxilCBuffer> pCB);
  DxilCBuffer &GetCBuffer(unsigned idx);
  const DxilCBuffer &GetCBuffer(unsigned idx) const;
  const std::vector<std::unique_ptr<DxilCBuffer> > &GetCBuffers() const;

  unsigned AddSampler(std::unique_ptr<DxilSampler> pSampler);
  DxilSampler &GetSampler(unsigned idx);
  const DxilSampler &GetSampler(unsigned idx) const;
  const std::vector<std::unique_ptr<DxilSampler> > &GetSamplers() const;

  unsigned AddSRV(std::unique_ptr<DxilResource> pSRV);
  DxilResource &GetSRV(unsigned idx);
  const DxilResource &GetSRV(unsigned idx) const;
  const std::vector<std::unique_ptr<DxilResource> > &GetSRVs() const;

  unsigned AddUAV(std::unique_ptr<DxilResource> pUAV);
  DxilResource &GetUAV(unsigned idx);
  const DxilResource &GetUAV(unsigned idx) const;
  const std::vector<std::unique_ptr<DxilResource> > &GetUAVs() const;

  void RemoveUnusedResources();

  // Signatures.
  DxilSignature &GetInputSignature();
  const DxilSignature &GetInputSignature() const;
  DxilSignature &GetOutputSignature();
  const DxilSignature &GetOutputSignature() const;
  DxilSignature &GetPatchConstantSignature();
  const DxilSignature &GetPatchConstantSignature() const;
  const RootSignatureHandle &GetRootSignature() const;

  // Remove Root Signature from module metadata
  void StripRootSignatureFromMetadata();

  // DXIL type system.
  DxilTypeSystem &GetTypeSystem();

  /// Emit llvm.used array to make sure that optimizations do not remove unreferenced globals.
  void EmitLLVMUsed();
  std::vector<llvm::GlobalVariable* > &GetLLVMUsed();

  // DXIL metadata manipulation.
  /// Serialize DXIL in-memory form to metadata form.
  void EmitDxilMetadata();
  /// Deserialize DXIL metadata form into in-memory form.
  void LoadDxilMetadata();
  /// Check if a Named meta data node is known by dxil module.
  static bool IsKnownNamedMetaData(llvm::NamedMDNode &Node);

  // Reset functions used to transfer ownership.
  void ResetInputSignature(DxilSignature *pValue);
  void ResetOutputSignature(DxilSignature *pValue);
  void ResetPatchConstantSignature(DxilSignature *pValue);
  void ResetRootSignature(RootSignatureHandle *pValue);
  void ResetTypeSystem(DxilTypeSystem *pValue);

  void StripDebugRelatedCode();
  llvm::DebugInfoFinder &GetOrCreateDebugInfoFinder();

  static DxilModule *TryGetDxilModule(llvm::Module *pModule);

public:
  // Shader properties.
  class ShaderFlags {
  public:
    ShaderFlags();

    unsigned GetGlobalFlags() const;
    void SetDisableOptimizations(bool flag) { m_bDisableOptimizations = flag; }
    void SetDisableMathRefactoring(bool flag) { m_bDisableMathRefactoring = flag; }
    void SetEnableDoublePrecision(bool flag) { m_bEnableDoublePrecision = flag; }
    void SetForceEarlyDepthStencil(bool flag) { m_bForceEarlyDepthStencil = flag; }
    void SetEnableRawAndStructuredBuffers(bool flag) { m_bEnableRawAndStructuredBuffers = flag; }
    void SetEnableMinPrecision(bool flag) { m_bEnableMinPrecision = flag; }
    void SetEnableDoubleExtensions(bool flag) { m_bEnableDoubleExtensions = flag; }
    void SetEnableMSAD(bool flag) { m_bEnableMSAD = flag; }
    void SetAllResourcesBound(bool flag) { m_bAllResourcesBound = flag; }

    uint64_t GetFeatureInfo() const;
    bool GetWaveOps() const { return m_bWaveOps; }
    void SetCSRawAndStructuredViaShader4X(bool flag) { m_bCSRawAndStructuredViaShader4X = flag; }
    void SetROVs(bool flag) { m_bROVS = flag; }
    void SetWaveOps(bool flag) { m_bWaveOps = flag; }
    void SetInt64Ops(bool flag) { m_bInt64Ops = flag; }
    void SetTiledResources(bool flag) { m_bTiledResources = flag; }
    void SetStencilRef(bool flag) { m_bStencilRef = flag; }
    void SetInnerCoverage(bool flag) { m_bInnerCoverage = flag; }
    void SetViewportAndRTArrayIndex(bool flag) { m_bViewportAndRTArrayIndex = flag; }
    void SetUAVLoadAdditionalFormats(bool flag) { m_bUAVLoadAdditionalFormats = flag; }
    void SetLevel9ComparisonFiltering(bool flag) { m_bLevel9ComparisonFiltering = flag; }
    void Set64UAVs(bool flag) { m_b64UAVs = flag; }
    void SetUAVsAtEveryStage(bool flag) { m_UAVsAtEveryStage = flag; }

    static uint64_t GetShaderFlagsRawForCollection(); // some flags are collected (eg use 64-bit), some provided (eg allow refactoring)
    uint64_t GetShaderFlagsRaw() const;
    void SetShaderFlagsRaw(uint64_t data);

  private:
    unsigned m_bDisableOptimizations :1;   // D3D11_1_SB_GLOBAL_FLAG_SKIP_OPTIMIZATION
    unsigned m_bDisableMathRefactoring :1; //~D3D10_SB_GLOBAL_FLAG_REFACTORING_ALLOWED
    unsigned m_bEnableDoublePrecision :1; // D3D11_SB_GLOBAL_FLAG_ENABLE_DOUBLE_PRECISION_FLOAT_OPS
    unsigned m_bForceEarlyDepthStencil :1; // D3D11_SB_GLOBAL_FLAG_FORCE_EARLY_DEPTH_STENCIL
    unsigned m_bEnableRawAndStructuredBuffers :1; // D3D11_SB_GLOBAL_FLAG_ENABLE_RAW_AND_STRUCTURED_BUFFERS
    unsigned m_bEnableMinPrecision :1; // D3D11_1_SB_GLOBAL_FLAG_ENABLE_MINIMUM_PRECISION
    unsigned m_bEnableDoubleExtensions :1; // D3D11_1_SB_GLOBAL_FLAG_ENABLE_DOUBLE_EXTENSIONS
    unsigned m_bEnableMSAD :1;        // D3D11_1_SB_GLOBAL_FLAG_ENABLE_SHADER_EXTENSIONS
    unsigned m_bAllResourcesBound :1; // D3D12_SB_GLOBAL_FLAG_ALL_RESOURCES_BOUND

    unsigned m_bViewportAndRTArrayIndex :1;   // SHADER_FEATURE_VIEWPORT_AND_RT_ARRAY_INDEX_FROM_ANY_SHADER_FEEDING_RASTERIZER
    unsigned m_bInnerCoverage :1;             // SHADER_FEATURE_INNER_COVERAGE
    unsigned m_bStencilRef  :1;               // SHADER_FEATURE_STENCIL_REF
    unsigned m_bTiledResources  :1;           // SHADER_FEATURE_TILED_RESOURCES
    unsigned m_bUAVLoadAdditionalFormats :1;  // SHADER_FEATURE_TYPED_UAV_LOAD_ADDITIONAL_FORMATS
    unsigned m_bLevel9ComparisonFiltering :1; // SHADER_FEATURE_LEVEL_9_COMPARISON_FILTERING
                                              // SHADER_FEATURE_11_1_SHADER_EXTENSIONS shared with EnableMSAD
    unsigned m_b64UAVs :1;                    // SHADER_FEATURE_64_UAVS
    unsigned m_UAVsAtEveryStage :1;           // SHADER_FEATURE_UAVS_AT_EVERY_STAGE
    unsigned m_bCSRawAndStructuredViaShader4X : 1; // SHADER_FEATURE_COMPUTE_SHADERS_PLUS_RAW_AND_STRUCTURED_BUFFERS_VIA_SHADER_4_X
    
    // SHADER_FEATURE_COMPUTE_SHADERS_PLUS_RAW_AND_STRUCTURED_BUFFERS_VIA_SHADER_4_X is specifically
    // about shader model 4.x.

    unsigned m_bROVS :1;              // SHADER_FEATURE_ROVS
    unsigned m_bWaveOps :1;           // SHADER_FEATURE_WAVE_OPS
    unsigned m_bInt64Ops :1;          // SHADER_FEATURE_INT64_OPS
    unsigned m_align0 :11;        // align to 32 bit.
    uint32_t m_align1;            // align to 64 bit.
  };

  ShaderFlags m_ShaderFlags;
  void CollectShaderFlags(ShaderFlags &Flags);

  // Compute shader.
  unsigned m_NumThreads[3];

  // Geometry shader.
  DXIL::InputPrimitive GetInputPrimitive() const;
  void SetInputPrimitive(DXIL::InputPrimitive IP);
  unsigned GetMaxVertexCount() const;
  void SetMaxVertexCount(unsigned Count);
  DXIL::PrimitiveTopology GetStreamPrimitiveTopology() const;
  void SetStreamPrimitiveTopology(DXIL::PrimitiveTopology Topology);
  bool HasMultipleOutputStreams() const;
  unsigned GetOutputStream() const;
  unsigned GetGSInstanceCount() const;
  void SetGSInstanceCount(unsigned Count);
  bool IsStreamActive(unsigned Stream) const;
  void SetStreamActive(unsigned Stream, bool bActive);
  void SetActiveStreamMask(unsigned Mask);
  unsigned GetActiveStreamMask() const;

  // Hull and Domain shaders.
  unsigned GetInputControlPointCount() const;
  void SetInputControlPointCount(unsigned NumICPs);
  DXIL::TessellatorDomain GetTessellatorDomain() const;
  void SetTessellatorDomain(DXIL::TessellatorDomain TessDomain);

  // Hull shader.
  unsigned GetOutputControlPointCount() const;
  void SetOutputControlPointCount(unsigned NumOCPs);
  DXIL::TessellatorPartitioning GetTessellatorPartitioning() const;
  void SetTessellatorPartitioning(DXIL::TessellatorPartitioning TessPartitioning);
  DXIL::TessellatorOutputPrimitive GetTessellatorOutputPrimitive() const;
  void SetTessellatorOutputPrimitive(DXIL::TessellatorOutputPrimitive TessOutputPrimitive);
  float GetMaxTessellationFactor() const;
  void SetMaxTessellationFactor(float MaxTessellationFactor);

private:
  // Signatures.
  std::unique_ptr<DxilSignature> m_InputSignature;
  std::unique_ptr<DxilSignature> m_OutputSignature;
  std::unique_ptr<DxilSignature> m_PatchConstantSignature;
  std::unique_ptr<RootSignatureHandle> m_RootSignature;

  // Shader resources.
  std::vector<std::unique_ptr<DxilResource> > m_SRVs;
  std::vector<std::unique_ptr<DxilResource> > m_UAVs;
  std::vector<std::unique_ptr<DxilCBuffer> > m_CBuffers;
  std::vector<std::unique_ptr<DxilSampler> > m_Samplers;

  // Geometry shader.
  DXIL::InputPrimitive m_InputPrimitive;
  unsigned m_MaxVertexCount;
  DXIL::PrimitiveTopology m_StreamPrimitiveTopology;
  unsigned m_ActiveStreamMask;
  unsigned m_NumGSInstances;

  // Hull and Domain shaders.
  unsigned m_InputControlPointCount;
  DXIL::TessellatorDomain m_TessellatorDomain;

  // Hull shader.
  unsigned m_OutputControlPointCount;
  DXIL::TessellatorPartitioning m_TessellatorPartitioning;
  DXIL::TessellatorOutputPrimitive m_TessellatorOutputPrimitive;
  float m_MaxTessellationFactor;

private:
  llvm::LLVMContext &m_Ctx;
  llvm::Module *m_pModule;
  llvm::Function *m_pEntryFunc;
  llvm::Function *m_pPatchConstantFunc;
  std::string m_EntryName;
  std::unique_ptr<DxilMDHelper> m_pMDHelper;
  std::unique_ptr<llvm::DebugInfoFinder> m_pDebugInfoFinder;
  const ShaderModel *m_pSM;
  unsigned m_DxilMajor;
  unsigned m_DxilMinor;

  std::unique_ptr<OP> m_pOP;
  size_t m_pUnused;

  // LLVM used.
  std::vector<llvm::GlobalVariable*> m_LLVMUsed;

  // Type annotations.
  std::unique_ptr<DxilTypeSystem> m_pTypeSystem;

  // DXIL metadata serialization/deserialization.
  llvm::MDTuple *EmitDxilResources();
  void LoadDxilResources(const llvm::MDOperand &MDO);
  llvm::MDTuple *EmitDxilShaderProperties();
  void LoadDxilShaderProperties(const llvm::MDOperand &MDO);

  // Helpers.
  template<typename T> unsigned AddResource(std::vector<std::unique_ptr<T> > &Vec, std::unique_ptr<T> pRes);
  void LoadDxilSignature(const llvm::MDTuple *pSigTuple, DxilSignature &Sig, bool bInput);
};

} // namespace hlsl

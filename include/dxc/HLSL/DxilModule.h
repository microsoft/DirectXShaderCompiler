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
#include "dxc/HLSL/ComputeViewIdState.h"



#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace llvm {
class LLVMContext;
class Module;
class Function;
class Instruction;
class MDTuple;
class MDOperand;
class DebugInfoFinder;
}

namespace hlsl {

class ShaderModel;
class OP;
class RootSignatureHandle;
struct DxilFunctionProps;

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
  void GetDxilVersion(unsigned &DxilMajor, unsigned &DxilMinor) const;
  void SetValidatorVersion(unsigned ValMajor, unsigned ValMinor);
  bool UpgradeValidatorVersion(unsigned ValMajor, unsigned ValMinor);
  void GetValidatorVersion(unsigned &ValMajor, unsigned &ValMinor) const;

  // Return true on success, requires valid shader model and CollectShaderFlags to have been set
  bool GetMinValidatorVersion(unsigned &ValMajor, unsigned &ValMinor) const;
  // Update validator version to minimum if higher than current (ex: after CollectShaderFlags)
  bool UpgradeToMinValidatorVersion();

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

  void CreateResourceLinkInfo();
  struct ResourceLinkInfo;
  const ResourceLinkInfo &GetResourceLinkInfo(DXIL::ResourceClass resClass,
                                        unsigned rangeID) const;

  void LoadDxilResourceBaseFromMDNode(llvm::MDNode *MD, DxilResourceBase &R);
  void LoadDxilResourceFromMDNode(llvm::MDNode *MD, DxilResource &R);
  void LoadDxilSamplerFromMDNode(llvm::MDNode *MD, DxilSampler &S);

  void RemoveUnusedResources();
  void RemoveFunction(llvm::Function *F);

  // Signatures.
  DxilSignature &GetInputSignature();
  const DxilSignature &GetInputSignature() const;
  DxilSignature &GetOutputSignature();
  const DxilSignature &GetOutputSignature() const;
  DxilSignature &GetPatchConstantSignature();
  const DxilSignature &GetPatchConstantSignature() const;
  const RootSignatureHandle &GetRootSignature() const;
  bool HasDxilEntrySignature(llvm::Function *F) const;
  DxilEntrySignature &GetDxilEntrySignature(llvm::Function *F);
  // Move DxilEntrySignature of F to NewF.
  void ReplaceDxilEntrySignature(llvm::Function *F, llvm::Function *NewF);

  // DxilFunctionProps.
  bool HasDxilFunctionProps(llvm::Function *F) const;
  DxilFunctionProps &GetDxilFunctionProps(llvm::Function *F);
  // Move DxilFunctionProps of F to NewF.
  void ReplaceDxilFunctionProps(llvm::Function *F, llvm::Function *NewF);

  // Remove Root Signature from module metadata
  void StripRootSignatureFromMetadata();
  // Update validator version metadata to current setting
  void UpdateValidatorVersionMetadata();

  // DXIL type system.
  DxilTypeSystem &GetTypeSystem();

  /// Emit llvm.used array to make sure that optimizations do not remove unreferenced globals.
  void EmitLLVMUsed();
  std::vector<llvm::GlobalVariable* > &GetLLVMUsed();

  // ViewId state.
  DxilViewIdState &GetViewIdState();
  const DxilViewIdState &GetViewIdState() const;

  // DXIL metadata manipulation.
  /// Clear all DXIL data that exists in in-memory form.
  static void ClearDxilMetadata(llvm::Module &M);
  /// Serialize DXIL in-memory form to metadata form.
  void EmitDxilMetadata();
  /// Update resource metadata.
  void ReEmitDxilResources();
  /// Deserialize DXIL metadata form into in-memory form.
  void LoadDxilMetadata();
  /// Check if a Named meta data node is known by dxil module.
  static bool IsKnownNamedMetaData(llvm::NamedMDNode &Node);

  // Reset functions used to transfer ownership.
  void ResetEntrySignature(DxilEntrySignature *pValue);
  void ResetRootSignature(RootSignatureHandle *pValue);
  void ResetTypeSystem(DxilTypeSystem *pValue);
  void ResetOP(hlsl::OP *hlslOP);
  void ResetFunctionPropsMap(
      std::unordered_map<llvm::Function *, std::unique_ptr<DxilFunctionProps>>
          &&propsMap);
  void ResetEntrySignatureMap(
      std::unordered_map<llvm::Function *, std::unique_ptr<DxilEntrySignature>>
          &&SigMap);

  void StripDebugRelatedCode();
  llvm::DebugInfoFinder &GetOrCreateDebugInfoFinder();

  static DxilModule *TryGetDxilModule(llvm::Module *pModule);

  // Helpers for working with precise.

  // Return true if the instruction should be considered precise.
  //
  // An instruction can be marked precise in the following ways:
  //
  // 1. Global refactoring is disabled.
  // 2. The instruction has a precise metadata annotation.
  // 3. The instruction has precise fast math flags set.
  //
  bool IsPrecise(const llvm::Instruction *inst) const;

  // Check if the instruction has fast math flags configured to indicate
  // the instruction is precise.
  static bool HasPreciseFastMathFlags(const llvm::Instruction *inst);
  
  // Set fast math flags configured to indicate the instruction is precise.
  static void SetPreciseFastMathFlags(llvm::Instruction *inst);
  
  // True if fast math flags are preserved across serialize/deserialize.
  static bool PreservesFastMathFlags(const llvm::Instruction *inst);

public:
  // Shader properties.
  class ShaderFlags {
  public:
    ShaderFlags();

    unsigned GetGlobalFlags() const;
    void SetDisableOptimizations(bool flag) { m_bDisableOptimizations = flag; }
    bool GetDisableOptimizations() const { return m_bDisableOptimizations; }

    void SetDisableMathRefactoring(bool flag) { m_bDisableMathRefactoring = flag; }
    bool GetDisableMathRefactoring() const { return m_bDisableMathRefactoring; }

    void SetEnableDoublePrecision(bool flag) { m_bEnableDoublePrecision = flag; }
    bool GetEnableDoublePrecision() const { return m_bEnableDoublePrecision; }

    void SetForceEarlyDepthStencil(bool flag) { m_bForceEarlyDepthStencil = flag; }
    bool GetForceEarlyDepthStencil() const { return m_bForceEarlyDepthStencil; }

    void SetEnableRawAndStructuredBuffers(bool flag) { m_bEnableRawAndStructuredBuffers = flag; }
    bool GetEnableRawAndStructuredBuffers() const { return m_bEnableRawAndStructuredBuffers; }

    void SetLowPrecisionPresent(bool flag) { m_bLowPrecisionPresent = flag; }
    bool GetLowPrecisionPresent() const { return m_bLowPrecisionPresent; }

    void SetEnableDoubleExtensions(bool flag) { m_bEnableDoubleExtensions = flag; }
    bool GetEnableDoubleExtensions() const { return m_bEnableDoubleExtensions; }

    void SetEnableMSAD(bool flag) { m_bEnableMSAD = flag; }
    bool GetEnableMSAD() const { return m_bEnableMSAD; }

    void SetAllResourcesBound(bool flag) { m_bAllResourcesBound = flag; }
    bool GetAllResourcesBound() const { return m_bAllResourcesBound; }

    uint64_t GetFeatureInfo() const;
    void SetCSRawAndStructuredViaShader4X(bool flag) { m_bCSRawAndStructuredViaShader4X = flag; }
    bool GetCSRawAndStructuredViaShader4X() const { return m_bCSRawAndStructuredViaShader4X; }

    void SetROVs(bool flag) { m_bROVS = flag; }
    bool GetROVs() const { return m_bROVS; }

    void SetWaveOps(bool flag) { m_bWaveOps = flag; }
    bool GetWaveOps() const { return m_bWaveOps; }

    void SetInt64Ops(bool flag) { m_bInt64Ops = flag; }
    bool GetInt64Ops() const { return m_bInt64Ops; }

    void SetTiledResources(bool flag) { m_bTiledResources = flag; }
    bool GetTiledResources() const { return m_bTiledResources; }

    void SetStencilRef(bool flag) { m_bStencilRef = flag; }
    bool GetStencilRef() const { return m_bStencilRef; }

    void SetInnerCoverage(bool flag) { m_bInnerCoverage = flag; }
    bool GetInnerCoverage() const { return m_bInnerCoverage; }

    void SetViewportAndRTArrayIndex(bool flag) { m_bViewportAndRTArrayIndex = flag; }
    bool GetViewportAndRTArrayIndex() const { return m_bViewportAndRTArrayIndex; }

    void SetUAVLoadAdditionalFormats(bool flag) { m_bUAVLoadAdditionalFormats = flag; }
    bool GetUAVLoadAdditionalFormats() const { return m_bUAVLoadAdditionalFormats; }

    void SetLevel9ComparisonFiltering(bool flag) { m_bLevel9ComparisonFiltering = flag; }
    bool GetLevel9ComparisonFiltering() const { return m_bLevel9ComparisonFiltering; }

    void Set64UAVs(bool flag) { m_b64UAVs = flag; }
    bool Get64UAVs() const { return m_b64UAVs; }

    void SetUAVsAtEveryStage(bool flag) { m_UAVsAtEveryStage = flag; }
    bool GetUAVsAtEveryStage() const { return m_UAVsAtEveryStage; }

    void SetViewID(bool flag) { m_bViewID = flag; }
    bool GetViewID() const { return m_bViewID; }

    void SetBarycentrics(bool flag) { m_bBarycentrics = flag; }
    bool GetBarycentrics() const { return m_bBarycentrics; }

    void SetUseNativeLowPrecision(bool flag) { m_bUseNativeLowPrecision = flag; }
    bool GetUseNativeLowPrecision() const { return m_bUseNativeLowPrecision; }

    static uint64_t GetShaderFlagsRawForCollection(); // some flags are collected (eg use 64-bit), some provided (eg allow refactoring)
    uint64_t GetShaderFlagsRaw() const;
    void SetShaderFlagsRaw(uint64_t data);

  private:
    unsigned m_bDisableOptimizations :1;   // D3D11_1_SB_GLOBAL_FLAG_SKIP_OPTIMIZATION
    unsigned m_bDisableMathRefactoring :1; //~D3D10_SB_GLOBAL_FLAG_REFACTORING_ALLOWED
    unsigned m_bEnableDoublePrecision :1; // D3D11_SB_GLOBAL_FLAG_ENABLE_DOUBLE_PRECISION_FLOAT_OPS
    unsigned m_bForceEarlyDepthStencil :1; // D3D11_SB_GLOBAL_FLAG_FORCE_EARLY_DEPTH_STENCIL
    unsigned m_bEnableRawAndStructuredBuffers :1; // D3D11_SB_GLOBAL_FLAG_ENABLE_RAW_AND_STRUCTURED_BUFFERS
    unsigned m_bLowPrecisionPresent :1; // D3D11_1_SB_GLOBAL_FLAG_ENABLE_MINIMUM_PRECISION
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
    unsigned m_bViewID : 1;           // SHADER_FEATURE_VIEWID
    unsigned m_bBarycentrics : 1;     // SHADER_FEATURE_BARYCENTRICS

    unsigned m_bUseNativeLowPrecision : 1;

    unsigned m_align0 : 8;        // align to 32 bit.
    uint32_t m_align1;            // align to 64 bit.
  };

  ShaderFlags m_ShaderFlags;
  void CollectShaderFlags(ShaderFlags &Flags);

  // Check if DxilModule contains multi component UAV Loads.
  // This funciton must be called after unused resources are removed from DxilModule
  bool ModuleHasMulticomponentUAVLoads();

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

  void SetShaderProperties(DxilFunctionProps *props);

  // Shader resource information only needed before linking.
  // Use constant as rangeID for resource in a library.
  // When link the library, replace these constants with real rangeID.
  struct ResourceLinkInfo {
    llvm::Constant *ResRangeID;
  };

private:
  // Signatures.
  std::unique_ptr<DxilEntrySignature> m_EntrySignature;
  std::unique_ptr<RootSignatureHandle> m_RootSignature;

  // Shader resources.
  std::vector<std::unique_ptr<DxilResource> > m_SRVs;
  std::vector<std::unique_ptr<DxilResource> > m_UAVs;
  std::vector<std::unique_ptr<DxilCBuffer> > m_CBuffers;
  std::vector<std::unique_ptr<DxilSampler> > m_Samplers;

  // Save resource link for library, when link replace it with real resource ID.
  std::vector<ResourceLinkInfo> m_SRVsLinkInfo;
  std::vector<ResourceLinkInfo> m_UAVsLinkInfo;
  std::vector<ResourceLinkInfo> m_CBuffersLinkInfo;
  std::vector<ResourceLinkInfo> m_SamplersLinkInfo;

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
  unsigned m_ValMajor;
  unsigned m_ValMinor;

  std::unique_ptr<OP> m_pOP;
  size_t m_pUnused;

  // LLVM used.
  std::vector<llvm::GlobalVariable*> m_LLVMUsed;

  // Type annotations.
  std::unique_ptr<DxilTypeSystem> m_pTypeSystem;

  // Function properties for shader functions.
  std::unordered_map<llvm::Function *, std::unique_ptr<DxilFunctionProps>>
      m_DxilFunctionPropsMap;
  // EntrySig for shader functions.
  std::unordered_map<llvm::Function *, std::unique_ptr<DxilEntrySignature>>
      m_DxilEntrySignatureMap;

  // ViewId state.
  std::unique_ptr<DxilViewIdState> m_pViewIdState;

  // DXIL metadata serialization/deserialization.
  llvm::MDTuple *EmitDxilResources();
  void LoadDxilResources(const llvm::MDOperand &MDO);
  void EmitDxilResourcesLinkInfo();
  void LoadDxilResourcesLinkInfo();
  llvm::MDTuple *EmitDxilShaderProperties();
  void LoadDxilShaderProperties(const llvm::MDOperand &MDO);

  // Helpers.
  template<typename T> unsigned AddResource(std::vector<std::unique_ptr<T> > &Vec, std::unique_ptr<T> pRes);
  void LoadDxilSignature(const llvm::MDTuple *pSigTuple, DxilSignature &Sig, bool bInput);
};

} // namespace hlsl

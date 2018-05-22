///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilLibraryReflection.cpp                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Defines shader reflection for runtime usage.                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/hlsl/DxilRuntimeReflection.h"
#include <windows.h>
#include <unordered_map>
#include <vector>
#include <memory>

namespace hlsl {
namespace RDAT {

struct ResourceKey {
  uint32_t Class, ID;
  ResourceKey(uint32_t Class, uint32_t ID) : Class(Class), ID(ID) {}
  bool operator==(const ResourceKey& other) const {
    return other.Class == Class && other.ID == ID;
  }
};

DxilRuntimeData::DxilRuntimeData() : DxilRuntimeData(nullptr) {}

DxilRuntimeData::DxilRuntimeData(const char *ptr)
    : m_TableCount(0), m_StringReader(), m_ResourceTableReader(),
      m_FunctionTableReader(), m_IndexTableReader(), m_Context() {
  m_Context = {&m_StringReader, &m_IndexTableReader, &m_ResourceTableReader,
               &m_FunctionTableReader};
  m_ResourceTableReader.SetContext(&m_Context);
  m_FunctionTableReader.SetContext(&m_Context);
  InitFromRDAT(ptr);
}

// initializing reader from RDAT. return true if no error has occured.
bool DxilRuntimeData::InitFromRDAT(const void *pRDAT) {
  if (pRDAT) {
    const char *ptr = static_cast<const char *>(pRDAT);
    uint32_t TableCount = (uint32_t)*ptr;
    RuntimeDataTableHeader *records = (RuntimeDataTableHeader *)(ptr + 4);
    for (uint32_t i = 0; i < TableCount; ++i) {
      RuntimeDataTableHeader *curRecord = &records[i];
      switch (static_cast<RuntimeDataPartType>(curRecord->tableType)) {
      case RuntimeDataPartType::Resource: {
        m_ResourceTableReader.SetResourceInfo(
            (RuntimeDataResourceInfo *)(ptr + curRecord->offset),
            curRecord->size / sizeof(RuntimeDataResourceInfo));
        break;
      }
      case RuntimeDataPartType::String: {
        m_StringReader =
            StringTableReader(ptr + curRecord->offset, curRecord->size);
        break;
      }
      case RuntimeDataPartType::Function: {
        m_FunctionTableReader.SetFunctionInfo(
            (RuntimeDataFunctionInfo *)(ptr + curRecord->offset));
        m_FunctionTableReader.SetCount(curRecord->size /
                                       sizeof(RuntimeDataFunctionInfo));
        break;
      }
      case RuntimeDataPartType::Index: {
        m_IndexTableReader = IndexTableReader(
            (uint32_t *)(ptr + curRecord->offset), curRecord->size / 4);
        break;
      }
      default:
        return false;
      }
    }
    return true;
  }
  return false;
}

FunctionTableReader *DxilRuntimeData::GetFunctionTableReader() {
  return &m_FunctionTableReader;
}

ResourceTableReader *DxilRuntimeData::GetResourceTableReader() {
  return &m_ResourceTableReader;
}

}} // hlsl::RDAT

using namespace hlsl;
using namespace RDAT;

template<>
struct std::hash<ResourceKey> {
public:
  size_t operator()(const ResourceKey& key) const throw() {
    return (std::hash<uint32_t>()(key.Class) * (size_t)16777619U)
      ^ std::hash<uint32_t>()(key.ID);
  }
};

namespace {

class DxilRuntimeReflection_impl : public DxilRuntimeReflection {
private:
  typedef std::unordered_map<const char *, std::unique_ptr<wchar_t[]>> StringMap;
  typedef std::vector<DXIL_RESOURCE> ResourceList;
  typedef std::vector<DXIL_RESOURCE *> ResourceRefList;
  typedef std::vector<DXIL_FUNCTION> FunctionList;
  typedef std::vector<const wchar_t *> WStringList;

  DxilRuntimeData m_RuntimeData;
  StringMap m_StringMap;
  ResourceList m_Resources;
  FunctionList m_Functions;
  std::unordered_map<ResourceKey, DXIL_RESOURCE *> m_ResourceMap;
  std::unordered_map<DXIL_FUNCTION *, ResourceRefList> m_FuncToResMap;
  std::unordered_map<DXIL_FUNCTION *, WStringList> m_FuncToStringMap;
  bool m_initialized;

  const wchar_t *GetWideString(const char *ptr);
  void AddString(const char *ptr);
  void InitializeReflection();
  const DXIL_RESOURCE * const*GetResourcesForFunction(DXIL_FUNCTION &function,
                             const FunctionReader &functionReader);
  const wchar_t **GetDependenciesForFunction(DXIL_FUNCTION &function,
                             const FunctionReader &functionReader);
  DXIL_RESOURCE *AddResource(const ResourceReader &resourceReader);
  DXIL_FUNCTION *AddFunction(const FunctionReader &functionReader);

public:
  // TODO: Implement pipeline state validation with runtime data
  // TODO: Update BlobContainer.h to recognize 'RDAT' blob
  // TODO: Add size and verification to InitFromRDAT and DxilRuntimeData
  DxilRuntimeReflection_impl()
      : m_RuntimeData(), m_StringMap(), m_Resources(), m_Functions(),
        m_FuncToResMap(), m_FuncToStringMap(), m_initialized(false) {}
  virtual ~DxilRuntimeReflection_impl() {}
  // This call will allocate memory for GetLibraryReflection call
  bool InitFromRDAT(const void *pRDAT) override;
  const DXIL_LIBRARY_DESC GetLibraryReflection() override;
};

void DxilRuntimeReflection_impl::AddString(const char *ptr) {
  if (m_StringMap.find(ptr) == m_StringMap.end()) {
    int size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ptr, -1,
                                     nullptr, 0);
    if (size != 0) {
      auto pNew = std::make_unique<wchar_t[]>(size);
      ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ptr, -1,
                            pNew.get(), size);
      m_StringMap[ptr] = std::move(pNew);
    }
  }
}

const wchar_t *DxilRuntimeReflection_impl::GetWideString(const char *ptr) {
  if (m_StringMap.find(ptr) == m_StringMap.end()) {
    AddString(ptr);
  }
  return m_StringMap.at(ptr).get();
}

bool DxilRuntimeReflection_impl::InitFromRDAT(const void *pRDAT) {
  m_initialized = m_RuntimeData.InitFromRDAT(pRDAT);
  if (m_initialized)
    InitializeReflection();
  return m_initialized;
}

const DXIL_LIBRARY_DESC DxilRuntimeReflection_impl::GetLibraryReflection() {
  DXIL_LIBRARY_DESC reflection = {};
  if (m_initialized) {
    reflection.NumResources =
        m_RuntimeData.GetResourceTableReader()->GetNumResources();
    reflection.pResource = m_Resources.data();
    reflection.NumFunctions =
        m_RuntimeData.GetFunctionTableReader()->GetNumFunctions();
    reflection.pFunction = m_Functions.data();
  }
  return reflection;
}

void DxilRuntimeReflection_impl::InitializeReflection() {
  // First need to reserve spaces for resources because functions will need to
  // reference them via pointers.
  const ResourceTableReader *resourceTableReader = m_RuntimeData.GetResourceTableReader();
  m_Resources.reserve(resourceTableReader->GetNumResources());
  for (uint32_t i = 0; i < resourceTableReader->GetNumResources(); ++i) {
    ResourceReader resourceReader = resourceTableReader->GetItem(i);
    AddString(resourceReader.GetName());
    DXIL_RESOURCE *pResource = AddResource(resourceReader);
    if (pResource) {
      ResourceKey key(pResource->Class, pResource->ID);
      m_ResourceMap[key] = pResource;
    }
  }
  const FunctionTableReader *functionTableReader = m_RuntimeData.GetFunctionTableReader();
  m_Functions.reserve(functionTableReader->GetNumFunctions());
  for (uint32_t i = 0; i < functionTableReader->GetNumFunctions(); ++i) {
    FunctionReader functionReader = functionTableReader->GetItem(i);
    AddString(functionReader.GetName());
    AddFunction(functionReader);
  }
}

DXIL_RESOURCE *
DxilRuntimeReflection_impl::AddResource(const ResourceReader &resourceReader) {
  assert(m_Resources.size() < m_Resources.capacity() && "Otherwise, number of resources was incorrect");
  if (!(m_Resources.size() < m_Resources.capacity()))
    return nullptr;
  m_Resources.emplace_back(DXIL_RESOURCE({0}));
  DXIL_RESOURCE &resource = m_Resources.back();
  resource.Class = (uint32_t)resourceReader.GetResourceClass();
  resource.Kind = (uint32_t)resourceReader.GetResourceKind();
  resource.Space = resourceReader.GetSpace();
  resource.LowerBound = resourceReader.GetLowerBound();
  resource.UpperBound = resourceReader.GetUpperBound();
  resource.ID = resourceReader.GetID();
  resource.Flags = resourceReader.GetFlags();
  resource.Name = GetWideString(resourceReader.GetName());
  return &resource;
}

const DXIL_RESOURCE * const*DxilRuntimeReflection_impl::GetResourcesForFunction(
    DXIL_FUNCTION &function, const FunctionReader &functionReader) {
  if (m_FuncToResMap.find(&function) == m_FuncToResMap.end())
    m_FuncToResMap.insert(std::pair<DXIL_FUNCTION *, ResourceRefList>(
        &function, ResourceRefList()));
  ResourceRefList &resourceList = m_FuncToResMap.at(&function);
  if (resourceList.empty()) {
    resourceList.reserve(functionReader.GetNumResources());
    for (uint32_t i = 0; i < functionReader.GetNumResources(); ++i) {
      const ResourceReader resourceReader = functionReader.GetResource(i);
      ResourceKey key((uint32_t)resourceReader.GetResourceClass(),
                      resourceReader.GetID());
      auto it = m_ResourceMap.find(key);
      assert(it != m_ResourceMap.end() && it->second && "Otherwise, resource was not in map, or was null");
      resourceList.emplace_back(it->second);
    }
  }
  return resourceList.empty() ? nullptr : resourceList.data();
}

const wchar_t **DxilRuntimeReflection_impl::GetDependenciesForFunction(
    DXIL_FUNCTION &function, const FunctionReader &functionReader) {
  if (m_FuncToStringMap.find(&function) == m_FuncToStringMap.end())
    m_FuncToStringMap.insert(
        std::pair<DXIL_FUNCTION *, WStringList>(&function, WStringList()));
  WStringList &wStringList = m_FuncToStringMap.at(&function);
  for (uint32_t i = 0; i < functionReader.GetNumDependencies(); ++i) {
    wStringList.emplace_back(GetWideString(functionReader.GetDependency(i)));
  }
  return wStringList.empty() ? nullptr : wStringList.data();
}

DXIL_FUNCTION *
DxilRuntimeReflection_impl::AddFunction(const FunctionReader &functionReader) {
  assert(m_Functions.size() < m_Functions.capacity() && "Otherwise, number of functions was incorrect");
  if (!(m_Functions.size() < m_Functions.capacity()))
    return nullptr;
  m_Functions.emplace_back(DXIL_FUNCTION({0}));
  DXIL_FUNCTION &function = m_Functions.back();
  function.Name = GetWideString(functionReader.GetName());
  function.UnmangledName = GetWideString(functionReader.GetUnmangledName());
  function.NumResources = functionReader.GetNumResources();
  function.Resources = GetResourcesForFunction(function, functionReader);
  function.NumFunctionDependencies = functionReader.GetNumDependencies();
  function.FunctionDependencies =
      GetDependenciesForFunction(function, functionReader);
  function.ShaderKind = (uint32_t)functionReader.GetShaderKind();
  function.PayloadSizeInBytes = functionReader.GetPayloadSizeInBytes();
  function.AttributeSizeInBytes = functionReader.GetAttributeSizeInBytes();
  function.FeatureInfo1 = functionReader.GetFeatureInfo1();
  function.FeatureInfo2 = functionReader.GetFeatureInfo2();
  function.ShaderStageFlag = functionReader.GetShaderStageFlag();
  function.MinShaderTarget = functionReader.GetMinShaderTarget();
  return &function;
}

} // namespace anon

DxilRuntimeReflection *hlsl::RDAT::CreateDxilRuntimeReflection() {
  return new DxilRuntimeReflection_impl();
}

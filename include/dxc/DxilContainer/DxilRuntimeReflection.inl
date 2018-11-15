///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilRuntimeReflection.inl                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Defines shader reflection for runtime usage.                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DxilContainer/DxilRuntimeReflection.h"
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

// Size-checked reader
//  on overrun: throw buffer_overrun{};
//  on overlap: throw buffer_overlap{};
class CheckedReader {
  const char *Ptr;
  size_t Size;
  size_t Offset;

public:
  class exception : public std::exception {};
  class buffer_overrun : public exception {
  public:
    buffer_overrun() noexcept {}
    virtual const char * what() const noexcept override {
      return ("buffer_overrun");
    }
  };
  class buffer_overlap : public exception {
  public:
    buffer_overlap() noexcept {}
    virtual const char * what() const noexcept override {
      return ("buffer_overlap");
    }
  };

  CheckedReader(const void *ptr, size_t size) :
    Ptr(reinterpret_cast<const char*>(ptr)), Size(size), Offset(0) {}
  void Reset(size_t offset = 0) {
    if (offset >= Size) throw buffer_overrun{};
    Offset = offset;
  }
  // offset is absolute, ensure offset is >= current offset
  void Advance(size_t offset = 0) {
    if (offset < Offset) throw buffer_overlap{};
    if (offset >= Size) throw buffer_overrun{};
    Offset = offset;
  }
  void CheckBounds(size_t size) const {
    assert(Offset <= Size && "otherwise, offset larger than size");
    if (size > Size - Offset)
      throw buffer_overrun{};
  }
  template <typename T>
  const T *Cast(size_t size = 0) {
    if (0 == size) size = sizeof(T);
    CheckBounds(size);
    return reinterpret_cast<const T*>(Ptr + Offset);
  }
  template <typename T>
  const T &Read() {
    const size_t size = sizeof(T);
    const T* p = Cast<T>(size);
    Offset += size;
    return *p;
  }
  template <typename T>
  const T *ReadArray(size_t count = 1) {
    const size_t size = sizeof(T) * count;
    const T* p = Cast<T>(size);
    Offset += size;
    return p;
  }
};

DxilRuntimeData::DxilRuntimeData() : DxilRuntimeData(nullptr, 0) {}

DxilRuntimeData::DxilRuntimeData(const void *ptr, size_t size)
    : m_TableCount(0), m_StringReader(), m_IndexTableReader(), m_RawBytesReader(),
      m_ResourceTableReader(), m_FunctionTableReader(),
      m_SubobjectTableReader(), m_Context() {
  m_Context = {&m_StringReader, &m_IndexTableReader, &m_RawBytesReader,
               &m_ResourceTableReader, &m_FunctionTableReader,
               &m_SubobjectTableReader};
  m_ResourceTableReader.SetContext(&m_Context);
  m_FunctionTableReader.SetContext(&m_Context);
  m_SubobjectTableReader.SetContext(&m_Context);
  InitFromRDAT(ptr, size);
}

// initializing reader from RDAT. return true if no error has occured.
bool DxilRuntimeData::InitFromRDAT(const void *pRDAT, size_t size) {
  if (pRDAT) {
    try {
      CheckedReader Reader(pRDAT, size);
      RuntimeDataHeader RDATHeader = Reader.Read<RuntimeDataHeader>();
      if (RDATHeader.Version < RDAT_Version_10) {
        // Prerelease version, fallback to that Init
        return InitFromRDAT_Prerelease(pRDAT, size);
      }
      const uint32_t *offsets = Reader.ReadArray<uint32_t>(RDATHeader.PartCount);
      for (uint32_t i = 0; i < RDATHeader.PartCount; ++i) {
        Reader.Advance(offsets[i]);
        RuntimeDataPartHeader part = Reader.Read<RuntimeDataPartHeader>();
        CheckedReader PR(Reader.ReadArray<char>(part.Size), part.Size);
        switch (part.Type) {
        case RuntimeDataPartType::StringBuffer: {
          m_StringReader = StringTableReader(
            PR.ReadArray<char>(part.Size), part.Size);
          break;
        }
        case RuntimeDataPartType::IndexArrays: {
          uint32_t count = part.Size / sizeof(uint32_t);
          m_IndexTableReader = IndexTableReader(
            PR.ReadArray<uint32_t>(count), count);
          break;
        }
        case RuntimeDataPartType::RawBytes: {
          m_RawBytesReader = RawBytesReader(
            PR.ReadArray<char>(part.Size), part.Size);
          break;
        }
        case RuntimeDataPartType::ResourceTable: {
          RuntimeDataTableHeader table = PR.Read<RuntimeDataTableHeader>();
          size_t tableSize = table.RecordCount * table.RecordStride;
          m_ResourceTableReader.SetResourceInfo(PR.ReadArray<char>(tableSize),
            table.RecordCount, table.RecordStride);
          break;
        }
        case RuntimeDataPartType::FunctionTable: {
          RuntimeDataTableHeader table = PR.Read<RuntimeDataTableHeader>();
          size_t tableSize = table.RecordCount * table.RecordStride;
          m_FunctionTableReader.SetFunctionInfo(PR.ReadArray<char>(tableSize),
            table.RecordCount, table.RecordStride);
          break;
        }
        case RuntimeDataPartType::SubobjectTable: {
          RuntimeDataTableHeader table = PR.Read<RuntimeDataTableHeader>();
          size_t tableSize = table.RecordCount * table.RecordStride;
          m_SubobjectTableReader.SetSubobjectInfo(PR.ReadArray<char>(tableSize),
            table.RecordCount, table.RecordStride);
          break;
        }
        default:
          continue; // Skip unrecognized parts
        }
      }
      return true;
    } catch(CheckedReader::exception e) {
      // TODO: error handling
      //throw hlsl::Exception(DXC_E_MALFORMED_CONTAINER, e.what());
      return false;
    }
  }
  return false;
}

bool DxilRuntimeData::InitFromRDAT_Prerelease(const void *pRDAT, size_t size) {
  enum class RuntimeDataPartType_Prerelease : uint32_t {
    Invalid = 0,
    String,
    Function,
    Resource,
    Index
  };
  struct RuntimeDataTableHeader_Prerelease {
    uint32_t tableType; // RuntimeDataPartType
    uint32_t size;
    uint32_t offset;
  };
  if (pRDAT) {
    try {
      CheckedReader Reader(pRDAT, size);
      uint32_t partCount = Reader.Read<uint32_t>();
      const RuntimeDataTableHeader_Prerelease *tableHeaders =
        Reader.ReadArray<RuntimeDataTableHeader_Prerelease>(partCount);
      for (uint32_t i = 0; i < partCount; ++i) {
        uint32_t partSize = tableHeaders[i].size;
        Reader.Advance(tableHeaders[i].offset);
        CheckedReader PR(Reader.ReadArray<char>(partSize), partSize);
        switch ((RuntimeDataPartType_Prerelease)(tableHeaders[i].tableType)) {
        case RuntimeDataPartType_Prerelease::String: {
          m_StringReader = StringTableReader(
            PR.ReadArray<char>(partSize), partSize);
          break;
        }
        case RuntimeDataPartType_Prerelease::Index: {
          uint32_t count = partSize / sizeof(uint32_t);
          m_IndexTableReader = IndexTableReader(
            PR.ReadArray<uint32_t>(count), count);
          break;
        }
        case RuntimeDataPartType_Prerelease::Resource: {
          uint32_t count = partSize / sizeof(RuntimeDataResourceInfo);
          m_ResourceTableReader.SetResourceInfo(PR.ReadArray<char>(partSize),
            count, sizeof(RuntimeDataResourceInfo));
          break;
        }
        case RuntimeDataPartType_Prerelease::Function: {
          uint32_t count = partSize / sizeof(RuntimeDataFunctionInfo);
          m_FunctionTableReader.SetFunctionInfo(PR.ReadArray<char>(partSize),
            count, sizeof(RuntimeDataFunctionInfo));
          break;
        }
        default:
          return false; // There should be no unrecognized parts
        }
      }
      return true;
    } catch(CheckedReader::exception e) {
      // TODO: error handling
      //throw hlsl::Exception(DXC_E_MALFORMED_CONTAINER, e.what());
      return false;
    }
  }
  return false;
}

FunctionTableReader *DxilRuntimeData::GetFunctionTableReader() {
  return &m_FunctionTableReader;
}

ResourceTableReader *DxilRuntimeData::GetResourceTableReader() {
  return &m_ResourceTableReader;
}

SubobjectTableReader *DxilRuntimeData::GetSubobjectTableReader() {
  return &m_SubobjectTableReader;
}

}} // hlsl::RDAT

using namespace hlsl;
using namespace RDAT;

namespace std {
template <> struct hash<ResourceKey> {
  size_t operator()(const ResourceKey &key) const throw() {
    return (hash<uint32_t>()(key.Class) * (size_t)16777619U) ^
           hash<uint32_t>()(key.ID);
  }
};
} // namespace std

namespace {

class DxilRuntimeReflection_impl : public DxilRuntimeReflection {
private:
  typedef std::unordered_map<const char *, std::unique_ptr<wchar_t[]>> StringMap;
  typedef std::unordered_map<const void *, std::unique_ptr<char[]>> BytesMap;
  typedef std::vector<const wchar_t *> WStringList;
  typedef std::vector<DxilResourceDesc> ResourceList;
  typedef std::vector<DxilResourceDesc *> ResourceRefList;
  typedef std::vector<DxilFunctionDesc> FunctionList;
  typedef std::vector<DxilSubobjectDesc> SubobjectList;

  DxilRuntimeData m_RuntimeData;
  StringMap m_StringMap;
  BytesMap m_BytesMap;
  ResourceList m_Resources;
  FunctionList m_Functions;
  SubobjectList m_Subobjects;
  std::unordered_map<ResourceKey, DxilResourceDesc *> m_ResourceMap;
  std::unordered_map<DxilFunctionDesc *, ResourceRefList> m_FuncToResMap;
  std::unordered_map<DxilFunctionDesc *, WStringList> m_FuncToDependenciesMap;
  std::unordered_map<DxilSubobjectDesc *, WStringList> m_SubobjectToExportsMap;
  bool m_initialized;

  const wchar_t *GetWideString(const char *ptr);
  void AddString(const char *ptr);
  const void *GetBytes(const void *ptr, size_t size);
  void InitializeReflection();
  const DxilResourceDesc * const*GetResourcesForFunction(DxilFunctionDesc &function,
                             const FunctionReader &functionReader);
  const wchar_t **GetDependenciesForFunction(DxilFunctionDesc &function,
                             const FunctionReader &functionReader);
  const wchar_t **GetExportsForAssociation(DxilSubobjectDesc &subobject,
                             const SubobjectReader &subobjectReader);
  DxilResourceDesc *AddResource(const ResourceReader &resourceReader);
  DxilFunctionDesc *AddFunction(const FunctionReader &functionReader);
  DxilSubobjectDesc *AddSubobject(const SubobjectReader &subobjectReader);

public:
  // TODO: Implement pipeline state validation with runtime data
  // TODO: Update BlobContainer.h to recognize 'RDAT' blob
  DxilRuntimeReflection_impl()
      : m_RuntimeData(), m_StringMap(), m_BytesMap(), m_Resources(), m_Functions(),
        m_FuncToResMap(), m_FuncToDependenciesMap(), m_SubobjectToExportsMap(),
        m_initialized(false) {}
  virtual ~DxilRuntimeReflection_impl() {}
  // This call will allocate memory for GetLibraryReflection call
  bool InitFromRDAT(const void *pRDAT, size_t size) override;
  const DxilLibraryDesc GetLibraryReflection() override;
};

void DxilRuntimeReflection_impl::AddString(const char *ptr) {
  if (m_StringMap.find(ptr) == m_StringMap.end()) {
    int size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ptr, -1,
                                     nullptr, 0);
    if (size != 0) {
      std::unique_ptr<wchar_t[]> pNew(new wchar_t[size]);
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

const void *DxilRuntimeReflection_impl::GetBytes(const void *ptr, size_t size) {
  auto it = m_BytesMap.find(ptr);
  if (it != m_BytesMap.end())
    return it->second.get();

  auto inserted = m_BytesMap.insert(std::make_pair(ptr, std::unique_ptr<char[]>(new char[size])));
  void *newPtr = inserted.first->second.get();
  memcpy(newPtr, ptr, size);
  return newPtr;
}

bool DxilRuntimeReflection_impl::InitFromRDAT(const void *pRDAT, size_t size) {
  assert(!m_initialized && "may only initialize once");
  m_initialized = m_RuntimeData.InitFromRDAT(pRDAT, size);
  if (m_initialized)
    InitializeReflection();
  return m_initialized;
}

const DxilLibraryDesc DxilRuntimeReflection_impl::GetLibraryReflection() {
  DxilLibraryDesc reflection = {};
  if (m_initialized) {
    reflection.NumResources =
        m_RuntimeData.GetResourceTableReader()->GetNumResources();
    reflection.pResource = m_Resources.data();
    reflection.NumFunctions =
        m_RuntimeData.GetFunctionTableReader()->GetNumFunctions();
    reflection.pFunction = m_Functions.data();
    reflection.NumSubobjects =
        m_RuntimeData.GetSubobjectTableReader()->GetCount();
    reflection.pSubobjects = m_Subobjects.data();
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
    DxilResourceDesc *pResource = AddResource(resourceReader);
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
  const SubobjectTableReader *subobjectTableReader = m_RuntimeData.GetSubobjectTableReader();
  m_Subobjects.reserve(subobjectTableReader->GetCount());
  for (uint32_t i = 0; i < subobjectTableReader->GetCount(); ++i) {
    SubobjectReader subobjectReader = subobjectTableReader->GetItem(i);
    AddString(subobjectReader.GetName());
    AddSubobject(subobjectReader);
  }
}

DxilResourceDesc *
DxilRuntimeReflection_impl::AddResource(const ResourceReader &resourceReader) {
  assert(m_Resources.size() < m_Resources.capacity() && "Otherwise, number of resources was incorrect");
  if (!(m_Resources.size() < m_Resources.capacity()))
    return nullptr;
  m_Resources.emplace_back(DxilResourceDesc({}));
  DxilResourceDesc &resource = m_Resources.back();
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

const DxilResourceDesc * const*DxilRuntimeReflection_impl::GetResourcesForFunction(
    DxilFunctionDesc &function, const FunctionReader &functionReader) {
  if (!functionReader.GetNumResources())
    return nullptr;
  auto it = m_FuncToResMap.insert(std::make_pair(&function, ResourceRefList()));
  assert(it.second && "otherwise, collision");
  ResourceRefList &resourceList = it.first->second;
  resourceList.reserve(functionReader.GetNumResources());
  for (uint32_t i = 0; i < functionReader.GetNumResources(); ++i) {
    const ResourceReader resourceReader = functionReader.GetResource(i);
    ResourceKey key((uint32_t)resourceReader.GetResourceClass(),
                    resourceReader.GetID());
    auto it = m_ResourceMap.find(key);
    assert(it != m_ResourceMap.end() && it->second && "Otherwise, resource was not in map, or was null");
    resourceList.emplace_back(it->second);
  }
  return resourceList.data();
}

const wchar_t **DxilRuntimeReflection_impl::GetDependenciesForFunction(
    DxilFunctionDesc &function, const FunctionReader &functionReader) {
  auto it = m_FuncToDependenciesMap.insert(std::make_pair(&function, WStringList()));
  assert(it.second && "otherwise, collision");
  WStringList &wStringList = it.first->second;
  for (uint32_t i = 0; i < functionReader.GetNumDependencies(); ++i) {
    wStringList.emplace_back(GetWideString(functionReader.GetDependency(i)));
  }
  return wStringList.empty() ? nullptr : wStringList.data();
}

DxilFunctionDesc *
DxilRuntimeReflection_impl::AddFunction(const FunctionReader &functionReader) {
  assert(m_Functions.size() < m_Functions.capacity() && "Otherwise, number of functions was incorrect");
  if (!(m_Functions.size() < m_Functions.capacity()))
    return nullptr;
  m_Functions.emplace_back(DxilFunctionDesc({}));
  DxilFunctionDesc &function = m_Functions.back();
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

const wchar_t **DxilRuntimeReflection_impl::GetExportsForAssociation(
  DxilSubobjectDesc &subobject, const SubobjectReader &subobjectReader) {
  auto it = m_SubobjectToExportsMap.insert(std::make_pair(&subobject, WStringList()));
  assert(it.second && "otherwise, collision");
  WStringList &wStringList = it.first->second;
  for (uint32_t i = 0; i < subobjectReader.GetSubobjectToExportsAssociation_NumExports(); ++i) {
    wStringList.emplace_back(GetWideString(subobjectReader.GetSubobjectToExportsAssociation_Export(i)));
  }
  return wStringList.empty() ? nullptr : wStringList.data();
}

DxilSubobjectDesc *DxilRuntimeReflection_impl::AddSubobject(const SubobjectReader &subobjectReader) {
  assert(m_Subobjects.size() < m_Subobjects.capacity() && "Otherwise, number of subobjects was incorrect");
  if (!(m_Subobjects.size() < m_Subobjects.capacity()))
    return nullptr;
  m_Subobjects.emplace_back(DxilSubobjectDesc({}));
  DxilSubobjectDesc &subobject = m_Subobjects.back();
  subobject.Name = GetWideString(subobjectReader.GetName());
  subobject.Kind = (uint32_t)subobjectReader.GetKind();
  switch (subobjectReader.GetKind()) {
  case DXIL::SubobjectKind::StateObjectConfig:
    subobject.StateObjectConfig.Flags = subobjectReader.GetStateObjectConfig_Flags();
    break;
  case DXIL::SubobjectKind::GlobalRootSignature:
  case DXIL::SubobjectKind::LocalRootSignature:
    if (!subobjectReader.GetRootSignature(&subobject.RootSignature.pSerializedSignature, &subobject.RootSignature.SizeInBytes))
      return nullptr;
    subobject.RootSignature.pSerializedSignature = GetBytes(subobject.RootSignature.pSerializedSignature, subobject.RootSignature.SizeInBytes);
    break;
  case DXIL::SubobjectKind::SubobjectToExportsAssociation:
    subobject.SubobjectToExportsAssociation.Subobject =
      GetWideString(subobjectReader.GetSubobjectToExportsAssociation_Subobject());
    subobject.SubobjectToExportsAssociation.NumExports = subobjectReader.GetSubobjectToExportsAssociation_NumExports();
    subobject.SubobjectToExportsAssociation.Exports = GetExportsForAssociation(subobject, subobjectReader);
    break;
  case DXIL::SubobjectKind::RaytracingShaderConfig:
    subobject.RaytracingShaderConfig.MaxPayloadSizeInBytes = subobjectReader.GetRaytracingShaderConfig_MaxPayloadSizeInBytes();
    subobject.RaytracingShaderConfig.MaxAttributeSizeInBytes = subobjectReader.GetRaytracingShaderConfig_MaxAttributeSizeInBytes();
    break;
  case DXIL::SubobjectKind::RaytracingPipelineConfig:
    subobject.RaytracingPipelineConfig.MaxTraceRecursionDepth = subobjectReader.GetRaytracingPipelineConfig_MaxTraceRecursionDepth();
    break;
  case DXIL::SubobjectKind::HitGroup:
    subobject.HitGroup.Type = (uint32_t)subobjectReader.GetHitGroup_Type();
    subobject.HitGroup.Intersection = GetWideString(subobjectReader.GetHitGroup_Intersection());
    subobject.HitGroup.AnyHit = GetWideString(subobjectReader.GetHitGroup_AnyHit());
    subobject.HitGroup.ClosestHit = GetWideString(subobjectReader.GetHitGroup_ClosestHit());
    break;
  default:
    // Ignore contents of unrecognized subobject type (forward-compat)
    break;
  }
  return &subobject;
}

} // namespace anon

DxilRuntimeReflection *hlsl::RDAT::CreateDxilRuntimeReflection() {
  return new DxilRuntimeReflection_impl();
}

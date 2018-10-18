///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSubobject.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Defines Subobject types for DxilModule.                                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <map>
#include "DxilConstants.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/StringRef.h"

using namespace llvm;

namespace hlsl {

class DxilSubobjects;

class DxilSubobject {
public:
  using Kind = DXIL::SubobjectKind;

  DxilSubobject() = delete;
  DxilSubobject(const DxilSubobject &other) = delete;
  DxilSubobject(DxilSubobject &&other);
  ~DxilSubobject();

  DxilSubobject &operator=(const DxilSubobject &other) = delete;

  Kind GetKind() const { return m_Kind; }
  llvm::StringRef GetName() const { return m_Name; }

  // Note: strings and root signature data is owned by DxilModule
  // When creating subobjects, use canonical strings from module


  bool GetStateObjectConfig(uint32_t &Flags) const;
  bool GetRootSignature(bool local, const void * &Data, uint32_t &Size) const;
  bool GetSubobjectToExportsAssociation(llvm::StringRef &Subobject,
                                        const char * const * &Exports,
                                        uint32_t &NumExports) const;
  bool GetRaytracingShaderConfig(uint32_t &MaxPayloadSizeInBytes,
                                 uint32_t &MaxAttributeSizeInBytes) const;
  bool GetRaytracingPipelineConfig(uint32_t &MaxTraceRecursionDepth) const;
  bool GetHitGroup(llvm::StringRef &Intersection,
                   llvm::StringRef &AnyHit,
                   llvm::StringRef &ClosestHit) const;

private:
  DxilSubobject(DxilSubobjects &owner, Kind kind, llvm::StringRef name);
  DxilSubobject(DxilSubobjects &owner, const DxilSubobject &other, llvm::StringRef name);
  void CopyUnionedContents(const DxilSubobject &other);
  void InternStrings();

  DxilSubobjects &m_Owner;
  Kind m_Kind;
  llvm::StringRef m_Name;

  std::vector<const char*> m_Exports;

  union {
    struct {
      uint32_t Flags;   // DXIL::StateObjectFlags
    } StateObjectConfig;
    struct {
      uint32_t Size;
      const void *Data;
    } RootSignature;
    struct {
      const char *Subobject;
      // see m_Exports for export list
    } SubobjectToExportsAssociation;
    struct {
      uint32_t MaxPayloadSizeInBytes;
      uint32_t MaxAttributeSizeInBytes;
    } RaytracingShaderConfig;
    struct {
      uint32_t MaxTraceRecursionDepth;
    } RaytracingPipelineConfig;
    struct {
      const char *Intersection;
      const char *AnyHit;
      const char *ClosestHit;
    } HitGroup;
  };

  friend class DxilSubobjects;
};

class DxilSubobjects {
public:
  typedef llvm::MapVector< llvm::StringRef, std::string > StringStorage;
  typedef llvm::MapVector< const void*, std::vector<char> > RawBytesStorage;
  typedef llvm::MapVector< llvm::StringRef, std::unique_ptr<DxilSubobject> > SubobjectStorage;
  using Kind = DXIL::SubobjectKind;

  DxilSubobjects();
  DxilSubobjects(const DxilSubobjects &other) = delete;
  DxilSubobjects(DxilSubobjects &&other);
  ~DxilSubobjects();

  DxilSubobjects &operator=(const DxilSubobjects &other) = delete;

  // Add/find string in owned subobject strings, returning canonical ptr
  llvm::StringRef GetSubobjectString(StringRef value);
  // Add/find raw bytes, returning canonical ptr
  const void *GetRawBytes(const void *ptr, size_t size);
  DxilSubobject *FindSubobject(StringRef name);
  void RemoveSubobject(StringRef name);
  DxilSubobject &CloneSubobject(const DxilSubobject &Subobject, llvm::StringRef Name);
  const SubobjectStorage &GetSubobjects() const { return m_Subobjects;  }

  // Create DxilSubobjects

  DxilSubobject &CreateStateObjectConfig(llvm::StringRef Name,
                                         uint32_t Flags);
  // Local/Global RootSignature
  DxilSubobject &CreateRootSignature(llvm::StringRef Name,
                                     bool local,
                                     const void *Data,
                                     uint32_t Size);
  DxilSubobject &CreateSubobjectToExportsAssociation(
    llvm::StringRef Name,
    llvm::StringRef Subobject, const char * const *Exports, uint32_t NumExports);
  DxilSubobject &CreateRaytracingShaderConfig(
    llvm::StringRef Name,
    uint32_t MaxPayloadSizeInBytes,
    uint32_t MaxAttributeSizeInBytes);
  DxilSubobject &CreateRaytracingPipelineConfig(
    llvm::StringRef Name,
    uint32_t MaxTraceRecursionDepth);
  DxilSubobject &CreateHitGroup(llvm::StringRef Name,
                                llvm::StringRef Intersection,
                                llvm::StringRef AnyHit,
                                llvm::StringRef ClosestHit);

private:
  DxilSubobject &CreateSubobject(Kind kind, llvm::StringRef Name);

  StringStorage m_StringStorage;
  RawBytesStorage m_RawBytesStorage;
  SubobjectStorage m_Subobjects;
};

} // namespace hlsl

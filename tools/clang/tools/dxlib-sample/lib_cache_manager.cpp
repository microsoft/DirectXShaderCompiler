///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// lib_cache_manager.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements lib blob cache manager.                                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Hashing.h"
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include "lib_share_helper.h"

using namespace llvm;
using namespace libshare;

namespace {

struct KeyHash {
  std::size_t operator()(const hash_code &k) const { return k; }
};
struct KeyEqual {
  bool operator()(const hash_code &l, const hash_code &r) const {
    return l == r;
  }
};

class LibCacheManagerImpl : public libshare::LibCacheManager {
public:
  ~LibCacheManagerImpl() {}
  HRESULT AddLibBlob(IDxcBlob *pSource, CompileInput &compiler, size_t &hash,
                     IDxcBlob **pResultLib,
                     std::function<void(void)> compileFn) override;
  bool GetLibBlob(IDxcBlob *pSource, CompileInput &compiler, size_t &hash,
                  IDxcBlob **pResultLib) override;
  void Release() { m_libCache.clear(); }

private:
  hash_code GetHash(IDxcBlob *pSource, CompileInput &compiler);
  using libCacheType =
      std::unordered_map<hash_code, CComPtr<IDxcBlob>, KeyHash, KeyEqual>;
  libCacheType m_libCache;
  std::shared_mutex m_mutex;
};

static hash_code CombineWStr(hash_code hash, LPCWSTR Arg) {
  CW2A pUtf8Arg(Arg, CP_UTF8);
  unsigned length = strlen(pUtf8Arg.m_psz);
  return hash_combine(hash, StringRef(pUtf8Arg.m_psz, length));
}

hash_code LibCacheManagerImpl::GetHash(IDxcBlob *pSource, CompileInput &compiler) {
  hash_code libHash = hash_value(
      StringRef((char *)pSource->GetBufferPointer(), pSource->GetBufferSize()));
  // Combine compile input.
  for (auto &Arg : compiler.arguments) {
    libHash = CombineWStr(libHash, Arg);
  }
  for (auto &Define : compiler.defines) {
    libHash = CombineWStr(libHash, Define.Name);
    if (Define.Value) {
      libHash = CombineWStr(libHash, Define.Value);
    }
  }
  return libHash;
}

bool LibCacheManagerImpl::GetLibBlob(IDxcBlob *pSource, CompileInput &compiler,
                                 size_t &hash, IDxcBlob **pResultLib) {
  if (!pSource || !pResultLib) {
    return false;
  }
  // Create hash from source.
  hash_code libHash = GetHash(pSource, compiler);
  hash = libHash;
  // lock
  std::shared_lock<std::shared_mutex> lk(m_mutex);

  auto it = m_libCache.find(libHash);
  if (it != m_libCache.end()) {
    *pResultLib = it->second;
    return true;
  } else {
    return false;
  }
}

HRESULT
LibCacheManagerImpl::AddLibBlob(IDxcBlob *pSource, CompileInput &compiler,
                            size_t &hash, IDxcBlob **pResultLib,
                            std::function<void(void)> compileFn) {
  if (!pSource || !pResultLib) {
    return E_FAIL;
  }

  std::unique_lock<std::shared_mutex> lk(m_mutex);

  auto it = m_libCache.find(hash);
  if (it != m_libCache.end()) {
    *pResultLib = it->second;
    return S_OK;
  }

  compileFn();

  m_libCache[hash] = *pResultLib;

  return S_OK;
}

LibCacheManager *GetLibCacheManagerPtr(bool bFree) {
  static std::unique_ptr<LibCacheManagerImpl> g_LibCache =
      std::make_unique<LibCacheManagerImpl>();
  if (bFree)
    g_LibCache.reset();
  return g_LibCache.get();
}

} // namespace

LibCacheManager &LibCacheManager::GetLibCacheManager() {
  return *GetLibCacheManagerPtr(/*bFree*/false);
}

void LibCacheManager::ReleaseLibCacheManager() {
  GetLibCacheManagerPtr(/*bFree*/true);
}
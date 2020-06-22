///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// D3DReflectionDumper.h                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Use this to dump D3D Reflection data for testing.                         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/Support/Global.h"
#include <algorithm>
#include <string>
#include <ostream>
#include <iomanip>
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"
#include <d3d12shader.h>
#include "dxc/DxilContainer/DxilContainer.h"

namespace refl_dump {

LPCSTR ToString(D3D_CBUFFER_TYPE CBType);
LPCSTR ToString(D3D_SHADER_INPUT_TYPE Type);
LPCSTR ToString(D3D_RESOURCE_RETURN_TYPE ReturnType);
LPCSTR ToString(D3D_SRV_DIMENSION Dimension);
LPCSTR ToString(D3D_PRIMITIVE_TOPOLOGY GSOutputTopology);
LPCSTR ToString(D3D_PRIMITIVE InputPrimitive);
LPCSTR ToString(D3D_TESSELLATOR_OUTPUT_PRIMITIVE HSOutputPrimitive);
LPCSTR ToString(D3D_TESSELLATOR_PARTITIONING HSPartitioning);
LPCSTR ToString(D3D_TESSELLATOR_DOMAIN TessellatorDomain);
LPCSTR ToString(D3D_SHADER_VARIABLE_CLASS Class);
LPCSTR ToString(D3D_SHADER_VARIABLE_TYPE Type);
LPCSTR ToString(D3D_SHADER_VARIABLE_FLAGS Flag);
LPCSTR ToString(D3D_SHADER_INPUT_FLAGS Flag);
LPCSTR ToString(D3D_SHADER_CBUFFER_FLAGS Flag);
LPCSTR ToString(D3D_PARAMETER_FLAGS Flag);

template<typename _T>
struct EnumValue {
public:
  EnumValue(const _T &e) : eValue(e) {}
  _T eValue;
};

template<typename _T, typename _StoreT = uint32_t>
struct FlagsValue {
public:
  FlagsValue(const _StoreT &f) : Flags(f) {}
  _StoreT Flags;
};

template<typename _T>
std::ostream& operator<<(std::ostream& out, const EnumValue<_T> &obj) {
  if (LPCSTR szValue = ToString(obj.eValue))
    return out << szValue;
  else
    return out << "<unknown: " << std::hex << std::showbase << (UINT)obj.eValue << ">";
}

template<typename _T, typename _StoreT>
std::ostream& operator<<(std::ostream& out, const FlagsValue<_T, _StoreT> &obj) {
  _StoreT Flags = obj.Flags;
  if (!Flags) {
    LPCSTR szValue = ToString((_T)0);
    if (szValue)
      return out << "0 (" << szValue << ")";
    else
      return out << "0";
  }
  uint32_t flag = 0;
  out << "(";
  while (Flags) {
    if (flag)
      out << " | ";
    flag = (Flags & ~(Flags - 1));
    Flags ^= flag;
    out << EnumValue<_T>((_T)flag);
  }
  out << ")";
  return out;
}

class DumperBase {
private:
  std::ostream &m_out;
  unsigned m_indent = 0;
  bool m_bCheckByName = false;
  std::ostream &DoIndent() {
    return m_out << std::setfill(' ')
      << std::setw(std::min(m_indent * 2, (unsigned)32))
      << "";
  }

public:
  DumperBase(std::ostream &outStream) : m_out(outStream) {}

  void Indent() { if (m_indent < (1 << 30)) m_indent++; }
  void Dedent() { if (m_indent > 0) m_indent--; }

  template<typename _T>
  std::ostream &Write(std::ostream &out, _T t) {
    return out << t;
  }
  template<typename _T, typename... Args>
  std::ostream &Write(std::ostream &out, _T t, Args... args) {
    return Write(out << t, args...);
  }

  template<typename _T>
  std::ostream &WriteLn(_T t) {
    return Write(DoIndent(), t) << std::endl
      << std::resetiosflags(std::ios_base::basefield | std::ios_base::showbase);
  }
  template<typename _T, typename... Args>
  std::ostream &WriteLn(_T t, Args... args) {
    return Write(Write(DoIndent(), t), args...) << std::endl
      << std::resetiosflags(std::ios_base::basefield | std::ios_base::showbase);
  }

  template <typename _T>
  std::ostream &WriteEnumValue(_T eValue) {
    LPCSTR szValue = ToString(eValue);
    if (szValue)
      return Write(szValue);
    else
      return Write("<unknown: ", std::hex, std::showbase, (UINT)eValue, ">");
  }

  template<typename _T>
  void DumpEnum(const char *Name, _T eValue) {
    WriteLn(Name, ": ", EnumValue<_T>(eValue));
  }
  template<typename _T, typename _StoreT = uint32_t>
  void DumpFlags(const char *Name, _StoreT Flags) {
    WriteLn(Name, ": ", FlagsValue<_T, _StoreT>(Flags));
  }

  template<typename... Args>
  void Failure(Args... args) {
    WriteLn("Failed: ", args...);
  }

};

class D3DReflectionDumper : public DumperBase {
private:
  bool m_bCheckByName = false;
  const char *m_LastName = nullptr;
  void SetLastName(const char *Name = nullptr) { m_LastName = Name ? Name : "<nullptr>"; }

public:
  D3DReflectionDumper(std::ostream &outStream) : DumperBase(outStream) {}
  void SetCheckByName(bool bCheckByName) { m_bCheckByName = bCheckByName; }

  void DumpShaderVersion(UINT Version);
  void DumpDefaultValue(LPCVOID pDefaultValue, UINT Size);

  void Dump(D3D12_SHADER_TYPE_DESC &tyDesc);
  void Dump(D3D12_SHADER_VARIABLE_DESC &varDesc);
  void Dump(D3D12_SHADER_BUFFER_DESC &Desc);
  void Dump(D3D12_SHADER_INPUT_BIND_DESC &resDesc);
  void Dump(D3D12_SHADER_DESC &Desc);
  void Dump(D3D12_FUNCTION_DESC &Desc);
  void Dump(D3D12_LIBRARY_DESC &Desc);

  void Dump(ID3D12ShaderReflectionType *pType);
  void Dump(ID3D12ShaderReflectionVariable *pVar);

  void Dump(ID3D12ShaderReflectionConstantBuffer *pCBReflection);

  void Dump(ID3D12ShaderReflection *pShaderReflection);
  void Dump(ID3D12FunctionReflection *pFunctionReflection);
  void Dump(ID3D12LibraryReflection *pLibraryReflection);

};

} // namespace refl_dump

//===- ClassDefinitionDumper.h - --------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ClassDefinitionDumper.h                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_TOOLS_LLVMPDBDUMP_CLASSDEFINITIONDUMPER_H
#define LLVM_TOOLS_LLVMPDBDUMP_CLASSDEFINITIONDUMPER_H

#include "llvm/DebugInfo/PDB/PDBSymDumper.h"
#include "llvm/DebugInfo/PDB/PDBSymbolFunc.h"
#include "llvm/DebugInfo/PDB/PDBSymbolData.h"

#include <list>
#include <memory>
#include <unordered_map>

namespace llvm {

class LinePrinter;

class ClassDefinitionDumper : public PDBSymDumper {
public:
  ClassDefinitionDumper(LinePrinter &P);

  void start(const PDBSymbolTypeUDT &Exe);

  void dump(const PDBSymbolTypeBaseClass &Symbol) override;
  void dump(const PDBSymbolData &Symbol) override;
  void dump(const PDBSymbolTypeEnum &Symbol) override;
  void dump(const PDBSymbolFunc &Symbol) override;
  void dump(const PDBSymbolTypeTypedef &Symbol) override;
  void dump(const PDBSymbolTypeUDT &Symbol) override;
  void dump(const PDBSymbolTypeVTable &Symbol) override;

private:
  LinePrinter &Printer;

  struct SymbolGroup {
    SymbolGroup() {}
    SymbolGroup(SymbolGroup &&Other) {
      Functions = std::move(Other.Functions);
      Data = std::move(Other.Data);
      Unknown = std::move(Other.Unknown);
    }

    std::list<std::unique_ptr<PDBSymbolFunc>> Functions;
    std::list<std::unique_ptr<PDBSymbolData>> Data;
    std::list<std::unique_ptr<PDBSymbol>> Unknown;
    SymbolGroup(const SymbolGroup &other) = delete;
    SymbolGroup &operator=(const SymbolGroup &other) = delete;
  };
  typedef std::unordered_map<int, SymbolGroup> SymbolGroupByAccess;

  int dumpAccessGroup(PDB_MemberAccess Access, const SymbolGroup &Group);
};
}

#endif

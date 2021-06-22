///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilResourceBinding.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/ADT/Twine.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include "dxc/DxilResourceBinding/DxilResourceBinding.h"
#include "dxc/DXIL/DxilMetadataHelper.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilResourceBase.h"

#include <unordered_set>

using namespace llvm;
using namespace hlsl;

bool hlsl::ParseResourceBindingFile(llvm::StringRef fileName, llvm::StringRef content, llvm::raw_ostream &errors, ResourceBinding *outBinding) {

  struct Parser {
    StringRef fileName;
    const char *curr = nullptr;
    const char *end  = nullptr;
    int line = 1;
    int col  = 1;
    llvm::raw_ostream &errors;
    bool WasNewline = false;

    struct Location {
      int line = 0;
      int col = 0;
    };

    inline static bool IsDelimiter(char c) {
      return c == ',' || c == '\n' || c == '\r';
    }
    inline static bool IsNewline(char c) {
       return c == '\r' || c == '\n';
    }
    inline  static bool IsWhitespace(char c) {
      return c == ' ' || c == '\t';
    }
    inline Parser(StringRef fileName, StringRef content, llvm::raw_ostream &errors) :
      fileName(fileName),
      curr(content.data()),
      end(content.data() + content.size()),
      errors(errors)
    {
      EatWhiteSpaceAndNewlines();
    }
    inline bool WasJustEndOfLine() const {
      return WasNewline;
    }

    inline void EatWhitespace() {
      for (;;) {
        if (IsWhitespace(Peek()))
          Advance();
        else
          break;
      }
    }
    inline void EatWhiteSpaceAndNewlines() {
      for (;;) {
        if (IsWhitespace(Peek()) || IsNewline(Peek()))
          Advance();
        else
          break;
      }
    }
    inline Location GetLoc() const {
      Location loc;
      loc.line = line;
      loc.col = col;
      return loc;
    }
    inline void Advance() {
      if (ReachedEnd())
        return;
      if (*curr == '\n') {
        line++;
        col = 1;
      }
      else if (*curr != '\r') {
        col++;
      }
      curr++;
    }
    inline bool ReachedEnd() const {
      return curr >= end || *curr == '\0';
    }
    inline void Warn(Location loc, const Twine &err) {
      (void)Error(loc, err);
    }
    inline bool Error(Location loc, const Twine &err) {
      errors << (Twine(fileName) + ":" + Twine(loc.line) + ":" + Twine(loc.col) + ": " + err + "\n").str();
      return false;
    }
    inline bool Error(const Twine &err) {
      Error(GetLoc(), err);
      return false;
    }
    inline char Peek() const {
      if (ReachedEnd()) return '\0';
      return *curr;
    }

    inline bool ParseCell(SmallVectorImpl<char> *str) {
      EatWhitespace();

      if (ReachedEnd()) {
        return Error("Unexpected EOF when parsing cell.");
      }

      bool hasQuote = false;
      if (Peek() == '"') {
        hasQuote = true;
        Advance();
      }

      while (!ReachedEnd()) {
        if (IsDelimiter(Peek())) {
          if (hasQuote)
            return Error("Expected end quote '\"'.");
          // Trim the white space at the end of the string
          while (str->size() && IsWhitespace(str->back())) {
            str->pop_back();
          }
          break;
        }
        // Double quotes
        if (Peek() == '"') {
          Advance();
          if (!hasQuote)
            return Error("'\"' not allowed in non-quoted cell.");
          EatWhitespace();
          if (!IsDelimiter(Peek())) {
            return Error("Unexpected character after quote.");
          }
          break;
        }

        str->push_back(Peek());
        Advance();
      }

      // Handle delimiter
      {
        // If this delimiter is not a newline, set our newline flag to false.
        if (!IsNewline(Peek())) {
          WasNewline = false;
          Advance();

          // Eat white spaces so we can detect the next newline if this
          // is a trailing comma.
          EatWhitespace();
        }

        if (IsNewline(Peek())) {
          WasNewline = true;
          EatWhiteSpaceAndNewlines();
        }
      }

      return true;
    }

    enum IntegerConversionStatus {
      Success,
      OutOfBounds,
      Invalid,
      Empty,
    };
    inline static IntegerConversionStatus ToUnsigned32(StringRef str, uint32_t *outInteger) {
      *outInteger = 0;

      if (str.empty())
        return IntegerConversionStatus::Empty;

      llvm::APInt integer;
      if (llvm::StringRef(str).getAsInteger(0, integer)) {
        return IntegerConversionStatus::Invalid;
      }

      if (integer.getBitWidth() > 32) {
        return IntegerConversionStatus::OutOfBounds;
      }

      *outInteger = (uint32_t)integer.getLimitedValue();
      return IntegerConversionStatus::Success;
    }

    inline bool ParseResourceIndex(hlsl::DXIL::ResourceClass *outClass, unsigned *outIndex) {

      *outClass = hlsl::DXIL::ResourceClass::Invalid;
      *outIndex = UINT_MAX;

      auto loc = GetLoc();
      SmallString<32> str;
      if (!ParseCell(&str))
        return false;

      if (str.empty()) {
        return Error(loc, "Resource index cannot be empty.");
      }

      switch (str[0]) {
      case 'b':
        *outClass = hlsl::DXIL::ResourceClass::CBuffer;
        break;
      case 's':
        *outClass = hlsl::DXIL::ResourceClass::Sampler;
        break;
      case 't':
        *outClass = hlsl::DXIL::ResourceClass::SRV;
        break;
      case 'u':
        *outClass = hlsl::DXIL::ResourceClass::UAV;
        break;
      default:
        return Error(loc, "Invalid resource class. Needs to be one of 'b', 's', 't', or 'u'.");
        break;
      }

      StringRef integer( &str[1], str.size() - 1);
      if (auto result = ToUnsigned32(integer, outIndex)) {
        switch (result) {
        case IntegerConversionStatus::OutOfBounds:
          return Error(loc, Twine() + integer + " is out of bounds of 32-bit integer");
        default:
          return Error(loc, Twine() + "'" + integer + "' is an invalid 32-bit integer.");
        }
      }

      return true;
    }

    inline bool ParseUnsignedCell(unsigned *outResult) {
      auto loc = GetLoc();
      SmallString<32> str;
      if (!ParseCell(&str))
        return false;

      if (str.empty()) {
        return Error(loc, "Expected unsigned integer, but got empty cell.");
      }

      llvm::APInt integer;
      if (llvm::StringRef(str).getAsInteger(0, integer)) {
        return Error(loc, Twine("'") + str + "' is not a valid unsigned integer.");
      }

      if (integer.getBitWidth() > 32) {
        return Error(loc, Twine() + "'" + str + "' is out of range of an 32-bit unsigned integer.");
      }

      *outResult = (uint32_t)integer.getLimitedValue();
      return true;
    }
  };

  Parser P(fileName, content, errors);

  enum class ColumnType {
    Name,
    Index,
    Space,
    Unknown,
  };

  llvm::SmallVector<ColumnType, 5> columns;
  std::unordered_set<ColumnType> columnsSet;

  for (;;) {
    llvm::SmallString<32> column;
    if (!P.ParseCell(&column)) {
      return false;
    }

    for (char &c : column)
      c = std::tolower(c);

    auto loc = P.GetLoc();
    if (column == "name") {
      if (!columnsSet.insert(ColumnType::Name).second) {
        return P.Error(loc, "Column 'Name' already specified.");
      }
      columns.push_back(ColumnType::Name);
    }
    else if (column == "index") {
      if (!columnsSet.insert(ColumnType::Index).second) {
        return P.Error(loc, "Column 'Index' already specified.");
      }
      columns.push_back(ColumnType::Index);
    }
    else if (column == "space") {
      if (!columnsSet.insert(ColumnType::Space).second) {
        return P.Error(loc, "Column 'Space' already specified.");
      }
      columns.push_back(ColumnType::Space);
    }
    else {
      P.Warn(loc, Twine() + "Unknown column '" + column + "'");
      columns.push_back(ColumnType::Unknown);
    }

    if (P.WasJustEndOfLine())
      break;
  }

  if (!columnsSet.count(ColumnType::Name)  ||
      !columnsSet.count(ColumnType::Index) ||
      !columnsSet.count(ColumnType::Space))
  {
    return P.Error(Twine() + "Need at least 'Name', 'Index', and 'Space'.");
  }

  while (!P.ReachedEnd()) {

    SmallString<32> name;
    hlsl::DXIL::ResourceClass cls = hlsl::DXIL::ResourceClass::Invalid;
    SmallString<32> unknown;
    unsigned index = 0;
    unsigned space = 0;

    for (unsigned i = 0; i < columns.size(); i++) {
      ColumnType column = columns[i];
      switch (column) {
      case ColumnType::Name:
      {
        if (!P.ParseCell(&name))
          return false;
      } break;

      case ColumnType::Index:
      {
        if (!P.ParseResourceIndex(&cls, &index))
          return false;
      } break;

      case ColumnType::Space:
      {
        if (!P.ParseUnsignedCell(&space))
          return false;
      } break;
      default:
      {
        if (!P.ParseCell(&unknown))
          return false;
      } break;
      }

      if (P.WasJustEndOfLine() && i+1 != columns.size()) {
        return P.Error("Row ended after just " + Twine(i+1) + " columns. Expected " + Twine(columns.size()) + ".");
      }
    }

    ResourceBinding::Entry entry;
    entry.space = space;
    entry.index = index;

    outBinding->entries[ResourceBinding::Key(name.c_str(), cls)] = entry;

    if (!P.WasJustEndOfLine()) {
      return P.Error("Unexpected cell at the end of row. There should only be "
        + Twine(columns.size()) + " columns");
    }
  }

  return true;
}

typedef std::pair<std::string, hlsl::DXIL::ResourceClass> ResourceKey;
typedef std::map<ResourceKey, DxilResourceBase *> ResourceMap;

template<typename T>
static inline void GatherResources(const std::vector<std::unique_ptr<T> > &List, ResourceMap *Map) {
  for (const std::unique_ptr<T> &ptr : List) {
    (*Map)[ResourceKey(ptr->GetGlobalName(), ptr->GetClass())] = ptr.get();
  }
}

void hlsl::WriteResourceBindingToMetadata(llvm::Module &M, const ResourceBinding &bindings) {
  llvm::NamedMDNode *bindingsMD = M.getOrInsertNamedMetadata(hlsl::DxilMDHelper::kDxilResourceBindingMDName);
  LLVMContext &LLVMCtx = M.getContext();

  // Don't add operands repeatedly
  if (bindingsMD->getNumOperands()) {
    return;
  }

  for (const std::pair<std::pair<std::string, hlsl::DXIL::ResourceClass>, ResourceBinding::Entry> &binding : bindings.entries) {

    auto GetInt32MD = [&LLVMCtx](uint32_t val) -> llvm::ValueAsMetadata* {
      return llvm::ValueAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt32Ty(LLVMCtx), val));
    };

    llvm::Metadata *operands[4] = {};
    operands[hlsl::DxilMDHelper::kDxilResourceBindingName]  = llvm::MDString::get(LLVMCtx, binding.first.first);
    operands[hlsl::DxilMDHelper::kDxilResourceBindingClass] = GetInt32MD((unsigned)binding.first.second);
    operands[hlsl::DxilMDHelper::kDxilResourceBindingIndex] = GetInt32MD(binding.second.index);
    operands[hlsl::DxilMDHelper::kDxilResourceBindingSpace] = GetInt32MD(binding.second.space);

    llvm::MDTuple *entry = llvm::MDNode::get(LLVMCtx, operands);
    bindingsMD->addOperand(entry);
  }
}

void hlsl::ApplyResourceBindingOverridesFromMetadata(DxilModule &DM) {
  Module &M = *DM.GetModule();
  NamedMDNode *bindings = M.getNamedMetadata(hlsl::DxilMDHelper::kDxilResourceBindingMDName);
  if (!bindings)
    return;

  ResourceMap resourceMap;
  GatherResources(DM.GetCBuffers(), &resourceMap);
  GatherResources(DM.GetSRVs(),     &resourceMap);
  GatherResources(DM.GetUAVs(),     &resourceMap);
  GatherResources(DM.GetSamplers(), &resourceMap);

  for (MDNode *mdEntry : bindings->operands()) {

    Metadata *nameMD  = mdEntry->getOperand(DxilMDHelper::kDxilResourceBindingName);
    Metadata *classMD = mdEntry->getOperand(DxilMDHelper::kDxilResourceBindingClass);
    Metadata *indexMD = mdEntry->getOperand(DxilMDHelper::kDxilResourceBindingIndex);
    Metadata *spaceMD = mdEntry->getOperand(DxilMDHelper::kDxilResourceBindingSpace);

    StringRef name = cast<MDString>(nameMD)->getString();
    hlsl::DXIL::ResourceClass cls =
      (hlsl::DXIL::ResourceClass)cast<ConstantInt>(cast<ValueAsMetadata>(classMD)->getValue())->getLimitedValue();
    unsigned index = cast<ConstantInt>(cast<ValueAsMetadata>(indexMD)->getValue())->getLimitedValue();
    unsigned space = cast<ConstantInt>(cast<ValueAsMetadata>(spaceMD)->getValue())->getLimitedValue();

    auto it = resourceMap.find(ResourceKey(name, cls));
    if (it != resourceMap.end()) {
      DxilResourceBase *resource = it->second;
      if (!resource->IsAllocated()) {
        resource->SetLowerBound(index);
        resource->SetSpaceID(space);
      }
    }
  }
}


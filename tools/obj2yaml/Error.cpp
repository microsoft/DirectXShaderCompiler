//===- Error.cpp - system_error extensions for obj2yaml ---------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Error.cpp                                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "Error.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {
class _obj2yaml_error_category : public std::error_category {
public:
  const char *name() const LLVM_NOEXCEPT override;
  std::string message(int ev) const override;
};
} // namespace

const char *_obj2yaml_error_category::name() const LLVM_NOEXCEPT {
  return "obj2yaml";
}

std::string _obj2yaml_error_category::message(int ev) const {
  switch (static_cast<obj2yaml_error>(ev)) {
  case obj2yaml_error::success:
    return "Success";
  case obj2yaml_error::file_not_found:
    return "No such file.";
  case obj2yaml_error::unrecognized_file_format:
    return "Unrecognized file type.";
  case obj2yaml_error::unsupported_obj_file_format:
    return "Unsupported object file format.";
  }
  llvm_unreachable("An enumerator of obj2yaml_error does not have a message "
                   "defined.");
}

namespace llvm {
  const std::error_category &obj2yaml_category() {
  static _obj2yaml_error_category o;
  return o;
}
} // namespace llvm

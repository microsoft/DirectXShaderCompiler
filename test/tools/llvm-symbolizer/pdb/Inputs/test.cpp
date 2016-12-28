//===- test.cpp --------------------------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// test.cpp                                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
// To generate the corresponding EXE/PDB, run:                               //
// cl /Zi test.cpp                                                           //

namespace NS {
struct Foo {
  void bar() {}
};
}

void foo() {
}

int main() {
  foo();
  
  NS::Foo f;
  f.bar();
}

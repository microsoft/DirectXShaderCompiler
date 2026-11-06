// RUN: %dxc -T lib_6_3 -HV 202x %s -verify

template <typename T> T NoImportAtDefinition(T value) {
  return sin(value); // expected-error{{use of undeclared identifier 'sin'}}
}

using namespace hlsl;

float InstantiateWithoutImport(float value) {
  return NoImportAtDefinition(value); // expected-note{{in instantiation of function template specialization}}
}

// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// The removed keywords are identifiers in HLSL 202x.
float shared = 1;
float uniform = shared;

// Uses of the old modifier syntax now produce normal parsing diagnostics.
shared float globalShared; // expected-error {{unknown type name 'shared'}} expected-error {{expected unqualified-id}}
uniform float globalUniform; // expected-error {{unknown type name 'uniform'}} expected-error {{expected unqualified-id}}

float useRemovedParameter(uniform float value); // expected-error {{unknown type name 'uniform'}} expected-error {{expected ')'}} expected-note {{to match this '('}}

float readIdentifiers() {
  return shared + uniform;
}

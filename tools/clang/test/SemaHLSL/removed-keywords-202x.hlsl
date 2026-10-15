// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// The removed keywords are identifiers in HLSL 202x. These are declared
// 'static' so they are not shader constants, which cannot be initialized.
static float shared = 1;
static float uniform = shared;
static float interface = uniform;

// Uses of the old modifier syntax now produce normal parsing diagnostics.
shared float globalShared; // expected-error {{unknown type name 'shared'}} expected-error {{expected unqualified-id}}
uniform float globalUniform; // expected-error {{unknown type name 'uniform'}} expected-error {{expected unqualified-id}}
interface RemovedInterface {}; // expected-error {{unknown type name 'interface'}} expected-error {{expected ';' after top level declarator}}

float useRemovedParameter(uniform float value); // expected-error {{unknown type name 'uniform'}} expected-error {{expected ')'}} expected-note {{to match this '('}}

float readIdentifiers() {
  return shared + uniform + interface;
}

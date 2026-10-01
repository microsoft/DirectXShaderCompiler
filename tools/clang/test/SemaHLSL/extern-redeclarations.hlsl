// RUN: %dxc -T lib_6_3 -verify %s

extern float externThenDefinition; // expected-note {{previous definition is here}}
float externThenDefinition; // expected-error {{redefinition of 'externThenDefinition'}}

float definitionThenExtern; // expected-note {{previous definition is here}}
extern float definitionThenExtern; // expected-error {{redefinition of 'definitionThenExtern'}}

extern float duplicateExtern; // expected-note {{previous definition is here}}
extern float duplicateExtern; // expected-error {{redefinition of 'duplicateExtern'}}

extern Texture2D externTextureThenDefinition; // expected-note {{previous definition is here}}
Texture2D externTextureThenDefinition; // expected-error {{redefinition of 'externTextureThenDefinition'}}

Texture2D textureDefinitionThenExtern; // expected-note {{previous definition is here}}
extern Texture2D textureDefinitionThenExtern; // expected-error {{redefinition of 'textureDefinitionThenExtern'}}

extern float mismatched; // expected-note {{previous declaration is here}}
int mismatched; // expected-error {{redefinition of 'mismatched' with a different type}}

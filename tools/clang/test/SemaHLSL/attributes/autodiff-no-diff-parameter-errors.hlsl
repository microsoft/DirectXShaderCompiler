// RUN: %dxc -T lib_6_9 -HV 2021 -verify %s

[[dxc::NoDiff]] float function_subject(float x) { // expected-error {{'NoDiff' attribute only applies to parameters}}
  return x;
}

struct [[dxc::NoDiff]] RecordSubject { // expected-error {{'NoDiff' attribute only applies to parameters}}
  float value;
};

[[dxc::NoDiff]] float global_subject; // expected-error {{'NoDiff' attribute only applies to parameters}}

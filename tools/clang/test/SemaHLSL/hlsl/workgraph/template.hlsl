// RUN: %dxc -Tlib_6_8 -verify %s

// GroupNodeInputRecords may not have zero size record on DependentType.
// GroupNodeInputRecords may not non-struct/class DependentType. 

template<typename T>
struct Record {  }; // expected-note{{zero sized record defined here}}

template <typename T> void foo(GroupNodeInputRecords<T> data) {}

template <typename T> void bar() {
  GroupNodeInputRecords<Record<T> > data; // expected-error{{record used in GroupNodeInputRecords may not have zero size}}
  foo(data);
}

template <typename T> void bar2() {
  GroupNodeInputRecords<T > data; // expected-error{{'float' cannot be used as a type parameter where a struct/class is required}}
  foo(data);
}

void test() {
  bar<float>(); // expected-note{{in instantiation of function template specialization 'bar<float>' requested here}}
  bar2<float>();// expected-note{{in instantiation of function template specialization 'bar2<float>' requested here}}
}

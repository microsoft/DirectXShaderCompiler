 //RUN: %dxc -T cs_6_8 -verify %s

template<typename T>
struct TFoo
{
    T x;
};
typedef TFoo<float> Foo; 

// expected-no-diagnostics
ConstantBuffer<Foo> CB;

[numthreads(1, 1, 1)]
void main()
{   
}



Secure HLSL Specification

Contents

1. Introduction
2. Basics
3. Variables and Types
4. Operators and Expressions
5. Statements
6. Builtin identifiers
7. Grammar
8. Errors
9. Open Issues

# Variables and Types

All variables and functions must be declared before being referenced. Variables and functions are referenced with an Identifier. There is no concept of a default type - all variable declarations and function declarations must specify types.

Identifier tokens are defined in 2.TODO. The grammar in section 7 shows the variety of declaration statement syntax. Multiple variables can be declared in a single statement, and declarations can initialize most types.

Types can be split into three categories - basic types, object types and user defined types.

## Basic Types

Type | Description
--- | ---
void                | Only valid when describing the return type of a function
bool                | Boolean value that can only be true or false
int                 | Scalar 32 bit signed integer
uint                | Scalar 32 bit unsigned integer
dword               | Scalar 32 bit unsigned integer
float               | Scalar 32 bit float
&lt;type&gt;*N*     | Vector of *type* with dimension N (1 <= N <= 4)
&lt;type&gt;*N*x*M* | Matrix of *type* with N rows and M columns (1 <= N &vert; M <= 4)

There is no string type in Secure HLSL.

## Object Types

Type | Description
--- | ---
SamplerState            | Texture sampler
SamplerComparisonState  | Texture sampler with comparison
Texture*N*D             | Texture with N dimensions

## User Defined Types

User defined types are defined with the struct keyword and allow the aggregation of basic types and other structures. The members can be arrays.

## Arrays

All types can be declared with arrays with positive nonzero integer size. Only single dimensional arrays are allowed.

# Operators and Expressions

## Array subscripting

The square bracket operator [*index*] is used to access an element of an array.

Accessing an index that is out of bounds for the declared array results in a zeroed result being returned. If the variable is a struct then
the members of the struct are all zeroed out.

Accessing a texture or sampler array with an out of bounds index will result in a *null object* being returned. Any subsequent operations
done on this object result in a zeroed output being returned.

# Open Issues

- Should half / double / min16 / min10 be part of secure HLSL?
- Vector&lt;type, size&gt; vs &lt;type&gt;N?
- How many variants of vector / matrix declaration will be allowed?
- Interpolation modifiers?
- Do structs hide previous type names?
- Local struct definitions?
- Array access with non-int types?
- Multidimensional arrays?
- Is null object the way we want to handle these cases?
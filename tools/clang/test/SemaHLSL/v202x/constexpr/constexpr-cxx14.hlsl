// RUN: %dxc -T lib_6_3 -HV 202x -Wno-c++14-extensions -verify %s

// expected-no-diagnostics

// Verify relaxed constexpr function bodies are evaluated at compile time.

constexpr int local_variable(int x) {
  int value = x + 1;
  return value;
}

constexpr int conditional(int x) {
  if (x > 0)
    return x;
  return -x;
}

constexpr int attributed_statement(int x) {
  [branch]
  if (x > 0)
    return x;
  return -x;
}

constexpr int multiple_returns(int x) {
  if (x == 0)
    return 10;
  if (x == 1)
    return 20;
  return 30;
}

constexpr int expression_statement(int x) {
  local_variable(x);
  return x;
}

constexpr int switch_statement(int x) {
  switch (x) {
  case 0:
    break;
  case 1:
    return 20;
  default:
    return 30;
  }
  return 10;
}

constexpr int nested_block(int x) {
  {
    int value = x * 2;
    return value;
  }
}

constexpr int local_type(int x) {
  enum Local {
    Value = 4
  };
  return x + Value;
}

static_assert(local_variable(3) == 4, "local variable");
static_assert(conditional(3) == 3, "true branch");
static_assert(conditional(-3) == 3, "false branch");
static_assert(attributed_statement(3) == 3, "attributed true branch");
static_assert(attributed_statement(-3) == 3, "attributed false branch");
static_assert(multiple_returns(0) == 10, "first return");
static_assert(multiple_returns(1) == 20, "second return");
static_assert(multiple_returns(2) == 30, "final return");
static_assert(expression_statement(3) == 3, "expression statement");
static_assert(switch_statement(0) == 10, "first case");
static_assert(switch_statement(1) == 20, "second case");
static_assert(switch_statement(2) == 30, "default case");
static_assert(nested_block(3) == 6, "nested block");
static_assert(local_type(2) == 6, "local type");

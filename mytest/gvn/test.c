int test0() {
  int a = 10;
  int b = 20;
  int c = a + b;
  int d = a + b;
  int e = a + b;
  return c + d + e;
}

// should not be optimized
int foo(int a, int b, int c) {
  int x;
  if (c > 0) {
    x = a - b;
  } else {
    x = a - b;
  }
  return x;
}

int foo1(int a, int b, int c) {
  int x;
  // mannually host common subexpression
  int x0 = a - b;
  if (c > 0) {
    x = a - b;
  } else {
    x = a - b;
  }
  return x;
}

int bar() {
  
}
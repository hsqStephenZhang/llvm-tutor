int demo(int a, int b, int c, int d) {
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    // next three values should all be moved out of the loop
    int v1 = a + b;
    int v2 = v1 + c;
    int v3 = v2 + d;
    sum += v3;
  }
  return sum;
}

void arr_access() {
  int n = 10;
  int arr[10];
  int arr2[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  for (int i = 0; i < n; i++) {
    int y = arr2[0] + 5;
    arr[i] = y * i;
  }
}

// some case we should NOT change
void cannot_move1(int a, int b) {
  int n = 10;
  int arr[10];
  int flag = 0;
  for (int i = 0; i < n; i++) {
    if (flag) {
      int x = a + b; // loop-invariant?
      arr[i] = x;
    }
  }
}
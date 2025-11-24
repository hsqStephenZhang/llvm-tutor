int sum_array(int *arr, int n) {
  int sum = 0;
  for (int i = 0, j = 0; i < n; i++, j++) {
    sum += arr[i] + arr[j];
  }
  return sum;
}

int sum_while(int *arr, int n) {
  int i = 0;
  int sum = 0;
  while (i < n) {
    sum += arr[i];
    i++;
  }
  return sum;
}

int find_first_positive_and_sum(int *arr, int n) {
  int sum = 0;
  for (int i = 0; i < n; ++i) {
    if (arr[i] > 100)
      break;
    sum += arr[i];
  }
  return sum;
}

// // loop unroll
// int foo(int arr[10]) {
//   int sum = 0;
//   int i = 0;
//   while (1) {
//     sum += arr[i];
//     i++;
//     if (i >= 10)
//       break;
//   }
//   return sum;
// }

int bar(int arr[10]) {
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += arr[i];
  }
  return sum;
}
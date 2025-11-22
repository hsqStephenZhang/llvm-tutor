define i32 @sum_of_squares(i32 %a, i32 %b) #0 {
entry:
  %a.addr = alloca i32, align 4
  %b.addr = alloca i32, align 4
  %square_a = alloca i32, align 4
  %square_b = alloca i32, align 4
  
  store i32 %a, ptr %a.addr, align 4
  store i32 %b, ptr %b.addr, align 4
  
  %0 = load i32, ptr %a.addr, align 4
  %1 = mul nsw i32 %0, %0
  store i32 %1, ptr %square_a, align 4
  
  %2 = load i32, ptr %b.addr, align 4
  %3 = mul nsw i32 %2, %2
  store i32 %3, ptr %square_b, align 4
  
  %4 = load i32, ptr %square_a, align 4
  %5 = load i32, ptr %square_b, align 4
  %sum = add nsw i32 %4, %5
  
  ret i32 %sum
}

define i32 @calculate_with_temp(i32 %x) #0 {
entry:
  %temp = alloca i32, align 4
  %result = alloca i32, align 4
  
  ; 计算 x * 2
  %doubled = mul nsw i32 %x, 2
  store i32 %doubled, ptr %temp, align 4
  
  ; 加载并加 10
  %0 = load i32, ptr %temp, align 4
  %1 = add nsw i32 %0, 10
  store i32 %1, ptr %result, align 4
  
  %2 = load i32, ptr %result, align 4
  ret i32 %2
}

define i32 @main() {
entry:
  %result1 = alloca i32, align 4
  %result2 = alloca i32, align 4
  %final = alloca i32, align 4
  
  ; should inline
  %call1 = call i32 @sum_of_squares(i32 3, i32 4)
  store i32 %call1, ptr %result1, align 4
  
  ; should inline
  %call2 = call i32 @calculate_with_temp(i32 5)
  store i32 %call2, ptr %result2, align 4
  
  %0 = load i32, ptr %result1, align 4
  %1 = load i32, ptr %result2, align 4
  %sum = add nsw i32 %0, %1
  store i32 %sum, ptr %final, align 4
  
  %2 = load i32, ptr %final, align 4
  ret i32 %2
}

attributes #0 = { alwaysinline nounwind }
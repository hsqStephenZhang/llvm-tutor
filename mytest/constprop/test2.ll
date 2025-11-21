; Test complex arithmetic chains
define i32 @test_arithmetic_chain() {
; CHECK-LABEL: @test_arithmetic_chain()
; CHECK-NEXT: ret i32 123
  %1 = add i32 10, 20
  %2 = mul i32 %1, 3
  %3 = sub i32 %2, 15
  %4 = add i32 %3, 48
  %5 = sdiv i32 %4, 1
  ret i32 %5
}

; Test mixed bitwise and arithmetic operations
define i64 @test_mixed_ops() {
; CHECK-LABEL: @test_mixed_ops()
; CHECK-NEXT: ret i64 255
  %1 = shl i64 1, 8
  %2 = sub i64 %1, 1
  %3 = and i64 %2, 511
  %4 = or i64 %3, 0
  %5 = xor i64 %4, 0
  ret i64 %5
}

; Test nested bitwise operations
define i32 @test_bitwise_pyramid() {
; CHECK-LABEL: @test_bitwise_pyramid()
; CHECK-NEXT: ret i32 42
  %1 = or i32 32, 8
  %2 = and i32 %1, 255
  %3 = xor i32 %2, 6
  %4 = shl i32 1, 2
  %5 = or i32 %3, %4
  %6 = and i32 %5, 63
  ret i32 %6
}

; Test with multiple intermediate casts
define i32 @test_cast_chain() {
; CHECK-LABEL: @test_cast_chain()
; CHECK-NEXT: ret i32 200
  %1 = add i8 100, 50
  %2 = sext i8 %1 to i16
  %3 = add i16 %2, 50
  %4 = zext i16 %3 to i32
  %5 = mul i32 %4, 1
  ret i32 %5
}

; Test select chains with constant conditions
define i32 @test_select_chain() {
; CHECK-LABEL: @test_select_chain()
; CHECK-NEXT: ret i32 75
  %1 = icmp sgt i32 100, 50
  %2 = select i1 %1, i32 100, i32 50
  %3 = icmp slt i32 %2, 200
  %4 = select i1 %3, i32 %2, i32 200
  %5 = sub i32 %4, 25
  ret i32 %5
}

; Test complex comparison chains
define i1 @test_comparison_chain() {
; CHECK-LABEL: @test_comparison_chain()
; CHECK-NEXT: ret i1 true
  %1 = icmp eq i32 50, 50
  %2 = icmp ne i32 100, 200
  %3 = and i1 %1, %2
  %4 = icmp sgt i32 75, 25
  %5 = or i1 %3, %4
  %6 = xor i1 %5, false
  ret i1 %6
}

; Test floating point computation chain
define double @test_fp_chain() {
; CHECK-LABEL: @test_fp_chain()
; CHECK-NEXT: ret double 1.000000e+01
  %1 = fadd double 2.5, 2.5
  %2 = fmul double %1, 3.0
  %3 = fsub double %2, 5.0
  %4 = fdiv double %3, 1.0
  %5 = fadd double %4, 0.0
  ret double %5
}

; Test shift and mask patterns
define i32 @test_shift_mask() {
; CHECK-LABEL: @test_shift_mask()
; CHECK-NEXT: ret i32 240
  %1 = shl i32 15, 4
  %2 = and i32 %1, 255
  %3 = lshr i32 %2, 0
  %4 = shl i32 %3, 0
  %5 = or i32 %4, 0
  ret i32 %5
}

; Test multiple levels of arithmetic with different types
define i64 @test_mixed_width_ops() {
; CHECK-LABEL: @test_mixed_width_ops()
; CHECK-NEXT: ret i64 1000
  %1 = add i32 100, 200
  %2 = mul i32 %1, 2
  %3 = sext i32 %2 to i64
  %4 = add i64 %3, 400
  %5 = sub i64 %4, 0
  ret i64 %5
}

; Test constant expression with multiple operations
define i32 @test_expression_tree() {
; CHECK-LABEL: @test_expression_tree()
; CHECK-NEXT: ret i32 144
  %a = add i32 5, 3
  %b = mul i32 4, 6
  %c = sub i32 %b, %a
  %d = add i32 10, 20
  %e = mul i32 %c, 2
  %f = add i32 %e, %d
  %g = sdiv i32 %f, 1
  ret i32 %g
}

; Test with logical operations on comparisons
define i32 @test_logical_select() {
; CHECK-LABEL: @test_logical_select()
; CHECK-NEXT: ret i32 999
  %1 = icmp eq i32 10, 10
  %2 = icmp sgt i32 50, 25
  %3 = and i1 %1, %2
  %4 = select i1 %3, i32 999, i32 0
  %5 = add i32 %4, 0
  ret i32 %5
}

; Test with bitcast and arithmetic
define i32 @test_bitcast_ops() {
; CHECK-LABEL: @test_bitcast_ops()
; CHECK-NEXT: ret i32 65536
  %1 = shl i32 1, 10
  %2 = mul i32 %1, 64
  %3 = bitcast i32 %2 to i32
  %4 = add i32 %3, 0
  ret i32 %4
}

; Test complex constant folding with multiple paths
define i32 @test_diamond_pattern() {
; CHECK-LABEL: @test_diamond_pattern()
; CHECK-NEXT: ret i32 300
  %cond = icmp eq i32 5, 5
  %left = add i32 100, 50
  %right = mul i32 50, 3
  %merge = select i1 %cond, i32 %left, i32 %right
  %final = add i32 %merge, %right
  ret i32 %final
}

; Test shift operations with chained computations
define i32 @test_shift_chain() {
; CHECK-LABEL: @test_shift_chain()
; CHECK-NEXT: ret i32 128
  %1 = shl i32 2, 3
  %2 = shl i32 %1, 2
  %3 = lshr i32 %2, 1
  %4 = shl i32 %3, 1
  %5 = and i32 %4, 255
  ret i32 %5
}

; Test with unsigned and signed operations
define i32 @test_signed_unsigned_mix() {
; CHECK-LABEL: @test_signed_unsigned_mix()
; CHECK-NEXT: ret i32 50
  %1 = add i32 100, 100
  %2 = udiv i32 %1, 2
  %3 = sdiv i32 %2, 2
  %4 = mul i32 %3, 1
  ret i32 %4
}

; Test with truncation and extension chain
define i32 @test_trunc_ext_chain() {
; CHECK-LABEL: @test_trunc_ext_chain()
; CHECK-NEXT: ret i32 42
  %1 = add i64 20, 22
  %2 = trunc i64 %1 to i32
  %3 = sext i32 %2 to i64
  %4 = trunc i64 %3 to i32
  %5 = add i32 %4, 0
  ret i32 %5
}

; Test boolean algebra with constants
define i1 @test_boolean_algebra() {
; CHECK-LABEL: @test_boolean_algebra()
; CHECK-NEXT: ret i1 true
  %a = icmp eq i32 10, 10
  %b = icmp ne i32 20, 30
  %c = and i1 %a, %b
  %d = icmp slt i32 5, 10
  %e = or i1 %c, %d
  %f = xor i1 %e, false
  %g = and i1 %f, true
  ret i1 %g
}

; Test with pointer arithmetic on constants (inttoptr/ptrtoint)
define i64 @test_ptr_arithmetic() {
; CHECK-LABEL: @test_ptr_arithmetic()
; CHECK-NEXT: ret i64 1040
  %1 = add i64 1000, 16
  %2 = add i64 %1, 24
  %3 = sub i64 %2, 0
  ret i64 %3
}

; Test modulo and remainder operations
define i32 @test_remainder_ops() {
; CHECK-LABEL: @test_remainder_ops()
; CHECK-NEXT: ret i32 11
  %1 = srem i32 100, 89
  %2 = add i32 %1, 0
  %3 = mul i32 %2, 1
  ret i32 %3
}

; Test with multiple constant vectors (scalar version)
define i32 @test_multi_operation() {
; CHECK-LABEL: @test_multi_operation()
; CHECK-NEXT: ret i32 2100
  %a = mul i32 10, 20
  %b = add i32 %a, 100
  %c = mul i32 %b, 2
  %d = sub i32 %c, 50
  %e = add i32 %d, 250
  %f = sdiv i32 %e, 1
  ret i32 %f
}ƒ
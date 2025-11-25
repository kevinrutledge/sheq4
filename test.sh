#!/bin/bash

pass=0
fail=0

test_case() {
    name="$1"
    input="$2"
    expected="$3"
    got=$(./sheq4 "$input" 2>/dev/null)
    if [ "$got" = "$expected" ]; then
        printf "%-40s OK\n" "$name"
        ((pass++))
    else
        printf "%-40s FAIL (expected %s, got %s)\n" "$name" "$expected" "$got"
        ((fail++))
    fi
}

test_err() {
    name="$1"
    input="$2"
    if ./sheq4 "$input" 2>&1 | grep -q "SHEQ"; then
        printf "%-40s OK\n" "$name"
        ((pass++))
    else
        printf "%-40s FAIL (expected SHEQ error)\n" "$name"
        ((fail++))
    fi
}

echo "SHEQ4 tests"
echo ""

test_case "number" "2" "2"
test_case "string" '"hello"' '"hello"'
test_case "true" "true" "true"
test_case "false" "false" "false"

test_case "add" "{+ 3 4}" "7"
test_case "sub" "{- 10 3}" "7"
test_case "mul" "{* 3 2}" "6"
test_case "div" "{/ 6 3}" "2"

test_case "lte true" "{<= 1 2}" "true"
test_case "lte false" "{<= 2 1}" "false"

test_case "equal? num" "{equal? 2 2}" "true"
test_case "equal? str" '{equal? "hi" "hi"}' "true"
test_case "equal? bool" "{equal? true false}" "false"

test_case "strlen" '{strlen "hello"}' "5"
test_case "substring" '{substring "hello" 0 2}' '"he"'

test_case "if true" "{if true 1 2}" "1"
test_case "if false" "{if false 1 2}" "2"

test_case "lambda" "{{lambda (x) : {+ x 1}} 5}" "6"
test_case "lambda 2 params" "{{lambda (x y) : {+ x y}} 3 4}" "7"
test_case "nested lambda" "{{lambda (x) : {{lambda (y) : {+ x y}} 3}} 5}" "8"

test_case "let" "{let {[x = 5]} in {+ x 3} end}" "8"
test_case "let multi" "{let {[x = 5] [y = 3]} in {+ x y} end}" "8"

test_case "closure capture" "{{let {[x = 5]} in {lambda (y) : {+ x y}} end} 3}" "8"

test_case "higher-order" "{{lambda (f) : {f 5}} {lambda (x) : {+ x 1}}}" "6"

test_err "div by zero" "{/ 5 0}"
test_err "user error" '{error "fail"}'
test_err "arity mismatch" "{{lambda (x) : x} 1 2}"
test_err "apply non-func" "{1 2}"
test_err "if non-bool" "{if 1 2 3}"
test_err "unbound" "x"

echo ""
echo "done: $pass passed, $fail failed"
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17
// UNSUPPORTED: no-localization

// libc++ does not interpret signs on duration fields or a leading duration sign.

#include <cassert>
#include <chrono>
#include <locale>
#include <sstream>
#include <string>

#include "make_string.h"
#include "test_macros.h"

#define ST(S) MAKE_STRING(CharT, S)

template <class CharT, class Duration>
void check_failure(const std::basic_string<CharT>& input, const std::basic_string<CharT>& format) {
  std::basic_istringstream<CharT> stream(input);
  stream.imbue(std::locale::classic());
  const Duration initial{42};
  Duration result = initial;
  std::chrono::from_stream(stream, format.c_str(), result);
  assert(stream.fail());
  assert(result == initial);
}

template <class CharT, class Duration>
void test_rep() {
  for (const auto& sign : {ST("+"), ST("-")}) {
    // Neither the first field nor a later field accepts a sign.
    for (const auto& format :
         {ST("%H"), ST("%I"), ST("%M"), ST("%S"), ST("%j"), ST("%OH"), ST("%OI"), ST("%OM"), ST("%OS")}) {
      check_failure<CharT, Duration>(sign + ST("01"), format);
      check_failure<CharT, Duration>(ST("01 ") + sign + ST("01"), ST("%H ") + format);
    }
    for (const auto& format : {ST("%R"), ST("%H:%M")}) {
      check_failure<CharT, Duration>(sign + ST("01:30"), format);
      check_failure<CharT, Duration>(ST("01:") + sign + ST("30"), format);
    }
    for (const auto& format : {ST("%T"), ST("%X"), ST("%EX")})
      check_failure<CharT, Duration>(sign + ST("01:30:00"), format);
    check_failure<CharT, Duration>(sign + ST("01:30:00 AM"), ST("%r"));
    check_failure<CharT, Duration>(sign + ST("PM 01:30"), ST("%p %I:%M"));
    check_failure<CharT, Duration>(sign + ST("1.25"), ST("%S"));
    check_failure<CharT, Duration>(sign + ST("0"), ST("%S"));
    check_failure<CharT, Duration>(ST("01:30 ") + sign + ST("01:30"), ST("%R %R"));
    check_failure<CharT, Duration>(sign + ST("01:30 ") + sign + ST("01:30"), ST("%R %R"));
  }
}

template <class CharT>
void test() {
  using namespace std::chrono;
  test_rep<CharT, seconds>();
  test_rep<CharT, milliseconds>();
  test_rep<CharT, duration<unsigned, std::milli>>();
  test_rep<CharT, duration<double, std::milli>>();

  // Explicit format literals are still matched, without changing the value's sign.
  for (const auto& sign : {ST("+"), ST("-")}) {
    std::basic_istringstream<CharT> stream(sign + ST("01:30"));
    minutes result{};
    from_stream(stream, (sign + ST("%R")).c_str(), result);
    assert(!stream.fail());
    assert(result == 90min);
  }

  // A duration can still be accompanied by a signed UTC offset output.
  std::basic_istringstream<CharT> stream(ST("01:30 -04:30"));
  minutes result{};
  minutes offset{};
  from_stream(stream, ST("%R %Ez").c_str(), result, static_cast<std::basic_string<CharT>*>(nullptr), &offset);
  assert(!stream.fail());
  assert(result == 90min);
  assert(offset == -270min);
}

int main(int, char**) {
  test<char>();
#ifndef TEST_HAS_NO_WIDE_CHARACTERS
  test<wchar_t>();
#endif
  return 0;
}

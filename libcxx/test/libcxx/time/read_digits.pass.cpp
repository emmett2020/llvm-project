//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17
// UNSUPPORTED: no-localization

// chrono::__read_digits stops immediately after consuming an overflowing digit.

#include <cassert>
#include <chrono>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>

#include "make_string.h"
#include "test_macros.h"

#define ST(S) MAKE_STRING(CharT, S)

template <class CharT>
void test() {
  struct TestCase {
    std::basic_string<CharT> input;
    unsigned width;
    std::uint64_t limit;
    std::uint64_t value;
    unsigned count;
    bool overflow;
    CharT next;
  };
  const TestCase cases[] = {
      {ST("12345"), 5, 12, 12, 3, true, CharT('4')},
      {ST("12345"), 2, 12, 12, 2, false, CharT('3')},
      {ST("9999"), 4, 12, 9, 2, true, CharT('9')},
      {ST("12"), 2, 0, 0, 1, true, CharT('2')},
      {ST("0012"), 4, 0, 0, 3, true, CharT('2')},
      {ST("12"), 0, 12, 0, 0, false, CharT('1')},
      {ST("X"), 1, 12, 0, 0, false, CharT('X')},
      {ST("18446744073709551615X"),
       21,
       std::numeric_limits<std::uint64_t>::max(),
       std::numeric_limits<std::uint64_t>::max(),
       20,
       false,
       CharT('X')},
      {ST("184467440737095516160X"),
       22,
       std::numeric_limits<std::uint64_t>::max(),
       1844674407370955161ULL,
       20,
       true,
       CharT('0')},
  };
  for (const auto& c : cases) {
    std::basic_istringstream<CharT> stream(c.input);
    auto result = std::chrono::__read_digits(stream, c.width, c.limit);
    assert(result.__value == c.value);
    assert(result.__digits_read == c.count);
    assert(result.__overflow == c.overflow);
    assert(stream.good()); // Reporting failure is the caller's responsibility.
    assert(stream.peek() == c.next);
  }
}

int main(int, char**) {
  test<char>();
#ifndef TEST_HAS_NO_WIDE_CHARACTERS
  test<wchar_t>();
#endif
  return 0;
}

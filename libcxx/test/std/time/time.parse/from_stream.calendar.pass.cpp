//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17
// UNSUPPORTED: no-localization

// Calendar types reject fields they cannot represent, but allow %z and %Z.

#include <cassert>
#include <chrono>
#include <initializer_list>
#include <locale>
#include <sstream>
#include <string>

#include "make_string.h"
#include "test_macros.h"

#define ST(S) MAKE_STRING(CharT, S)

template <class CharT, class Calendar>
void check(const std::basic_string<CharT>& input,
           const std::basic_string<CharT>& format,
           Calendar expected,
           bool success = true) {
  // UTC offsets and time zone abbreviations are allowed even when their output pointers are null.
  for (bool with_zone : {false, true}) {
    const auto text = input + (with_zone ? ST(" UTC +0130") : ST(""));
    const auto fmt  = format + (with_zone ? ST(" %Z %z") : ST(""));
    std::basic_istringstream<CharT> stream(text);
    stream.imbue(std::locale::classic());
    const Calendar initial{};
    Calendar result = initial;
    from_stream(stream, fmt.c_str(), result);
    assert(stream.fail() == !success);
    assert(result == (success ? expected : initial));
  }
}

template <class CharT, class Calendar>
void check_time_fields(
    const std::basic_string<CharT>& input, const std::basic_string<CharT>& format, Calendar expected) {
  check(input, format, expected);
  // Even zero-valued time fields cannot be represented by a calendar type.
  check(input + ST(" 00"), format + ST(" %H"), expected, false);
  check(input + ST(" 12"), format + ST(" %I"), expected, false);
  check(input + ST(" AM"), format + ST(" %p"), expected, false);
  check(input + ST(" 00"), format + ST(" %M"), expected, false);
  check(input + ST(" 00"), format + ST(" %S"), expected, false);
  check(input + ST(" 00:00:00"), format + ST(" %T"), expected, false);
  check(input + ST(" 00:00:00"), format + ST(" %X"), expected, false);
}

template <class CharT>
void test() {
  using namespace std::chrono;

  check_time_fields(ST("15"), ST("%d"), day{15});
  check(ST("02-15"), ST("%m-%d"), day{15}, false);
  check(ST("15 02"), ST("%d %m"), day{15}, false);
  check(ST("2026 15"), ST("%Y %d"), day{15}, false);
  check(ST("Mon 15"), ST("%a %d"), day{15}, false);
  check(ST("15 046"), ST("%d %j"), day{15}, false);
  check(ST(" 15"), ST("%n%e"), day{15});
  check(ST("00"), ST("%d"), day{15}, false);
  check(ST("32"), ST("%d"), day{15}, false);
  check(ST("02"), ST("%m"), day{15}, false);

  check_time_fields(ST("02"), ST("%m"), February);
  check(ST("Feb"), ST("%b"), February);
  check(ST("02-15"), ST("%m-%d"), February, false);
  check(ST("2026-02"), ST("%Y-%m"), February, false);
  check(ST("02 Mon"), ST("%m %a"), February, false);
  check(ST("13"), ST("%m"), February, false);

  check_time_fields(ST("2026"), ST("%Y"), 2026y);
  check(ST("20 26"), ST("%C %y"), 2026y);
  check(ST("26"), ST("%y"), 2026y);
  check(ST("2026 20"), ST("%Y %C"), 2026y);
  check(ST("2026-02"), ST("%Y-%m"), 2026y, false);
  check(ST("2026 15"), ST("%Y %d"), 2026y, false);
  check(ST("2026 Mon"), ST("%Y %a"), 2026y, false);
  check(ST("2026 046"), ST("%Y %j"), 2026y, false);
  check(ST("2026 25"), ST("%Y %y"), 2026y, false);
  check(ST("32768"), ST("%5Y"), 2026y, false);
  check(ST("20"), ST("%C"), 2026y, false);

  check_time_fields(ST("Mon"), ST("%a"), Monday);
  check(ST("1"), ST("%u"), Monday);
  check(ST("Mon 02"), ST("%a %m"), Monday, false);
  check(ST("Mon 15"), ST("%a %d"), Monday, false);
  check(ST("Mon 2026"), ST("%a %Y"), Monday, false);
  check(ST("7"), ST("%w"), Monday, false);

  check_time_fields(ST("02-15"), ST("%m-%d"), February / 15);
  check(ST("02-29"), ST("%m-%d"), February / 29);
  check(ST("2026-02-15"), ST("%F"), February / 15, false);
  check(ST("02-15 Sun"), ST("%m-%d %a"), February / 15, false);
  check(ST("02-30"), ST("%m-%d"), February / 15, false);
  check(ST("02"), ST("%m"), February / 15, false);

  check_time_fields(ST("2026-02"), ST("%Y-%m"), 2026y / February);
  check(ST("20 26 02"), ST("%C %y %m"), 2026y / February);
  check(ST("2026-02-15"), ST("%F"), 2026y / February, false);
  check(ST("2026-02 Sun"), ST("%Y-%m %a"), 2026y / February, false);
  check(ST("2026-02 046"), ST("%Y-%m %j"), 2026y / February, false);
  check(ST("2026-13"), ST("%Y-%m"), 2026y / February, false);

  const year_month_day date = 2026y / February / 15;
  check_time_fields(ST("2026-02-15"), ST("%F"), date);
  check(ST("20 26-02-15"), ST("%C %y-%m-%d"), date);
  check(ST("2026 046"), ST("%Y %j"), date);
  check(ST("2026-W07-7"), ST("%G-W%V-%u"), date);
  check(ST("2026 07 0"), ST("%Y %U %w"), date);
  check(ST("2026 06 7"), ST("%Y %W %u"), date);
  check(ST("2026-02-15 Sun"), ST("%F %a"), date);
  check(ST("2026-02-15 Mon"), ST("%F %a"), date, false);
  check(ST("2026-02-30"), ST("%F"), date, false);
  check(ST("2026-02"), ST("%Y-%m"), date, false);
}

int main(int, char**) {
  test<char>();
#ifndef TEST_HAS_NO_WIDE_CHARACTERS
  test<wchar_t>();
#endif
  return 0;
}

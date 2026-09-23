//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17
// UNSUPPORTED: no-localization

// libc++ checks redundant date fields even when they do not form a complete date.

#include <cassert>
#include <chrono>
#include <format>
#include <locale>
#include <sstream>
#include <string>

#include "make_string.h"
#include "test_macros.h"

#define ST(S) MAKE_STRING(CharT, S)

template <class CharT>
void check(const std::basic_string<CharT>& input,
           const std::basic_string<CharT>& format,
           std::chrono::year_month_day expected,
           bool success = true) {
  using namespace std::chrono;
  const year_month_day initial = 2000y / January / 1;
  {
    std::basic_istringstream<CharT> stream(input);
    stream.imbue(std::locale::classic());
    year_month_day result = initial;
    from_stream(stream, format.c_str(), result);
    assert(stream.fail() == !success);
    assert(result == (success ? expected : initial));
  }
  {
    std::basic_istringstream<CharT> stream(input);
    stream.imbue(std::locale::classic());
    sys_days result{initial};
    from_stream(stream, format.c_str(), result);
    assert(stream.fail() == !success);
    assert(result == sys_days{success ? expected : initial});
  }
}

template <class CharT>
void test() {
  using namespace std::chrono;
  const year_month_day feb1 = 2026y / February / 1;
  check(ST("2026 032 02"), ST("%Y %j %m"), feb1);
  check(ST("2026 032 03"), ST("%Y %j %m"), feb1, false);
  check(ST("2026 032 01"), ST("%Y %j %d"), feb1);
  check(ST("2026 032 02"), ST("%Y %j %d"), feb1, false);
  check(ST("03 2026 032"), ST("%m %Y %j"), feb1, false);

  // A week date can supply the complete date while calendar fields constrain it.
  check(ST("2026 05 0 02"), ST("%Y %U %w %m"), feb1);
  check(ST("2026 05 0 03"), ST("%Y %U %w %m"), feb1, false);
  check(ST("2026 04 7 01"), ST("%Y %W %u %d"), feb1);
  check(ST("2026 04 7 02"), ST("%Y %W %u %d"), feb1, false);

  const year_month_day jan1 = 2021y / January / 1;
  check(ST("2020-W53-5 2021 01 01 001 20"), ST("%G-W%V-%u %Y %m %d %j %C"), jan1);
  check(ST("2020-W53-5 2020"), ST("%G-W%V-%u %Y"), jan1, false);
  check(ST("2020-W53-5 02"), ST("%G-W%V-%u %m"), jan1, false);
  check(ST("2020-W53-5 02"), ST("%G-W%V-%u %d"), jan1, false);
  check(ST("2020-W53-5 002"), ST("%G-W%V-%u %j"), jan1, false);
  check(ST("2020-W53-5 21"), ST("%G-W%V-%u %C"), jan1, false);
  check(ST("2020-W53-5 20"), ST("%G-W%V-%u %C"), jan1);

  // Standalone ISO fields must agree, including across an ISO-year boundary.
  check(ST("2021-01-01 2020"), ST("%F %G"), jan1);
  check(ST("2021-01-01 2021"), ST("%F %G"), jan1, false);
  check(ST("2021-01-01 53"), ST("%F %V"), jan1);
  check(ST("2021-01-01 01"), ST("%F %V"), jan1, false);
  const year_month_day dec31 = 2018y / December / 31;
  check(ST("2018-12-31 2019 01"), ST("%F %G %V"), dec31);
  check(ST("2018-12-31 2018"), ST("%F %G"), dec31, false);
  check(ST("2018-12-31 52"), ST("%F %V"), dec31, false);

  // Week zero, different week starts, and the extra weekday are checked separately.
  for (const auto& format : {ST("%F %U"), ST("%F %W")}) {
    check(ST("2021-01-01 00"), format, jan1);
    check(ST("2021-01-01 01"), format, jan1, false);
  }
  check(ST("2018-12-31 52 53"), ST("%F %U %W"), dec31);
  check(ST("2018-12-31 53"), ST("%F %U"), dec31, false);
  check(ST("2018-12-31 52"), ST("%F %W"), dec31, false);
  check(ST("2021-01-01 Fri"), ST("%F %a"), jan1);
  check(ST("2021-01-01 Thu"), ST("%F %a"), jan1, false);

  // Leap days and a negative calendar year's century.
  check(ST("2024 060 02 29"), ST("%Y %j %m %d"), 2024y / February / 29);
  check(ST("2024 060 03"), ST("%Y %j %m"), 2024y / February / 29, false);
  check(ST("-0001 001 -1"), ST("%5Y %j %2C"), year{-1} / January / 1);
  check(ST("-0001 001 00"), ST("%5Y %j %C"), year{-1} / January / 1, false);

  // ISO-year calculations must also work at the limits of chrono::year.
  check(ST("-32767-01-01 53"), ST("%6F %V"), year::min() / January / 1);
  check(ST("-32767-01-01 52"), ST("%6F %V"), year::min() / January / 1, false);
  check(ST("32767-01-01 32766 52"), ST("%5F %5G %V"), year::max() / January / 1);
  check(ST("32767-12-31 32767 52"), ST("%5F %5G %V"), year::max() / December / 31);

  // Ordinary dates do not need redundant fields.
  check(ST("2026-02-01"), ST("%F"), feb1);
  check(ST("2026 032"), ST("%Y %j"), feb1);
  check(ST("2020-W53-5"), ST("%G-W%V-%u"), jan1);
}

int main(int, char**) {
  test<char>();
#ifndef TEST_HAS_NO_WIDE_CHARACTERS
  test<wchar_t>();
#endif
  // Cover all Gregorian year-boundary patterns in a complete 400-year cycle.
  // Formatting provides the redundant fields independently of the parser.
  for (int y = 2000; y != 2400; ++y) {
    using namespace std::chrono;
    const sys_days jan1{year{y} / January / 1};
    for (int offset = -7; offset != 7; ++offset) {
      const sys_days date = jan1 + days{offset};
      check<char>(std::format("{:%F %j %U %W %G %V %u}", date), "%F %j %U %W %G %V %u", year_month_day{date});
    }
  }
  return 0;
}

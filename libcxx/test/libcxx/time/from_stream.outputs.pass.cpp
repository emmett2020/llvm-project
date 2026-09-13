//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17
// UNSUPPORTED: no-localization

// libc++ does not update abbrev or offset until parsing and result construction succeed.

#include <cassert>
#include <chrono>
#include <locale>
#include <sstream>
#include <string>

#include "make_string.h"
#include "test_macros.h"

#define ST(S) MAKE_STRING(CharT, S)

template <class CharT>
void test() {
  using namespace std::chrono;

  auto check = [](const std::basic_string<CharT>& input,
                  const std::basic_string<CharT>& format,
                  bool success,
                  const std::basic_string<CharT>& expected_abbrev,
                  minutes expected_offset) {
    std::basic_istringstream<CharT> stream(input);
    stream.imbue(std::locale::classic());
    const year_month_day initial = year{2000} / January / 1;
    year_month_day result        = initial;
    auto abbrev                  = ST("original");
    minutes offset{42};
    from_stream(stream, format.c_str(), result, &abbrev, &offset);
    assert(stream.fail() == !success);
    assert(result == (success ? year{2026} / July / 20 : initial));
    assert(abbrev == expected_abbrev);
    assert(offset == expected_offset);
  };

  // A failed %Z, a later field/literal failure, and final validation failures.
  check(ST("2026-07-20 +0130 @"), ST("%F %z %Z"), false, ST("original"), 42min);
  check(ST("UTC +0130 invalid"), ST("%Z %z %Y"), false, ST("original"), 42min);
  check(ST("2026-07-20 UTC +0130 ?"), ST("%F %Z %z !"), false, ST("original"), 42min);
  check(ST("2026-02-30 UTC +0130"), ST("%F %Z %z"), false, ST("original"), 42min);
  check(ST("2026-07-20 25 UTC +0130"), ST("%F %y %Z %z"), false, ST("original"), 42min);
  check(ST("UTC +0130"), ST("%Z %z"), false, ST("original"), 42min);

  // Successful parsing, including EOF immediately after %Z or %z.
  check(ST("2026-07-20 UTC +0130"), ST("%F %Z %z"), true, ST("UTC"), 90min);
  check(ST("2026-07-20 -01:30 UTC"), ST("%F %Ez %Z"), true, ST("UTC"), -90min);
  check(ST("2026-07-20 +00:00 UTC"), ST("%F %Oz %Z"), true, ST("UTC"), 0min);

  // An absent directive leaves its output unchanged.
  check(ST("2026-07-20"), ST("%F"), true, ST("original"), 42min);
  check(ST("2026-07-20 UTC"), ST("%F %Z"), true, ST("UTC"), 42min);
  check(ST("2026-07-20 +0130"), ST("%F %z"), true, ST("original"), 90min);
}

int main(int, char**) {
  test<char>();
#ifndef TEST_HAS_NO_WIDE_CHARACTERS
  test<wchar_t>();
#endif
  return 0;
}

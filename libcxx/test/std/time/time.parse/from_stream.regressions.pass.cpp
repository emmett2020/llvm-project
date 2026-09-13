//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17
// UNSUPPORTED: no-localization

// <chrono>
// from_stream: signed widths, duration validation, overflow, and UTC offsets.

#include <cassert>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <limits>
#include <locale>
#include <ratio>
#include <sstream>
#include <string>

#include "make_string.h"
#include "test_macros.h"

#define ST(S) MAKE_STRING(CharT, S)

template <class CharT, class T>
void check(const std::basic_string<CharT>& input, const std::basic_string<CharT>& format, T expected) {
  std::basic_istringstream<CharT> stream(input);
  stream.imbue(std::locale::classic());
  T result{};
  std::chrono::from_stream(stream, format.c_str(), result);
  assert(!stream.fail());
  assert(result == expected);
}

template <class CharT, class T>
void check_failure(const std::basic_string<CharT>& input, const std::basic_string<CharT>& format, T initial) {
  std::basic_istringstream<CharT> stream(input);
  stream.imbue(std::locale::classic());
  T result = initial;
  std::chrono::from_stream(stream, format.c_str(), result);
  assert(stream.fail());
  assert(result == initial);
}

template <class CharT>
class extreme_time_get : public std::time_get<CharT> {
  int year_;
  int month_;

public:
  using iter_type = typename std::time_get<CharT>::iter_type;
  extreme_time_get(int year, int month) : year_(year), month_(month) {}

protected:
  iter_type
  do_get(iter_type first, iter_type last, std::ios_base&, std::ios_base::iostate& err, std::tm* value, char, char)
      const override {
    assert(first != last);
    ++first;
    value->tm_year = year_;
    value->tm_mon  = month_;
    value->tm_mday = 20;
    err            = first == last ? std::ios_base::eofbit : std::ios_base::goodbit;
    return first;
  }
};

template <class CharT>
void test() {
  using namespace std::chrono;

  // Calendar fields must not be accepted as parts of a duration, with or without a minus sign.
  for (const auto& format :
       {ST("%d %H"),
        ST("%e %H"),
        ST("%m %H"),
        ST("%w %H"),
        ST("%U %H"),
        ST("%V %H"),
        ST("%W %H"),
        ST("%y %H"),
        ST("%Y %H"),
        ST("%a %H"),
        ST("%b %H"),
        ST("%c %H"),
        ST("%x %H")}) {
    for (const auto& input : {ST("1 01"), ST("-1 01")})
      check_failure(input, format, 42s);
  }
  check_failure(ST("01 1"), ST("%H %w"), 42s);
  check_failure(ST("2026-07-20 01"), ST("%F %H"), 42s);

  // Only time-of-day/day-count fields consume a duration's leading minus sign.
  for (const auto& format : {ST("%T"), ST("%X"), ST("%EX")})
    check(ST("-01:30:00"), format, -90min);
  check(ST("-01:30:00 AM"), ST("%r"), -90min);
  check(ST("-PM 01:30"), ST("%p %I:%M"), -810min);
  check(ST("-2 01:30"), ST("%j %R"), -(48h + 90min));
  check_failure(ST("01:-30"), ST("%H:%M"), 42min);

  // A sign consumes one character of a signed field's width.
  for (const auto& format : {ST("%3Y"), ST("%3G")}) {
    std::basic_istringstream<CharT> stream(ST("+123"));
    // Supply the remainder of an ISO date when testing %G.
    if (format == ST("%3G")) {
      stream.str(ST("+123-W01-1"));
      sys_days result{};
      from_stream(stream, (format + ST("3-W%V-%u")).c_str(), result);
      assert(!stream.fail());
      assert(year_month_day{result}.year() == year{12});
    } else {
      year result{};
      from_stream(stream, format.c_str(), result);
      assert(!stream.fail());
      assert(result == year{12});
      assert(stream.peek() == CharT('3'));
    }
  }
  check(ST("+123"), ST("%4Y"), year{123});
  check(ST("-123"), ST("%4Y"), year{-123});
  check_failure(ST("+1"), ST("%1Y"), year{42});
  check_failure(ST("-1"), ST("%1Y"), year{42});
  check(ST("-20 76"), ST("%3C %y"), year{-1976});
  check(ST("-20 00"), ST("%3C %y"), year{-2000});
  check(ST("-1 01"), ST("%2C %y"), year{-1});
  check(ST("-1 99"), ST("%2C %y"), year{-99});
  check(ST("-1 00"), ST("%2C %y"), year{-100});
  check(ST("-1976 -20"), ST("%5Y %3C"), year{-1976});
  check_failure(ST("-1976 -19"), ST("%5Y %3C"), year{42});
  check(ST("-0123-07-20"), ST("%5F"), year_month_day{year{-123}, July, day{20}});

  // Check both scaling and accumulation, as well as the asymmetric signed limits.
  check_failure(ST("2147483647"), ST("%10H"), nanoseconds{42});
  check_failure(ST("2147483647"), ST("%10j"), nanoseconds{42});
  check(ST("2562047:47:16.854775807"), ST("%7H:%M:%S"), nanoseconds::max());
  check(ST("-2562047:47:16.854775808"), ST("%7H:%M:%S"), nanoseconds::min());
  check_failure(ST("2562047:47:16.854775808"), ST("%7H:%M:%S"), nanoseconds{42});
  check_failure(ST("-2562047:47:16.854775809"), ST("%7H:%M:%S"), nanoseconds{42});
  using UnsignedNanos = duration<std::uint64_t, std::nano>;
  check(ST("213503 23:34:33.709551615"), ST("%6j %T"), UnsignedNanos::max());
  check_failure(ST("213503 23:34:33.709551616"), ST("%6j %T"), UnsignedNanos{42});
  using Tiny = duration<signed char>;
  check(ST("127"), ST("%3S"), Tiny::max());
  check(ST("-128"), ST("%3S"), Tiny::min());
  check_failure(ST("128"), ST("%3S"), Tiny{42});
  check_failure(ST("-129"), ST("%3S"), Tiny{42});
  check_failure(ST("-1"), ST("%S"), duration<unsigned>{42});
  check(ST("-0"), ST("%S"), duration<unsigned>{0});
  check(ST("-1.25"), ST("%S"), duration<double, std::milli>{-1250});
  check(ST("2.50"), ST("%S"), duration<int, std::ratio<3, 2>>{1});
  check(ST("1 12"), ST("%j %H"), duration<int, std::ratio<129600>>{1});
  // This intermediate product needs more than 64 bits, but the result fits.
  using NearSecond = duration<std::uint64_t, std::ratio<8000000001LL, 8000000000LL>>;
  check(ST("925925:55:00"), ST("%6H:%M:%S"), NearSecond{3333333299ULL});

  // Offset signs are optional for all three spellings.
  const sys_seconds date = sys_days{2026y / July / 20};
  check(ST("2026-07-20 04"), ST("%F %z"), date - 4h);
  check(ST("2026-07-20 0430"), ST("%F %z"), date - 4h - 30min);
  // Offset minutes are two digits, not a clock-minute field restricted to 0-59.
  check(ST("2026-07-20 +0160"), ST("%F %z"), date - 120min);
  check(ST("2026-07-20 -0199"), ST("%F %z"), date + 159min);
  check_failure(ST("2026-07-20 019"), ST("%F %z"), sys_seconds{42s});
  for (const auto& format : {ST("%F %Ez"), ST("%F %Oz")}) {
    check(ST("2026-07-20 4"), format, date - 4h);
    check(ST("2026-07-20 4:30"), format, date - 4h - 30min);
    check(ST("2026-07-20 +4:30"), format, date - 4h - 30min);
    check(ST("2026-07-20 -4:30"), format, date + 4h + 30min);
    check(ST("2026-07-20 1:60"), format, date - 120min);
    check(ST("2026-07-20 +01:90"), format, date - 150min);
    check(ST("2026-07-20 -1:99"), format, date + 159min);
    check_failure(ST("2026-07-20 4:"), format, sys_seconds{42s});
    check_failure(ST("2026-07-20 4:9"), format, sys_seconds{42s});
  }
  check_failure(ST("2026-07-20 4"), ST("%F %z"), sys_seconds{42s});
  check_failure(ST("2026-07-20 +"), ST("%F %z"), sys_seconds{42s});

  // Facet results must be checked before adding the year/month biases.
  for (const auto& format : {ST("%c"), ST("%Ec"), ST("%x"), ST("%Ex")}) {
    for (bool extreme_year : {false, true}) {
      std::basic_istringstream<CharT> stream(ST("@"));
      stream.imbue(std::locale(std::locale::classic(),
                               new extreme_time_get<CharT>(extreme_year ? std::numeric_limits<int>::max() : 126,
                                                           extreme_year ? 6 : std::numeric_limits<int>::max())));
      const sys_seconds initial{42s};
      sys_seconds result = initial;
      from_stream(stream, format.c_str(), result);
      assert(stream.fail());
      assert(result == initial);
    }
  }
  for (const auto& format : {ST("%b"), ST("%Om")}) {
    std::basic_istringstream<CharT> stream(ST("@"));
    stream.imbue(
        std::locale(std::locale::classic(), new extreme_time_get<CharT>(126, std::numeric_limits<int>::max())));
    month result{July};
    from_stream(stream, format.c_str(), result);
    assert(stream.fail());
    assert(result == July);
  }
}

int main(int, char**) {
  test<char>();
#ifndef TEST_HAS_NO_WIDE_CHARACTERS
  test<wchar_t>();
#endif
  return 0;
}

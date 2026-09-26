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
// from_stream overloads for calendar types, durations, and time points.

#include <cassert>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <locale>
#include <ratio>
#include <sstream>
#include <string>
#include <type_traits>

#include "make_string.h"
#include "test_macros.h"

#define ST(S) MAKE_STRING(CharT, S)

// Arithmetic stays in the wrapper; there is no conversion to a built-in number.
template <class T>
struct rep {
  T value{};

  constexpr rep() = default;
  template <class U>
    requires std::is_arithmetic_v<U>
  constexpr rep(U v) : value(static_cast<T>(v)) {}
  template <class U>
  constexpr rep(rep<U> v) : value(static_cast<T>(v.value)) {}

  constexpr rep operator-() const { return rep{-value}; }
  constexpr rep& operator+=(rep other) {
    value += other.value;
    return *this;
  }
  friend constexpr rep operator+(rep lhs, rep rhs) { return rep{lhs.value + rhs.value}; }
  friend constexpr rep operator-(rep lhs, rep rhs) { return rep{lhs.value - rhs.value}; }
  friend constexpr rep operator*(rep lhs, rep rhs) { return rep{lhs.value * rhs.value}; }
  friend constexpr rep operator/(rep lhs, rep rhs) { return rep{lhs.value / rhs.value}; }
  friend constexpr bool operator==(rep lhs, rep rhs) { return lhs.value == rhs.value; }
  friend constexpr bool operator<(rep lhs, rep rhs) { return lhs.value < rhs.value; }
};

template <class T, class U>
struct std::common_type<rep<T>, rep<U>> {
  using type = rep<std::common_type_t<T, U>>;
};
template <class T, class U>
  requires std::is_arithmetic_v<U>
struct std::common_type<rep<T>, U> {
  using type = rep<std::common_type_t<T, U>>;
};
template <class T, class U>
  requires std::is_arithmetic_v<T>
struct std::common_type<T, rep<U>> {
  using type = rep<std::common_type_t<T, U>>;
};

template <class T>
struct std::chrono::treat_as_floating_point<rep<T>> : std::is_floating_point<T> {};

template <class T>
struct std::numeric_limits<rep<T>> : std::numeric_limits<T> {
  static constexpr rep<T> min() noexcept { return (std::numeric_limits<T>::min)(); }
  static constexpr rep<T> max() noexcept { return (std::numeric_limits<T>::max)(); }
  static constexpr rep<T> lowest() noexcept { return std::numeric_limits<T>::lowest(); }
};

static_assert(std::numeric_limits<rep<long long>>::is_integer);
static_assert(!std::is_integral_v<rep<long long>>);
static_assert(std::chrono::treat_as_floating_point_v<rep<double>>);
static_assert(!std::is_floating_point_v<rep<double>>);
static_assert(!std::is_convertible_v<rep<double>, long double>);

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

namespace calendar_tests {

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
}

template <class CharT>
void test() {
  using namespace std::chrono;

  check_time_fields(ST("15"), ST("%d"), day{15});
  check(ST("02-15"), ST("%m-%d"), day{15}, false);
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

} // namespace calendar_tests

namespace numeric_tests {

template <class CharT, class Duration>
static std::chrono::sys_time<Duration>
parse(const std::basic_string<CharT>& input, const std::basic_string<CharT>& fmt, bool expected_fail = false) {
  std::basic_istringstream<CharT> stream{input};
  stream.imbue(std::locale::classic());
  std::chrono::sys_time<Duration> tp{};
  std::chrono::from_stream(stream, fmt.c_str(), tp);
  assert(stream.fail() == expected_fail);
  return tp;
}

template <class CharT>
static void test() {
  using namespace std::chrono;
  using Seconds = std::chrono::seconds;

  const sys_days date         = sys_days{2026y / July / 20};
  const sys_seconds date_time = date + 13h + 45min + 30s;

  // Numeric fields, compound directives, and widths.

  // Individual numeric specifiers.
  assert((parse<CharT, Seconds>(ST("2026-07-20 13:45:30"), ST("%Y-%m-%d %H:%M:%S")) == date_time));

  // %e is equivalent to %d when parsing; leading zeroes are optional.
  assert((parse<CharT, Seconds>(ST("2026-07-7"), ST("%Y-%m-%e")) == sys_days{2026y / July / 7}));

  // Compound specifiers expand to the numeric ones.
  assert((parse<CharT, Seconds>(ST("2026-07-20 13:45:30"), ST("%F %T")) == date_time));
  assert((parse<CharT, Seconds>(ST("2026-07-20 13:45"), ST("%F %R")) == date + 13h + 45min));

  // A width on %F applies only to %Y; %m and %d retain their default widths.
  assert((parse<CharT, Seconds>(ST("002026-07-20"), ST("%6F")) == date));

  // No unsigned underflow when the width leaves no room for fractional digits.
  for (const auto& format : {ST("%1S"), ST("%2S")}) {
    std::basic_istringstream<CharT> stream{ST("1.25")};
    milliseconds result{};
    from_stream(stream, format.c_str(), result);
    assert(!stream.fail());
    assert(result == 1s);
    assert(stream.peek() == CharT('.'));
  }

  // ISO week dates combine the separately parsed year, week, and weekday fields.
  assert((parse<CharT, Seconds>(ST("2026-W30-1"), ST("%G-W%V-%u")) == date));
  assert((parse<CharT, Seconds>(ST("26-W30-1"), ST("%g-W%V-%u")) == date));

  // Missing time-of-day defaults to midnight.
  assert((parse<CharT, Seconds>(ST("2026-07-20"), ST("%F")) == date));

  // A literal '%' and explicit whitespace.
  assert((parse<CharT, Seconds>(ST("2026-07-20%"), ST("%F%%")) == date));
  assert((parse<CharT, Seconds>(ST("   2026-07-20"), ST(" %F")) == date));

  // Whitespace in the format matches zero or more whitespace in the input.
  assert((parse<CharT, Seconds>(ST("2026-07-20"), ST(" %Y-%m-%d")) == date));

  // Parse mismatches set failbit with the default exception mask.

  parse<CharT, Seconds>(ST("2026-02-30"), ST("%Y-%m-%d"), /*expected_fail=*/true); // invalid date
  parse<CharT, Seconds>(ST("2026/07/20"), ST("%Y-%m-%d"), /*expected_fail=*/true); // literal mismatch
  parse<CharT, Seconds>(ST("13:45:30"), ST("%H:%M:%S"), /*expected_fail=*/true);   // no date component
  parse<CharT, Seconds>(
      ST("2026-07-xx"), ST("%Y-%m-%d"), /*expected_fail=*/true); // non-digit where a digit is required
  parse<CharT, Seconds>(ST("2026-07- 7"), ST("%Y-%m-%e"), /*expected_fail=*/true); // %e does not skip whitespace
  parse<CharT, Seconds>(ST(""), ST("%Y"), /*expected_fail=*/true);                 // empty input
  parse<CharT, Seconds>(ST("2026/07/20"), ST("%6D"), /*expected_fail=*/true);      // %D does not allow a width
}

} // namespace numeric_tests

template <class CharT>
void test_time_point_resolution() {
  using namespace std::chrono;
  const sys_seconds expected = sys_days{2026y / July / 20} + 13h + 45min + 30s;
  auto test                  = [&](auto value) {
    check(ST("20 26-07-20 01:45:30 PM"), ST("%C %y-%m-%d %I:%M:%S %p"), value);
    check(ST("20 26 201 13 01:45:30 PM"), ST("%C %y %j %H %I:%M:%S %p"), value);
    check(ST("2026-07-20 13 01:45:30"), ST("%F %H %I:%M:%S"), value);
    check(ST("2026-07-20 01:45:30 13"), ST("%F %I:%M:%S %H"), value);
    check_failure(ST("2026-07-20 14 01:45:30"), ST("%F %H %I:%M:%S"), value);
    check_failure(ST("2026 25-07-20 13:45:30"), ST("%Y %y-%m-%d %T"), value);
    check_failure(ST("2026-07-20 12 01:45:30 PM"), ST("%F %H %I:%M:%S %p"), value);
    check_failure(ST("2026-07-20 13:45:30 AM"), ST("%F %T %p"), value);
    check_failure(ST("2026-07-20 24:00:00"), ST("%F %T"), value);
  };
  test(expected);
  test(local_seconds{expected.time_since_epoch()});
  test(file_clock::from_sys(expected));
}

template <class CharT>
void test_years() {
  using namespace std::chrono;
  // Calendar year boundaries are accepted; values outside the target are
  // rejected without modifying the result.
  {
    std::basic_istringstream<CharT> stream{ST("+32767")};
    year value{0};
    from_stream(stream, ST("%6Y").c_str(), value);
    assert(!stream.fail());
    assert(value == year::max());
  }
  {
    std::basic_istringstream<CharT> stream{ST("-32767")};
    year value{0};
    from_stream(stream, ST("%6Y").c_str(), value);
    assert(!stream.fail());
    assert(value == year::min());
  }
  {
    std::basic_istringstream<CharT> stream{ST("+32768X")};
    year value{2026};
    from_stream(stream, ST("%6Y").c_str(), value);
    assert(stream.fail());
    assert(value == year{2026});

    stream.clear();
    assert(stream.peek() == CharT('X'));
  }
  {
    std::basic_istringstream<CharT> stream{ST("-32769")};
    year value{2026};
    from_stream(stream, ST("%6Y").c_str(), value);
    assert(stream.fail());
    assert(value == year{2026});
  }
  {
    // The parser stores calendar fields in int, but year narrows internally.
    // Check the int value before construction so it cannot wrap to a valid year.
    std::basic_istringstream<CharT> stream{ST("+65537")};
    year value{2026};
    from_stream(stream, ST("%6Y").c_str(), value);
    assert(stream.fail());
    assert(value == year{2026});
  }
  {
    // Calendar components also narrow internally; reject the original value
    // before 257 can wrap to January.
    std::basic_istringstream<CharT> stream{ST("257")};
    month value{July};
    from_stream(stream, ST("%3m").c_str(), value);
    assert(stream.fail());
    assert(value == July);
  }
  {
    // Combining an individually valid int century with %y is checked too.
    std::basic_istringstream<CharT> stream{ST("+214748364799")};
    year value{2026};
    from_stream(stream, ST("%11C%2y").c_str(), value);
    assert(stream.fail());
    assert(value == year{2026});
  }

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
  check(ST("0"), ST("%Y"), year{0});
  check(ST("68"), ST("%y"), year{2068});
  check(ST("69"), ST("%y"), year{1969});
  check_failure(ST("100"), ST("%3y"), year{42});
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
}

template <class CharT>
void test_duration() {
  using namespace std::chrono;
  // Supply valid calendar fields so rejection tests the target's capabilities.
  check_failure(ST("15 01"), ST("%d %H"), 42s);
  check_failure(ST("07 01"), ST("%m %H"), 42s);
  check_failure(ST("2026 01"), ST("%Y %H"), 42s);
  check_failure(ST("Mon 01"), ST("%a %H"), 42s);
  check_failure(ST("Jul 01"), ST("%b %H"), 42s);
  check_failure(ST("2026-07-20 01"), ST("%F %H"), 42s);
  check_failure(ST("2026 30 1 01"), ST("%G %V %u %H"), 42s);
  check_failure(ST("2026 29 1 01"), ST("%Y %U %w %H"), 42s);

  // Parse time-of-day and day-count fields into a duration.
  for (const auto& format : {ST("%T"), ST("%X"), ST("%EX")})
    check(ST("01:30:00"), format, 90min);
  check(ST("01:30:00 AM"), ST("%r"), 90min);
  check(ST("PM 01:30"), ST("%p %I:%M"), 810min);
  check(ST("2 01:30"), ST("%j %R"), 48h + 90min);
  check(ST("1 01"), ST("%j %H"), 25h);
  check(ST("23:59:59"), ST("%T"), 23h + 59min + 59s);
  check_failure(ST("24"), ST("%H"), 42h);
  check_failure(ST("60"), ST("%M"), 42min);
  check_failure(ST("60"), ST("%S"), 42s);
  check_failure(ST("60.0"), ST("%S"), milliseconds{42});
  check_failure(ST("60.0"), ST("%S"), duration<double, std::milli>{42});
  check(ST("13 01 PM"), ST("%H %I %p"), 13h);
  check(ST("13 01"), ST("%H %I"), 13h);
  check(ST("01 13"), ST("%I %H"), 13h);
  check(ST("00 12"), ST("%H %I"), 0h);
  check(ST("12 12"), ST("%H %I"), 12h);
  check_failure(ST("14 01"), ST("%H %I"), 42min);
  check_failure(ST("25 01"), ST("%H %I"), 42min);
  check_failure(ST("12 01 PM"), ST("%H %I %p"), 42min);
  check_failure(ST("13 AM"), ST("%H %p"), 42min);
  check_failure(ST("PM"), ST("%p"), 42min);
  check_failure(ST("00 PM"), ST("%I %p"), 42min);
  check_failure(ST("13 PM"), ST("%I %p"), 42min);

  // Check duration conversion and representable boundary values.
  check_failure(ST("2147483647"), ST("%10H"), nanoseconds{42});
  check(ST("106751 23:47:16.854775807"), ST("%6j %T"), nanoseconds::max());
  using UnsignedNanos = duration<std::uint64_t, std::nano>;
  check(ST("213503 23:34:33.709551615"), ST("%6j %T"), UnsignedNanos::max());
  using Tiny = duration<signed char>;
  check(ST("02:07"), ST("%M:%S"), Tiny::max());
  check(ST("0"), ST("%S"), duration<unsigned>{0});
  check(ST("1.25"), ST("%S"), duration<double, std::milli>{1250});
  check(ST("2.50"), ST("%S"), duration<int, std::ratio<3, 2>>{1});
  check(ST("1 12"), ST("%j %H"), duration<int, std::ratio<129600>>{1});
  // Exercise a custom period without overflowing the intermediate representation.
  using NearSecond = duration<std::uint64_t, std::ratio<8000000001LL, 8000000000LL>>;
  check(ST("00:01:00"), ST("%T"), NearSecond{59});
}

template <class CharT>
void test_offsets() {
  using namespace std::chrono;
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
}

namespace custom_rep_tests {

template <class CharT>
void test() {
  using namespace std::chrono;
  using Int   = rep<long long>;
  using Float = rep<double>;

  check(ST("2 01:02:03"), ST("%j %T"), duration<Int>{176523});
  check(ST("01:30"), ST("%R"), duration<Int, std::ratio<60>>{90});
  check(ST("1.250"), ST("%S"), duration<Int, std::milli>{1250});

  // Combine before truncating: 1 second and 0.5 seconds together make one tick.
  check(ST("1.5"), ST("%S"), duration<Int, std::ratio<3, 2>>{1});
  check(ST("1.4"), ST("%S"), duration<Int, std::ratio<3, 2>>{0});
  check(ST("1 12"), ST("%j %H"), duration<Int, std::ratio<129600>>{1});

  check(ST("1.250"), ST("%S"), duration<Float, std::milli>{1250.0});
  check(ST("1.5"), ST("%S"), duration<Float, std::ratio<3, 2>>{1.0});
  check(ST("0.3"), ST("%S"), duration<Float, std::ratio<3, 2>>{0.2});
  check(ST("01:30:00"), ST("%T"), duration<Float, std::ratio<3600>>{1.5});

  // Parse failures must still preserve the target.
  std::basic_istringstream<CharT> stream(ST("1.250 ?"));
  duration<Int, std::milli> result{42};
  from_stream(stream, ST("%S !").c_str(), result);
  assert(stream.fail());
  assert(result.count().value == 42);
}

} // namespace custom_rep_tests

template <class CharT>
void test() {
  calendar_tests::test<CharT>();
  numeric_tests::test<CharT>();
  test_years<CharT>();
  test_duration<CharT>();
  custom_rep_tests::test<CharT>();
  test_time_point_resolution<CharT>();
  test_offsets<CharT>();
}

int main(int, char**) {
  test<char>();
#ifndef TEST_HAS_NO_WIDE_CHARACTERS
  test<wchar_t>();
#endif
  return 0;
}

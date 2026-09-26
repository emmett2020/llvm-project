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

#define STR(S) MAKE_STRING(CharT, S)

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
    const auto text = input + (with_zone ? STR(" UTC +0130") : STR(""));
    const auto fmt  = format + (with_zone ? STR(" %Z %z") : STR(""));
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
  check(input + STR(" 00"), format + STR(" %H"), expected, false);
  check(input + STR(" 12"), format + STR(" %I"), expected, false);
  check(input + STR(" AM"), format + STR(" %p"), expected, false);
  check(input + STR(" 00"), format + STR(" %M"), expected, false);
  check(input + STR(" 00"), format + STR(" %S"), expected, false);
}

template <class CharT>
void test() {
  using namespace std::chrono;

  check_time_fields(STR("15"), STR("%d"), day{15});
  check(STR("02-15"), STR("%m-%d"), day{15}, false);
  check(STR("2026 15"), STR("%Y %d"), day{15}, false);
  check(STR("Mon 15"), STR("%a %d"), day{15}, false);
  check(STR("15 046"), STR("%d %j"), day{15}, false);
  check(STR(" 15"), STR("%n%e"), day{15});
  check(STR("00"), STR("%d"), day{15}, false);
  check(STR("32"), STR("%d"), day{15}, false);
  check(STR("02"), STR("%m"), day{15}, false);

  check_time_fields(STR("02"), STR("%m"), February);
  check(STR("Feb"), STR("%b"), February);
  check(STR("02-15"), STR("%m-%d"), February, false);
  check(STR("2026-02"), STR("%Y-%m"), February, false);
  check(STR("02 Mon"), STR("%m %a"), February, false);
  check(STR("13"), STR("%m"), February, false);

  check_time_fields(STR("2026"), STR("%Y"), 2026y);
  check(STR("20 26"), STR("%C %y"), 2026y);
  check(STR("26"), STR("%y"), 2026y);
  check(STR("2026 20"), STR("%Y %C"), 2026y);
  check(STR("2026-02"), STR("%Y-%m"), 2026y, false);
  check(STR("2026 15"), STR("%Y %d"), 2026y, false);
  check(STR("2026 Mon"), STR("%Y %a"), 2026y, false);
  check(STR("2026 046"), STR("%Y %j"), 2026y, false);
  check(STR("2026 25"), STR("%Y %y"), 2026y, false);
  check(STR("32768"), STR("%5Y"), 2026y, false);
  check(STR("20"), STR("%C"), 2026y, false);

  check_time_fields(STR("Mon"), STR("%a"), Monday);
  check(STR("1"), STR("%u"), Monday);
  check(STR("Mon 02"), STR("%a %m"), Monday, false);
  check(STR("Mon 15"), STR("%a %d"), Monday, false);
  check(STR("Mon 2026"), STR("%a %Y"), Monday, false);
  check(STR("7"), STR("%w"), Monday, false);

  check_time_fields(STR("02-15"), STR("%m-%d"), February / 15);
  check(STR("02-29"), STR("%m-%d"), February / 29);
  check(STR("2026-02-15"), STR("%F"), February / 15, false);
  check(STR("02-15 Sun"), STR("%m-%d %a"), February / 15, false);
  check(STR("02-30"), STR("%m-%d"), February / 15, false);
  check(STR("02"), STR("%m"), February / 15, false);

  check_time_fields(STR("2026-02"), STR("%Y-%m"), 2026y / February);
  check(STR("20 26 02"), STR("%C %y %m"), 2026y / February);
  check(STR("2026-02-15"), STR("%F"), 2026y / February, false);
  check(STR("2026-02 Sun"), STR("%Y-%m %a"), 2026y / February, false);
  check(STR("2026-02 046"), STR("%Y-%m %j"), 2026y / February, false);
  check(STR("2026-13"), STR("%Y-%m"), 2026y / February, false);

  const year_month_day date = 2026y / February / 15;
  check_time_fields(STR("2026-02-15"), STR("%F"), date);
  check(STR("20 26-02-15"), STR("%C %y-%m-%d"), date);
  check(STR("2026 046"), STR("%Y %j"), date);
  check(STR("2026-W07-7"), STR("%G-W%V-%u"), date);
  check(STR("2026 07 0"), STR("%Y %U %w"), date);
  check(STR("2026 06 7"), STR("%Y %W %u"), date);
  check(STR("2026-02-15 Sun"), STR("%F %a"), date);
  check(STR("2026-02-15 Mon"), STR("%F %a"), date, false);
  check(STR("2026-02-30"), STR("%F"), date, false);
  check(STR("2026-02"), STR("%Y-%m"), date, false);
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
  assert((parse<CharT, Seconds>(STR("2026-07-20 13:45:30"), STR("%Y-%m-%d %H:%M:%S")) == date_time));

  // %e is equivalent to %d when parsing; leading zeroes are optional.
  assert((parse<CharT, Seconds>(STR("2026-07-7"), STR("%Y-%m-%e")) == sys_days{2026y / July / 7}));

  // Compound specifiers expand to the numeric ones.
  assert((parse<CharT, Seconds>(STR("2026-07-20 13:45:30"), STR("%F %T")) == date_time));
  assert((parse<CharT, Seconds>(STR("2026-07-20 13:45"), STR("%F %R")) == date + 13h + 45min));

  // A width on %F applies only to %Y; %m and %d retain their default widths.
  assert((parse<CharT, Seconds>(STR("002026-07-20"), STR("%6F")) == date));

  // No unsigned underflow when the width leaves no room for fractional digits.
  for (const auto& format : {STR("%1S"), STR("%2S")}) {
    std::basic_istringstream<CharT> stream{STR("1.25")};
    milliseconds result{};
    from_stream(stream, format.c_str(), result);
    assert(!stream.fail());
    assert(result == 1s);
    assert(stream.peek() == CharT('.'));
  }

  // ISO week dates combine the separately parsed year, week, and weekday fields.
  assert((parse<CharT, Seconds>(STR("2026-W30-1"), STR("%G-W%V-%u")) == date));
  assert((parse<CharT, Seconds>(STR("26-W30-1"), STR("%g-W%V-%u")) == date));

  // Missing time-of-day defaults to midnight.
  assert((parse<CharT, Seconds>(STR("2026-07-20"), STR("%F")) == date));

  // A literal '%' and explicit whitespace.
  assert((parse<CharT, Seconds>(STR("2026-07-20%"), STR("%F%%")) == date));
  assert((parse<CharT, Seconds>(STR("   2026-07-20"), STR(" %F")) == date));

  // Whitespace in the format matches zero or more whitespace in the input.
  assert((parse<CharT, Seconds>(STR("2026-07-20"), STR(" %Y-%m-%d")) == date));

  // Parse mismatches set failbit with the default exception mask.

  parse<CharT, Seconds>(STR("2026-02-30"), STR("%Y-%m-%d"), /*expected_fail=*/true); // invalid date
  parse<CharT, Seconds>(STR("2026/07/20"), STR("%Y-%m-%d"), /*expected_fail=*/true); // literal mismatch
  parse<CharT, Seconds>(STR("13:45:30"), STR("%H:%M:%S"), /*expected_fail=*/true);   // no date component
  parse<CharT, Seconds>(
      STR("2026-07-xx"), STR("%Y-%m-%d"), /*expected_fail=*/true); // non-digit where a digit is required
  parse<CharT, Seconds>(STR("2026-07- 7"), STR("%Y-%m-%e"), /*expected_fail=*/true); // %e does not skip whitespace
  parse<CharT, Seconds>(STR(""), STR("%Y"), /*expected_fail=*/true);                 // empty input
  parse<CharT, Seconds>(STR("2026/07/20"), STR("%6D"), /*expected_fail=*/true);      // %D does not allow a width
}

} // namespace numeric_tests

template <class CharT>
void test_time_point_resolution() {
  using namespace std::chrono;
  const sys_seconds expected = sys_days{2026y / July / 20} + 13h + 45min + 30s;
  auto test                  = [&](auto value) {
    check(STR("20 26-07-20 01:45:30 PM"), STR("%C %y-%m-%d %I:%M:%S %p"), value);
    check(STR("20 26 201 13 01:45:30 PM"), STR("%C %y %j %H %I:%M:%S %p"), value);
    check(STR("2026-07-20 13 01:45:30"), STR("%F %H %I:%M:%S"), value);
    check(STR("2026-07-20 01:45:30 13"), STR("%F %I:%M:%S %H"), value);
    check_failure(STR("2026-07-20 14 01:45:30"), STR("%F %H %I:%M:%S"), value);
    check_failure(STR("2026 25-07-20 13:45:30"), STR("%Y %y-%m-%d %T"), value);
    check_failure(STR("2026-07-20 12 01:45:30 PM"), STR("%F %H %I:%M:%S %p"), value);
    check_failure(STR("2026-07-20 13:45:30 AM"), STR("%F %T %p"), value);
    check_failure(STR("2026-07-20 24:00:00"), STR("%F %T"), value);
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
    std::basic_istringstream<CharT> stream{STR("+32767")};
    year value{0};
    from_stream(stream, STR("%6Y").c_str(), value);
    assert(!stream.fail());
    assert(value == year::max());
  }
  {
    std::basic_istringstream<CharT> stream{STR("-32767")};
    year value{0};
    from_stream(stream, STR("%6Y").c_str(), value);
    assert(!stream.fail());
    assert(value == year::min());
  }
  {
    std::basic_istringstream<CharT> stream{STR("+32768X")};
    year value{2026};
    from_stream(stream, STR("%6Y").c_str(), value);
    assert(stream.fail());
    assert(value == year{2026});

    stream.clear();
    assert(stream.peek() == CharT('X'));
  }
  {
    std::basic_istringstream<CharT> stream{STR("-32769")};
    year value{2026};
    from_stream(stream, STR("%6Y").c_str(), value);
    assert(stream.fail());
    assert(value == year{2026});
  }
  {
    // The parser stores calendar fields in int, but year narrows internally.
    // Check the int value before construction so it cannot wrap to a valid year.
    std::basic_istringstream<CharT> stream{STR("+65537")};
    year value{2026};
    from_stream(stream, STR("%6Y").c_str(), value);
    assert(stream.fail());
    assert(value == year{2026});
  }
  {
    // Calendar components also narrow internally; reject the original value
    // before 257 can wrap to January.
    std::basic_istringstream<CharT> stream{STR("257")};
    month value{July};
    from_stream(stream, STR("%3m").c_str(), value);
    assert(stream.fail());
    assert(value == July);
  }
  {
    // Combining an individually valid int century with %y is checked too.
    std::basic_istringstream<CharT> stream{STR("+214748364799")};
    year value{2026};
    from_stream(stream, STR("%11C%2y").c_str(), value);
    assert(stream.fail());
    assert(value == year{2026});
  }

  // A sign consumes one character of a signed field's width.
  for (const auto& format : {STR("%3Y"), STR("%3G")}) {
    std::basic_istringstream<CharT> stream(STR("+123"));
    // Supply the remainder of an ISO date when testing %G.
    if (format == STR("%3G")) {
      stream.str(STR("+123-W01-1"));
      sys_days result{};
      from_stream(stream, (format + STR("3-W%V-%u")).c_str(), result);
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
  check(STR("+123"), STR("%4Y"), year{123});
  check(STR("0"), STR("%Y"), year{0});
  check(STR("68"), STR("%y"), year{2068});
  check(STR("69"), STR("%y"), year{1969});
  check_failure(STR("100"), STR("%3y"), year{42});
  check(STR("-123"), STR("%4Y"), year{-123});
  check_failure(STR("+1"), STR("%1Y"), year{42});
  check_failure(STR("-1"), STR("%1Y"), year{42});
  check(STR("-20 76"), STR("%3C %y"), year{-1976});
  check(STR("-20 00"), STR("%3C %y"), year{-2000});
  check(STR("-1 01"), STR("%2C %y"), year{-1});
  check(STR("-1 99"), STR("%2C %y"), year{-99});
  check(STR("-1 00"), STR("%2C %y"), year{-100});
  check(STR("-1976 -20"), STR("%5Y %3C"), year{-1976});
  check_failure(STR("-1976 -19"), STR("%5Y %3C"), year{42});
  check(STR("-0123-07-20"), STR("%5F"), year_month_day{year{-123}, July, day{20}});
}

template <class CharT>
void test_duration() {
  using namespace std::chrono;
  // Supply valid calendar fields so rejection tests the target's capabilities.
  check_failure(STR("15 01"), STR("%d %H"), 42s);
  check_failure(STR("07 01"), STR("%m %H"), 42s);
  check_failure(STR("2026 01"), STR("%Y %H"), 42s);
  check_failure(STR("Mon 01"), STR("%a %H"), 42s);
  check_failure(STR("Jul 01"), STR("%b %H"), 42s);
  check_failure(STR("2026-07-20 01"), STR("%F %H"), 42s);
  check_failure(STR("2026 30 1 01"), STR("%G %V %u %H"), 42s);
  check_failure(STR("2026 29 1 01"), STR("%Y %U %w %H"), 42s);

  // Parse time-of-day and day-count fields into a duration.
  for (const auto& format : {STR("%T"), STR("%X"), STR("%EX")})
    check(STR("01:30:00"), format, 90min);
  check(STR("01:30:00 AM"), STR("%r"), 90min);
  check(STR("PM 01:30"), STR("%p %I:%M"), 810min);
  check(STR("2 01:30"), STR("%j %R"), 48h + 90min);
  check(STR("1 01"), STR("%j %H"), 25h);
  check(STR("23:59:59"), STR("%T"), 23h + 59min + 59s);
  check_failure(STR("24"), STR("%H"), 42h);
  check_failure(STR("60"), STR("%M"), 42min);
  check_failure(STR("60"), STR("%S"), 42s);
  check_failure(STR("60.0"), STR("%S"), milliseconds{42});
  check_failure(STR("60.0"), STR("%S"), duration<double, std::milli>{42});
  check(STR("13 01 PM"), STR("%H %I %p"), 13h);
  check(STR("13 01"), STR("%H %I"), 13h);
  check(STR("01 13"), STR("%I %H"), 13h);
  check(STR("00 12"), STR("%H %I"), 0h);
  check(STR("12 12"), STR("%H %I"), 12h);
  check_failure(STR("14 01"), STR("%H %I"), 42min);
  check_failure(STR("25 01"), STR("%H %I"), 42min);
  check_failure(STR("12 01 PM"), STR("%H %I %p"), 42min);
  check_failure(STR("13 AM"), STR("%H %p"), 42min);
  check_failure(STR("PM"), STR("%p"), 42min);
  check_failure(STR("00 PM"), STR("%I %p"), 42min);
  check_failure(STR("13 PM"), STR("%I %p"), 42min);

  // Check duration conversion and representable boundary values.
  check_failure(STR("2147483647"), STR("%10H"), nanoseconds{42});
  check(STR("106751 23:47:16.854775807"), STR("%6j %T"), nanoseconds::max());
  using UnsignedNanos = duration<std::uint64_t, std::nano>;
  check(STR("213503 23:34:33.709551615"), STR("%6j %T"), UnsignedNanos::max());
  using Tiny = duration<signed char>;
  check(STR("02:07"), STR("%M:%S"), Tiny::max());
  check(STR("0"), STR("%S"), duration<unsigned>{0});
  check(STR("1.25"), STR("%S"), duration<double, std::milli>{1250});
  check(STR("2.50"), STR("%S"), duration<int, std::ratio<3, 2>>{1});
  check(STR("1 12"), STR("%j %H"), duration<int, std::ratio<129600>>{1});
  // Exercise a custom period without overflowing the intermediate representation.
  using NearSecond = duration<std::uint64_t, std::ratio<8000000001LL, 8000000000LL>>;
  check(STR("00:01:00"), STR("%T"), NearSecond{59});
}

template <class CharT>
void test_offsets() {
  using namespace std::chrono;
  // Offset signs are optional for all three spellings.
  const sys_seconds date = sys_days{2026y / July / 20};
  check(STR("2026-07-20 04"), STR("%F %z"), date - 4h);
  check(STR("2026-07-20 0430"), STR("%F %z"), date - 4h - 30min);
  // Offset minutes are two digits, not a clock-minute field restricted to 0-59.
  check(STR("2026-07-20 +0160"), STR("%F %z"), date - 120min);
  check(STR("2026-07-20 -0199"), STR("%F %z"), date + 159min);
  check_failure(STR("2026-07-20 019"), STR("%F %z"), sys_seconds{42s});
  for (const auto& format : {STR("%F %Ez"), STR("%F %Oz")}) {
    check(STR("2026-07-20 4"), format, date - 4h);
    check(STR("2026-07-20 4:30"), format, date - 4h - 30min);
    check(STR("2026-07-20 +4:30"), format, date - 4h - 30min);
    check(STR("2026-07-20 -4:30"), format, date + 4h + 30min);
    check(STR("2026-07-20 1:60"), format, date - 120min);
    check(STR("2026-07-20 +01:90"), format, date - 150min);
    check(STR("2026-07-20 -1:99"), format, date + 159min);
    check_failure(STR("2026-07-20 4:"), format, sys_seconds{42s});
    check_failure(STR("2026-07-20 4:9"), format, sys_seconds{42s});
  }
  check_failure(STR("2026-07-20 4"), STR("%F %z"), sys_seconds{42s});
  check_failure(STR("2026-07-20 +"), STR("%F %z"), sys_seconds{42s});
}

namespace custom_rep_tests {

template <class CharT>
void test() {
  using namespace std::chrono;
  using Int   = rep<long long>;
  using Float = rep<double>;

  check(STR("2 01:02:03"), STR("%j %T"), duration<Int>{176523});
  check(STR("01:30"), STR("%R"), duration<Int, std::ratio<60>>{90});
  check(STR("1.250"), STR("%S"), duration<Int, std::milli>{1250});

  // Combine before truncating: 1 second and 0.5 seconds together make one tick.
  check(STR("1.5"), STR("%S"), duration<Int, std::ratio<3, 2>>{1});
  check(STR("1.4"), STR("%S"), duration<Int, std::ratio<3, 2>>{0});
  check(STR("1 12"), STR("%j %H"), duration<Int, std::ratio<129600>>{1});

  check(STR("1.250"), STR("%S"), duration<Float, std::milli>{1250.0});
  check(STR("1.5"), STR("%S"), duration<Float, std::ratio<3, 2>>{1.0});
  check(STR("0.3"), STR("%S"), duration<Float, std::ratio<3, 2>>{0.2});
  check(STR("01:30:00"), STR("%T"), duration<Float, std::ratio<3600>>{1.5});

  // Parse failures must still preserve the target.
  std::basic_istringstream<CharT> stream(STR("1.250 ?"));
  duration<Int, std::milli> result{42};
  from_stream(stream, STR("%S !").c_str(), result);
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

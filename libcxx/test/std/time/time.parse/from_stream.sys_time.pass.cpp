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

// template<class charT, class traits, class Duration, class Alloc = allocator<charT>>
//   basic_istream<charT, traits>&
//     from_stream(basic_istream<charT, traits>& is, const charT* fmt,
//                 sys_time<Duration>& tp,
//                 basic_string<charT, traits, Alloc>* abbrev = nullptr,
//                 minutes* offset = nullptr);

// This covers the Phase 1 subset: the numeric specifiers %Y %m %d %H %M %S, the
// compound specifiers %F %T %R, %% / whitespace handling, and the failbit
// reporting model (from_stream never throws on a parse mismatch).

#include <chrono>
#include <cassert>
#include <ctime>
#include <locale>
#include <sstream>

#include "make_string.h"
#include "test_macros.h"

#define ST(S) MAKE_STRING(CharT, S)

template <class CharT>
class alternative_day_time_get : public std::time_get<CharT> {
  using Base = std::time_get<CharT>;

public:
  using iter_type = typename Base::iter_type;

protected:
  iter_type do_get(iter_type begin,
                   iter_type end,
                   std::ios_base&,
                   std::ios_base::iostate& err,
                   std::tm* value,
                   char spec,
                   char modifier) const override {
    if ((spec != 'd' && spec != 'e') || modifier != 'O') {
      err |= std::ios_base::failbit;
      return begin;
    }

    for (char expected : {'x', 'y', 'z'}) {
      if (begin == end) {
        err |= std::ios_base::eofbit | std::ios_base::failbit;
        return begin;
      }
      if (*begin != CharT(expected)) {
        err |= std::ios_base::failbit;
        return begin;
      }
      ++begin;
    }

    value->tm_mday = 7;
    if (begin == end)
      err |= std::ios_base::eofbit;
    return begin;
  }
};

// Parses 'input' with 'fmt' into a sys_time<Duration> and asserts the resulting
// stream state, then returns the (possibly untouched) time_point.
template <class CharT, class Duration>
static std::chrono::sys_time<Duration>
parse(const std::basic_string<CharT>& input, const std::basic_string<CharT>& fmt, bool expected_fail = false) {
  std::basic_istringstream<CharT> stream{input};
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

  auto parse_alternative_day = [&](const std::basic_string<CharT>& fmt, bool expected_fail) {
    std::basic_istringstream<CharT> stream{ST("2026-07-xyz!")};
    stream.imbue(std::locale(std::locale::classic(), new alternative_day_time_get<CharT>));
    sys_time<Seconds> result{};
    from_stream(stream, fmt.c_str(), result);
    assert(stream.fail() == expected_fail);
    return result;
  };

  // --- Success cases -------------------------------------------------------

  // Individual numeric specifiers.
  assert((parse<CharT, Seconds>(ST("2026-07-20 13:45:30"), ST("%Y-%m-%d %H:%M:%S")) == date_time));

  // %e is equivalent to %d when parsing; leading zeroes are optional.
  assert((parse<CharT, Seconds>(ST("2026-07-7"), ST("%Y-%m-%e")) == sys_days{2026y / July / 7}));

  // Alternative day representations need not fit the ordinary two-digit width.
  assert((parse_alternative_day(ST("%Y-%m-%Od!"), false) == sys_days{2026y / July / 7}));
  assert((parse_alternative_day(ST("%Y-%m-%Oe!"), false) == sys_days{2026y / July / 7}));

  // Compound specifiers expand to the numeric ones.
  assert((parse<CharT, Seconds>(ST("2026-07-20 13:45:30"), ST("%F %T")) == date_time));
  assert((parse<CharT, Seconds>(ST("2026-07-20 13:45"), ST("%F %R")) == date + 13h + 45min));

  // A width on %F applies only to %Y; %m and %d retain their default widths.
  assert((parse<CharT, Seconds>(ST("002026-07-20"), ST("%6F")) == date));

  // ISO week dates combine the separately parsed year, week, and weekday fields.
  assert((parse<CharT, Seconds>(ST("2026-W30-1"), ST("%G-W%V-%u")) == date));
  assert((parse<CharT, Seconds>(ST("26-W30-1"), ST("%g-W%V-%u")) == date));

  // A leading minus sign applies to all duration fields, including fractions.
  struct DurationCase {
    std::basic_string<CharT> input;
    std::basic_string<CharT> format;
    milliseconds expected;
  };
  const DurationCase duration_cases[] = {
      {ST("-01:30"), ST("%H:%M"), -90min},
      {ST("-30:15"), ST("%M:%S"), -(30min + 15s)},
      {ST("-1.25"), ST("%S"), -1250ms},
      {ST("-2 01:30"), ST("%j %H:%M"), -(48h + 1h + 30min)},
      {ST("-01:30"), ST("%R"), -90min},
  };
  for (const auto& c : duration_cases) {
    std::basic_istringstream<CharT> stream(c.input);
    milliseconds result{};
    from_stream(stream, c.format.c_str(), result);
    assert(!stream.fail());
    assert(result == c.expected);
  }
  for (const auto& c : {DurationCase{ST("1:-30"), ST("%H:%M"), 42ms}, DurationCase{ST("-1."), ST("%S"), 42ms}}) {
    std::basic_istringstream<CharT> stream(c.input);
    milliseconds result = c.expected;
    from_stream(stream, c.format.c_str(), result);
    assert(stream.fail());
    assert(result == c.expected);
  }

  // Missing time-of-day defaults to midnight.
  assert((parse<CharT, Seconds>(ST("2026-07-20"), ST("%F")) == date));

  // A literal '%' and explicit whitespace.
  assert((parse<CharT, Seconds>(ST("2026-07-20%"), ST("%F%%")) == date));
  assert((parse<CharT, Seconds>(ST("   2026-07-20"), ST(" %F")) == date));

  // Whitespace in the format matches zero or more whitespace in the input.
  assert((parse<CharT, Seconds>(ST("2026-07-20"), ST("%Y-%m-%d")) == date));

  // --- Failure cases (reported via failbit, never thrown) ------------------

  parse<CharT, Seconds>(ST("2026-02-30"), ST("%Y-%m-%d"), /*expected_fail=*/true); // invalid date
  parse<CharT, Seconds>(ST("2026/07/20"), ST("%Y-%m-%d"), /*expected_fail=*/true); // literal mismatch
  parse<CharT, Seconds>(ST("13:45:30"), ST("%H:%M:%S"), /*expected_fail=*/true);   // no date component
  parse<CharT, Seconds>(
      ST("2026-07-xx"), ST("%Y-%m-%d"), /*expected_fail=*/true);   // non-digit where a digit is required
  parse<CharT, Seconds>(ST("2026-07- 7"), ST("%Y-%m-%e"), /*expected_fail=*/true); // %e does not skip whitespace
  parse_alternative_day(ST("%Y-%m-%Oe?"), true); // A following literal must still match.
  parse<CharT, Seconds>(ST(""), ST("%Y"), /*expected_fail=*/true); // empty input
  parse<CharT, Seconds>(ST("2026/07/20"), ST("%6D"), /*expected_fail=*/true); // %D does not allow a width

  // Calendar year boundaries are accepted; values outside the target are
  // rejected without modifying the result.
  {
    std::basic_istringstream<CharT> stream{ST("+32767")};
    year value{0};
    from_stream(stream, ST("%5Y").c_str(), value);
    assert(!stream.fail());
    assert(value == year::max());
  }
  {
    std::basic_istringstream<CharT> stream{ST("-32767")};
    year value{0};
    from_stream(stream, ST("%5Y").c_str(), value);
    assert(!stream.fail());
    assert(value == year::min());
  }
  {
    std::basic_istringstream<CharT> stream{ST("+32768X")};
    year value{2026};
    from_stream(stream, ST("%5Y").c_str(), value);
    assert(stream.fail());
    assert(value == year{2026});

    stream.clear();
    assert(stream.peek() == CharT('X'));
  }
  {
    std::basic_istringstream<CharT> stream{ST("-32769")};
    year value{2026};
    from_stream(stream, ST("%5Y").c_str(), value);
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
    // A signed field is bounded by int and still consumes all requested digits
    // when it overflows.
    std::basic_istringstream<CharT> stream{ST("+2147483648X")};
    year value{2026};
    from_stream(stream, ST("%10Y").c_str(), value);
    assert(stream.fail());
    assert(value == year{2026});

    stream.clear();
    assert(stream.peek() == CharT('X'));
  }
  {
    // Combining an individually valid int century with %y is checked too.
    std::basic_istringstream<CharT> stream{ST("+214748364799")};
    year value{2026};
    from_stream(stream, ST("%10C%2y").c_str(), value);
    assert(stream.fail());
    assert(value == year{2026});
  }

  // The seconds field uses an int intermediate. Its maximum is accepted for
  // a duration, while the next value is rejected and fully consumed.
  {
    std::basic_istringstream<CharT> stream{ST("2147483647")};
    duration<long long> value{};
    from_stream(stream, ST("%10S").c_str(), value);
    assert(!stream.fail());
    assert(value == duration<long long>{2147483647});
  }
  {
    std::basic_istringstream<CharT> stream{ST("2147483648X")};
    duration<long long> value{42};
    from_stream(stream, ST("%10S").c_str(), value);
    assert(stream.fail());
    assert(value == duration<long long>{42});

    stream.clear();
    assert(stream.peek() == CharT('X'));
  }

  // An oversized width is itself a parse failure and must not overflow int.
  {
    std::basic_istringstream<CharT> stream{ST("2026")};
    year value{2026};
    from_stream(stream, ST("%999999999999999999999999Y").c_str(), value);
    assert(stream.fail());
    assert(value == year{2026});
  }
}

int main(int, char**) {
  test<char>();
#ifndef TEST_HAS_NO_WIDE_CHARACTERS
  test<wchar_t>();
#endif

  return 0;
}

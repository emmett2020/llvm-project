//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17
// UNSUPPORTED: no-localization

#include <cassert>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <format>
#include <initializer_list>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

#include "make_string.h"
#include "test_macros.h"

#define STR(S) MAKE_STRING(CharT, S)

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
void test_read_digits() {
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
      {STR("12345"), 5, 12, 12, 3, true, CharT('4')},
      {STR("12345"), 2, 12, 12, 2, false, CharT('3')},
      {STR("9999"), 4, 12, 9, 2, true, CharT('9')},
      {STR("12"), 2, 0, 0, 1, true, CharT('2')},
      {STR("0012"), 4, 0, 0, 3, true, CharT('2')},
      {STR("12"), 0, 12, 0, 0, false, CharT('1')},
      {STR("X"), 1, 12, 0, 0, false, CharT('X')},
      {STR("18446744073709551615X"),
       21,
       std::numeric_limits<std::uint64_t>::max(),
       std::numeric_limits<std::uint64_t>::max(),
       20,
       false,
       CharT('X')},
      {STR("184467440737095516160X"),
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

void test_read_signed() {
  for (int expected : {std::numeric_limits<int>::min(), -1, 0, 1, std::numeric_limits<int>::max()}) {
    const std::string input = std::to_string(expected);
    std::istringstream stream{input};
    int result = 42;
    std::chrono::__read_signed(stream, static_cast<unsigned>(input.size()), result);
    assert(!stream.fail());
    assert(result == expected);
  }
}

void test_field_queries() {
  using Fields         = std::chrono::__fields_storage;
  using Parts          = std::chrono::__fields_set;
  constexpr auto check = [] {
    Fields fields;
    assert(fields.__has(Parts::__none));
    assert(!fields.__has_any(Parts::__none));
    assert(!fields.__has(Parts::__day));
    fields.__set(Parts::__utc_offset);
    assert(fields.__has(Parts::__utc_offset));
    assert(fields.__has_only(Parts::__none));
    assert(fields.__has_exactly(Parts::__none));
    fields.__set(Parts::__day);
    assert(fields.__has(Parts::__day | Parts::__utc_offset));
    assert(fields.__has_any(Parts::__day | Parts::__month));
    assert(!fields.__has(Parts::__day | Parts::__month));
    assert(fields.__has_only(Parts::__day | Parts::__month));
    assert(fields.__has_exactly(Parts::__day, Parts::__month));
    fields.__set(Parts::__month);
    assert(!fields.__has_only(Parts::__day));
    assert(!fields.__has_exactly(Parts::__day));
    assert(fields.__has_exactly(Parts::__day, Parts::__month));
    assert(fields.__has_exactly(Parts::__day | Parts::__month));
    return true;
  };
  static_assert(check());
  assert(check());
}

template <class CharT>
void test_numeric_limits() {
  using namespace std::chrono;
  const sys_seconds date = sys_days{2026y / July / 20};
  // Widths can use the full unsigned range for both signed and unsigned fields.
  {
    std::basic_ostringstream<CharT> format;
    format << CharT('%') << std::numeric_limits<unsigned>::max();
    check(STR("2026-07-20"), format.str() + STR("F"), date);

    std::basic_istringstream<CharT> stream{STR("1.25!")};
    milliseconds result{};
    from_stream(stream, (format.str() + STR("S!")).c_str(), result);
    assert(!stream.fail());
    assert(result == 1250ms);

    // An overflowing width fails before consuming input or changing the result.
    stream.str(STR("1.25!"));
    stream.clear();
    result = 42ms;
    from_stream(stream, (format.str() + STR("0S")).c_str(), result);
    assert(stream.fail());
    assert(result == 42ms);
    stream.clear();
    assert(stream.peek() == CharT('1'));
  }

  // Stop after the first overflowing digit, leaving the remaining digits unread.
  for (const auto& input : {STR("+214748364812X"), STR("-214748364912X")}) {
    std::basic_istringstream<CharT> stream{input};
    year value{2026};
    from_stream(stream, STR("%13Y").c_str(), value);
    assert(stream.fail());
    assert(value == year{2026});

    stream.clear();
    assert(stream.peek() == CharT('1'));
  }
}

template <class CharT>
void test_duration_signs() {
  using namespace std::chrono;
  // Signs are rejected by numeric duration fields, not treated as an overall sign.
  for (const auto& sign : {STR("+"), STR("-")}) {
    for (const auto& fmt : {STR("%H"), STR("%I"), STR("%M"), STR("%S"), STR("%j")}) {
      check_failure(sign + STR("01"), fmt, 42s);
      check_failure(STR("01 ") + sign + STR("01"), STR("%H ") + fmt, 42s);
    }
    check_failure(sign + STR("1.25"), STR("%S"), 42ms);
    check_failure(sign + STR("01:30"), STR("%R"), 42min);
    check_failure(STR("01:") + sign + STR("30"), STR("%R"), 42min);
    // An explicit sign in the format is just a literal.
    check(sign + STR("01:30"), sign + STR("%R"), 90min);
  }
  // %I alone is not interpreted as AM.
  check_failure(STR("01"), STR("%I"), 42h);
  check_failure(STR("2026-07-20 01:45:30"), STR("%F %I:%M:%S"), sys_seconds{42s});

  std::basic_istringstream<CharT> stream(STR("01:30 -04:30"));
  minutes result{};
  minutes offset{};
  from_stream(stream, STR("%R %Ez").c_str(), result, static_cast<std::basic_string<CharT>*>(nullptr), &offset);
  assert(!stream.fail());
  assert(result == 90min);
  assert(offset == -270min);
}

template <class CharT>
void check_date(const std::basic_string<CharT>& input,
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
void test_date_consistency() {
  using namespace std::chrono;
  const year_month_day feb1 = 2026y / February / 1;
  check_date(STR("2026 032 02"), STR("%Y %j %m"), feb1);
  check_date(STR("2026 032 03"), STR("%Y %j %m"), feb1, false);
  check_date(STR("2026 032 01"), STR("%Y %j %d"), feb1);
  check_date(STR("2026 032 02"), STR("%Y %j %d"), feb1, false);
  check_date(STR("03 2026 032"), STR("%m %Y %j"), feb1, false);

  // A week date can supply the complete date while calendar fields constrain it.
  check_date(STR("2026 05 0 02"), STR("%Y %U %w %m"), feb1);
  check_date(STR("2026 05 0 03"), STR("%Y %U %w %m"), feb1, false);
  check_date(STR("2026 04 7 01"), STR("%Y %W %u %d"), feb1);
  check_date(STR("2026 04 7 02"), STR("%Y %W %u %d"), feb1, false);

  const year_month_day jan1 = 2021y / January / 1;
  check_date(STR("2020-W53-5 2021 01 01 001 20"), STR("%G-W%V-%u %Y %m %d %j %C"), jan1);
  check_date(STR("2020-W53-5 2020"), STR("%G-W%V-%u %Y"), jan1, false);
  check_date(STR("2020-W53-5 02"), STR("%G-W%V-%u %m"), jan1, false);
  check_date(STR("2020-W53-5 02"), STR("%G-W%V-%u %d"), jan1, false);
  check_date(STR("2020-W53-5 002"), STR("%G-W%V-%u %j"), jan1, false);
  check_date(STR("2020-W53-5 21"), STR("%G-W%V-%u %C"), jan1, false);
  check_date(STR("2020-W53-5 20"), STR("%G-W%V-%u %C"), jan1);

  // Standalone ISO fields must agree, including across an ISO-year boundary.
  check_date(STR("2021-01-01 2020"), STR("%F %G"), jan1);
  check_date(STR("2021-01-01 2021"), STR("%F %G"), jan1, false);
  check_date(STR("2021-01-01 53"), STR("%F %V"), jan1);
  check_date(STR("2021-01-01 01"), STR("%F %V"), jan1, false);
  const year_month_day dec31 = 2018y / December / 31;
  check_date(STR("2018-12-31 2019 01"), STR("%F %G %V"), dec31);
  check_date(STR("2018-12-31 2018"), STR("%F %G"), dec31, false);
  check_date(STR("2018-12-31 52"), STR("%F %V"), dec31, false);

  // Week zero, different week starts, and the extra weekday are checked separately.
  for (const auto& format : {STR("%F %U"), STR("%F %W")}) {
    check_date(STR("2021-01-01 00"), format, jan1);
    check_date(STR("2021-01-01 01"), format, jan1, false);
  }
  check_date(STR("2018-12-31 52 53"), STR("%F %U %W"), dec31);
  check_date(STR("2018-12-31 53"), STR("%F %U"), dec31, false);
  check_date(STR("2018-12-31 52"), STR("%F %W"), dec31, false);
  check_date(STR("2021-01-01 Fri"), STR("%F %a"), jan1);
  check_date(STR("2021-01-01 Thu"), STR("%F %a"), jan1, false);

  // A complete calendar date must not hide conflicting or invalid alternatives.
  check_date(STR("2021-01-01 2020-W53-5 001"), STR("%F %G-W%V-%u %j"), jan1);
  check_date(STR("2021-01-01 2020-W52-5"), STR("%F %G-W%V-%u"), jan1, false);
  check_date(STR("2021-01-01 2021-W53-5"), STR("%F %G-W%V-%u"), jan1, false);
  check_date(STR("2021-01-01 366"), STR("%F %j"), jan1, false);

  // An invalid complete calendar representation must not fall back to ISO fields.
  check_date(STR("2021-02-30 2020-W53-5"), STR("%F %G-W%V-%u"), jan1, false);
  check_date(STR("2021 366 2020-W53-5"), STR("%Y %j %G-W%V-%u"), jan1, false);
  check_date(STR("2021 001 2020-W53-5"), STR("%Y %j %G-W%V-%u"), jan1);
  check_date(STR("2021 002 2020-W53-5"), STR("%Y %j %G-W%V-%u"), jan1, false);
  check_date(STR("2021 00 5 2020 53"), STR("%Y %U %w %G %V"), jan1);
  check_date(STR("2021 00 5 2020 52"), STR("%Y %U %w %G %V"), jan1, false);
  check_date(STR("2021 00 5 2020 53"), STR("%Y %W %w %G %V"), jan1);
  check_date(STR("2021 00 5 2021 53"), STR("%Y %W %w %G %V"), jan1, false);

  // Leap days and a negative calendar year's century.
  check_date(STR("2024 060 02 29"), STR("%Y %j %m %d"), 2024y / February / 29);
  check_date(STR("2024 060 03"), STR("%Y %j %m"), 2024y / February / 29, false);
  check_date(STR("-0001 001 -1"), STR("%5Y %j %2C"), year{-1} / January / 1);
  check_date(STR("-0001 001 00"), STR("%5Y %j %C"), year{-1} / January / 1, false);

  // ISO-year calculations must also work at the limits of chrono::year.
  check_date(STR("-32767-01-01 53"), STR("%6F %V"), year::min() / January / 1);
  check_date(STR("-32767-01-01 52"), STR("%6F %V"), year::min() / January / 1, false);
  check_date(STR("32767-01-01 32766 52"), STR("%5F %5G %V"), year::max() / January / 1);
  check_date(STR("32767-12-31 32767 52"), STR("%5F %5G %V"), year::max() / December / 31);

  // Ordinary dates do not need redundant fields.
  check_date(STR("2026-02-01"), STR("%F"), feb1);
  check_date(STR("2026 032"), STR("%Y %j"), feb1);
  check_date(STR("2020-W53-5"), STR("%G-W%V-%u"), jan1);
}

void test_date_cycle() {
  // Cover all Gregorian year-boundary patterns in a complete 400-year cycle.
  // Formatting provides the redundant fields independently of the parser.
  for (int y = 2000; y != 2400; ++y) {
    using namespace std::chrono;
    const sys_days jan1{year{y} / January / 1};
    for (int offset = -7; offset != 7; ++offset) {
      const sys_days date = jan1 + days{offset};
      check_date<char>(std::format("{:%F %j %U %W %G %V %u}", date), "%F %j %U %W %G %V %u", year_month_day{date});
    }
  }
}

template <class CharT>
void test_outputs() {
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
    auto abbrev                  = STR("original");
    minutes offset{42};
    from_stream(stream, format.c_str(), result, &abbrev, &offset);
    assert(stream.fail() == !success);
    assert(result == (success ? year{2026} / July / 20 : initial));
    assert(abbrev == expected_abbrev);
    assert(offset == expected_offset);
  };

  // A failed %Z, a later field/literal failure, and final validation failures.
  check(STR("2026-07-20 +0130 @"), STR("%F %z %Z"), false, STR("original"), 42min);
  check(STR("UTC +0130 invalid"), STR("%Z %z %Y"), false, STR("original"), 42min);
  check(STR("2026-07-20 UTC +0130 ?"), STR("%F %Z %z !"), false, STR("original"), 42min);
  check(STR("2026-02-30 UTC +0130"), STR("%F %Z %z"), false, STR("original"), 42min);
  check(STR("2026-07-20 25 UTC +0130"), STR("%F %y %Z %z"), false, STR("original"), 42min);
  check(STR("2026-07-20 29 UTC +0130"), STR("%F %V %Z %z"), false, STR("original"), 42min);
  check(STR("2026-07-20 00 UTC +0130"), STR("%F %H %Z %z"), false, STR("original"), 42min);
  check(STR("UTC +0130"), STR("%Z %z"), false, STR("original"), 42min);

  // Successful parsing, including EOF immediately after %Z or %z.
  check(STR("2026-07-20 UTC +0130"), STR("%F %Z %z"), true, STR("UTC"), 90min);
  check(STR("2026-07-20 -01:30 UTC"), STR("%F %Ez %Z"), true, STR("UTC"), -90min);
  check(STR("2026-07-20 +00:00 UTC"), STR("%F %Oz %Z"), true, STR("UTC"), 0min);

  // An absent directive leaves its output unchanged.
  check(STR("2026-07-20"), STR("%F"), true, STR("original"), 42min);
  check(STR("2026-07-20 UTC"), STR("%F %Z"), true, STR("UTC"), 42min);
  check(STR("2026-07-20 +0130"), STR("%F %z"), true, STR("original"), 90min);

  // UTC offsets and time zone abbreviations do not count as extra fields when constructing a day.
  auto check_day = [](const std::basic_string<CharT>& input, const std::basic_string<CharT>& format, bool success) {
    std::basic_istringstream<CharT> stream(input);
    stream.imbue(std::locale::classic());
    day result{1};
    auto abbrev = STR("original");
    minutes offset{42};
    from_stream(stream, format.c_str(), result, &abbrev, &offset);
    assert(stream.fail() == !success);
    assert(result == (success ? day{15} : day{1}));
    assert(abbrev == (success ? STR("UTC") : STR("original")));
    assert(offset == (success ? 480min : 42min));
  };
  check_day(STR("15 UTC +0800"), STR("%d %Z %z"), true);
  check_day(STR("UTC +0800"), STR("%Z %z"), false);
  check_day(STR("02-15 UTC +0800"), STR("%m-%d %Z %z"), false);
  check_day(STR("32 UTC +0800"), STR("%d %Z %z"), false);
}

template <class CharT, char Modifier>
class alternative_time_get : public std::time_get<CharT> {
  using Base = std::time_get<CharT>;
  char spec_;
  int value_;
  bool fail_;

public:
  using iter_type = typename Base::iter_type;
  alternative_time_get(char spec, int value, bool fail) : spec_(spec), value_(value), fail_(fail) {}

protected:
  iter_type
  do_get(iter_type first,
         iter_type last,
         std::ios_base& ios,
         std::ios_base::iostate& err,
         std::tm* value,
         char spec,
         char modifier) const override {
    if (modifier == 0)
      return Base::do_get(first, last, ios, err, value, spec, modifier);
    assert(modifier == Modifier);
    assert(spec == spec_);
    err = std::ios_base::goodbit;
    // The alternative representation need not fit the ordinary two-digit width.
    for (int i = 0; i < 3; ++i) {
      if (first == last) {
        err |= std::ios_base::eofbit | std::ios_base::failbit;
        return first;
      }
      if (*first != CharT('@')) {
        err |= std::ios_base::failbit;
        return first;
      }
      ++first;
    }
    if constexpr (Modifier == 'E') {
      value->tm_year = value_;
      value->tm_mon  = 6;
      value->tm_mday = 20;
      value->tm_hour = 13;
      value->tm_min  = 45;
      value->tm_sec  = 30;
    } else {
      switch (spec) {
      case 'd':
      case 'e':
        value->tm_mday = value_;
        break;
      case 'm':
        value->tm_mon = value_;
        break;
      case 'H':
      case 'I':
        value->tm_hour = value_;
        break;
      case 'M':
        value->tm_min = value_;
        break;
      case 'w':
        value->tm_wday = value_;
        break;
      case 'y':
        value->tm_year = value_;
        break;
      default:
        assert(false);
      }
    }
    if (first == last)
      err |= std::ios_base::eofbit;
    if (fail_)
      err |= std::ios_base::failbit;
    return first;
  }
};

template <class CharT, char Modifier>
void test_facet_state() {
  using namespace std::chrono;
  for (bool fail : {false, true}) {
    for (bool suffix : {false, true}) {
      const auto input = Modifier == 'E' ? STR("07-20 @@@") : STR("2026-07-@@@");
      const auto fmt   = Modifier == 'E' ? STR("%m-%d %EY") : STR("%Y-%m-%Od");
      std::basic_istringstream<CharT> stream(input + (suffix ? STR("!") : STR("")));
      stream.imbue(std::locale(
          std::locale::classic(),
          new alternative_time_get<CharT, Modifier>(Modifier == 'E' ? 'Y' : 'd', Modifier == 'E' ? 126 : 20, fail)));
      const sys_seconds initial{42s};
      sys_seconds result = initial;
      from_stream(stream, fmt.c_str(), result);
      assert(stream.fail() == fail);
      assert(result == (fail ? initial : sys_days{2026y / July / 20}));
      assert(stream.eof() == !suffix);
      if (!fail && suffix)
        assert(stream.peek() == CharT('!'));
    }
  }
}

template <class CharT>
void test_modifier_E() {
  using namespace std::chrono;
  const sys_seconds date = sys_days{2026y / July / 20};
  struct TestCase {
    std::basic_string<CharT> input;
    std::basic_string<CharT> format;
    char spec;
    int year;
    sys_seconds expected;
  };
  const TestCase cases[] = {
      {STR("@@@"), STR("%Ec"), 'c', 126, date + 13h + 45min + 30s},
      {STR("@@@"), STR("%Ex"), 'x', 126, date},
      {STR("2026-07-20 @@@"), STR("%F %EX"), 'X', 126, date + 13h + 45min + 30s},
      {STR("07-20 @@@"), STR("%m-%d %Ey"), 'y', 126, date},
      {STR("19 07-20 @@@"), STR("%EC %m-%d %Ey"), 'y', 126, sys_days{1926y / July / 20}},
      {STR("07-20 @@@"), STR("%m-%d %Ey"), 'y', 68, sys_days{2068y / July / 20}},
      {STR("07-20 @@@"), STR("%m-%d %Ey"), 'y', 69, sys_days{1969y / July / 20}},
      {STR("07-20 @@@"), STR("%m-%d %EY"), 'Y', 126, date},
      {STR("07-20 @@@"), STR("%m-%d %EY"), 'Y', -1901, sys_days{year{-1} / July / 20}},
  };
  for (const auto& c : cases) {
    std::basic_istringstream<CharT> stream(c.input + STR("!"));
    stream.imbue(std::locale(std::locale::classic(), new alternative_time_get<CharT, 'E'>(c.spec, c.year, false)));
    sys_seconds result{};
    from_stream(stream, c.format.c_str(), result);
    assert(!stream.fail());
    assert(result == c.expected);
    assert(stream.peek() == CharT('!'));
  }

  // Failed or truncated facet input must not fall back to numeric parsing.
  for (const auto& format : {STR("%Ey"), STR("%EY")}) {
    for (const auto& input : {STR("26"), STR("@@")}) {
      std::basic_istringstream<CharT> stream(input);
      stream.imbue(std::locale(
          std::locale::classic(), new alternative_time_get<CharT, 'E'>(static_cast<char>(format.back()), 126, false)));
      year result{42};
      from_stream(stream, format.c_str(), result);
      assert(stream.fail());
      assert(result == year{42});
    }
  }

  // Converting tm_year to a calendar year must not overflow int.
  {
    std::basic_istringstream<CharT> stream(STR("@@@"));
    stream.imbue(std::locale(
        std::locale::classic(), new alternative_time_get<CharT, 'E'>('Y', std::numeric_limits<int>::max(), false)));
    year result{42};
    from_stream(stream, STR("%EY").c_str(), result);
    assert(stream.fail());
    assert(result == year{42});
  }

  // %EC retains its numeric fallback; %Ez uses the dedicated offset parser.
  {
    std::basic_istringstream<CharT> stream(STR("20 26-07-20 +4:30"));
    stream.imbue(std::locale(std::locale::classic(), new alternative_time_get<CharT, 'E'>('?', 0, true)));
    sys_seconds result{};
    from_stream(stream, STR("%EC %y-%m-%d %Ez").c_str(), result);
    assert(!stream.fail());
    assert(result == date - 4h - 30min);
  }

  // The built-in facet accepts numeric years in the classic locale.
  for (const auto& format : {STR("%EC %Ey-%m-%d"), STR("%EC %EY-%m-%d")}) {
    std::basic_istringstream<CharT> stream(format == STR("%EC %Ey-%m-%d") ? STR("20 26-07-20") : STR("20 2026-07-20"));
    stream.imbue(std::locale::classic());
    sys_seconds result{};
    from_stream(stream, format.c_str(), result);
    assert(!stream.fail());
    assert(result == date);
  }
}

template <class CharT>
void test_modifier_O() {
  using namespace std::chrono;
  const sys_seconds date = sys_days{2026y / July / 20};

  struct TestCase {
    std::basic_string<CharT> input;
    std::basic_string<CharT> format;
    char spec;
    int value;
    sys_seconds expected;
  };
  const TestCase cases[] = {
      {STR("@@@-07-20"), STR("%Oy-%m-%d"), 'y', 126, date},
      {STR("19 @@@-07-20"), STR("%C %Oy-%m-%d"), 'y', 126, sys_days{1926y / July / 20}},
      {STR("@@@-07-20"), STR("%Oy-%m-%d"), 'y', 68, sys_days{2068y / July / 20}},
      {STR("@@@-07-20"), STR("%Oy-%m-%d"), 'y', 69, sys_days{1969y / July / 20}},
      {STR("2026-@@@-20"), STR("%Y-%Om-%d"), 'm', 6, date},
      {STR("2026-07-@@@"), STR("%Y-%m-%Od"), 'd', 20, date},
      {STR("2026-07-@@@"), STR("%Y-%m-%Oe"), 'e', 20, date},
      {STR("2026-07-20 @@@"), STR("%F %Ow"), 'w', 1, date},
      {STR("2026-07-20 @@@"), STR("%F %OH"), 'H', 13, date + 13h},
      {STR("2026-07-20 @@@ PM"), STR("%F %OI %p"), 'I', 1, date + 13h},
      {STR("2026-07-20 PM @@@"), STR("%F %p %OI"), 'I', 1, date + 13h},
      {STR("2026-07-20 @@@ AM"), STR("%F %OI %p"), 'I', 0, date},
      {STR("2026-07-20 @@@ PM"), STR("%F %OI %p"), 'I', 0, date + 12h},
      {STR("2026-07-20 @@@"), STR("%F %OM"), 'M', 45, date + 45min},
  };
  for (const auto& c : cases) {
    std::basic_istringstream<CharT> stream(c.input + STR("!"));
    stream.imbue(std::locale(std::locale::classic(), new alternative_time_get<CharT, 'O'>(c.spec, c.value, false)));
    sys_seconds result{};
    from_stream(stream, c.format.c_str(), result);
    assert(!stream.fail());
    assert(result == c.expected);
    assert(stream.peek() == CharT('!'));
  }

  // A truncated alternative representation still fails without changing the result.
  {
    std::basic_istringstream<CharT> stream(STR("2026-07-@@"));
    stream.imbue(std::locale(std::locale::classic(), new alternative_time_get<CharT, 'O'>('e', 20, false)));
    const sys_seconds initial{42s};
    sys_seconds result = initial;
    from_stream(stream, STR("%Y-%m-%Oe").c_str(), result);
    assert(stream.fail());
    assert(stream.eof());
    assert(result == initial);
  }

  {
    std::basic_istringstream<CharT> stream(STR("2026-07-@@@!"));
    stream.imbue(std::locale(std::locale::classic(), new alternative_time_get<CharT, 'O'>('e', 20, false)));
    sys_seconds result{42s};
    from_stream(stream, STR("%Y-%m-%Oe?").c_str(), result);
    assert(stream.fail());
    assert(result == sys_seconds{42s});
  }

  // Exercise the built-in facet, including %OI at midnight and noon.
  for (const auto& input : {STR("26-07-20 13:45 1"), STR("26-07-20 13:45 1!")}) {
    std::basic_istringstream<CharT> stream(input);
    stream.imbue(std::locale::classic());
    sys_seconds result{};
    from_stream(stream, STR("%Oy-%Om-%Od %OH:%OM %Ow").c_str(), result);
    assert(!stream.fail());
    assert(result == date + 13h + 45min);
  }

  // In the classic locale, %y, %Ey and %Oy all accept ordinary year digits.
  for (const auto& format : {STR("%y-%m-%d"), STR("%Ey-%m-%d"), STR("%Oy-%m-%d")}) {
    std::basic_istringstream<CharT> stream(STR("26-07-20"));
    stream.imbue(std::locale::classic());
    sys_seconds result{};
    from_stream(stream, format.c_str(), result);
    assert(!stream.fail());
    assert(result == date);
  }

  for (bool pm : {false, true}) {
    std::basic_istringstream<CharT> stream(STR("2026-07-20 12 ") + (pm ? STR("PM") : STR("AM")));
    stream.imbue(std::locale::classic());
    sys_seconds result{};
    from_stream(stream, STR("%F %OI %p").c_str(), result);
    assert(!stream.fail());
    assert(result == date + (pm ? 12h : 0h));
  }

  // A duration's first field can use a facet.
  {
    std::basic_istringstream<CharT> stream(STR("@@@:45"));
    stream.imbue(std::locale(std::locale::classic(), new alternative_time_get<CharT, 'O'>('H', 13, false)));
    minutes result{};
    from_stream(stream, STR("%OH:%M").c_str(), result);
    assert(!stream.fail());
    assert(result == 13h + 45min);
  }

  // %OS, %OU and %OW currently use their unmodified numeric parsers, and %Oz
  // uses its own offset parser. None of these should call time_get with O.
  {
    std::basic_istringstream<CharT> stream(STR("2026-07-20 13:45:30.125 +4:30"));
    stream.imbue(std::locale(std::locale::classic(), new alternative_time_get<CharT, 'O'>('?', 0, true)));
    sys_time<milliseconds> result{};
    from_stream(stream, STR("%F %H:%M:%OS %Oz").c_str(), result);
    assert(!stream.fail());
    assert(result == date + 13h + 45min + 30s + 125ms - 4h - 30min);
  }
  for (const auto& format : {STR("%Y %OU %w"), STR("%Y %OW %w")}) {
    std::basic_istringstream<CharT> stream(STR("2026 29 1"));
    stream.imbue(std::locale(std::locale::classic(), new alternative_time_get<CharT, 'O'>('?', 0, true)));
    sys_seconds result{};
    from_stream(stream, format.c_str(), result);
    assert(!stream.fail());
    assert(result == date);
  }

  // O is not a valid modifier for the ISO weekday or ISO week number.
  for (const auto& format : {STR("%F %Ou"), STR("%F %OV")}) {
    std::basic_istringstream<CharT> stream(STR("2026-07-20 1"));
    const sys_seconds initial{42s};
    sys_seconds result = initial;
    from_stream(stream, format.c_str(), result);
    assert(stream.fail());
    assert(result == initial);
  }
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
void test_facet_limits() {
  using namespace std::chrono;
  // Facet results must be checked before adding the year/month biases.
  for (const auto& format : {STR("%c"), STR("%Ec"), STR("%x"), STR("%Ex")}) {
    for (bool extreme_year : {false, true}) {
      std::basic_istringstream<CharT> stream(STR("@"));
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
  for (const auto& format : {STR("%b"), STR("%Om")}) {
    std::basic_istringstream<CharT> stream(STR("@"));
    stream.imbue(
        std::locale(std::locale::classic(), new extreme_time_get<CharT>(126, std::numeric_limits<int>::max())));
    month result{July};
    from_stream(stream, format.c_str(), result);
    assert(stream.fail());
    assert(result == July);
  }
}

template <class CharT>
void test() {
  test_read_digits<CharT>();
  test_numeric_limits<CharT>();
  test_duration_signs<CharT>();
  test_date_consistency<CharT>();
  test_outputs<CharT>();
  test_modifier_E<CharT>();
  test_modifier_O<CharT>();
  test_facet_state<CharT, 'E'>();
  test_facet_state<CharT, 'O'>();
  test_facet_limits<CharT>();
}

int main(int, char**) {
  test_read_signed();
  test_field_queries();
  test_date_cycle();
  test<char>();
#ifndef TEST_HAS_NO_WIDE_CHARACTERS
  test<wchar_t>();
#endif
  return 0;
}

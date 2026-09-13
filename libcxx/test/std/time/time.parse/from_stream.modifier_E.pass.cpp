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
// from_stream: E modifiers

#include <cassert>
#include <chrono>
#include <ctime>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

#include "make_string.h"
#include "test_macros.h"

#define ST(S) MAKE_STRING(CharT, S)

template <class CharT>
class alternative_time_get : public std::time_get<CharT> {
  using Base = std::time_get<CharT>;
  char spec_;
  int year_;
  bool fail_;

public:
  using iter_type = typename Base::iter_type;

  alternative_time_get(char spec, int year, bool fail) : spec_(spec), year_(year), fail_(fail) {}

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

    assert(modifier == 'E');
    assert(spec == spec_);
    err = std::ios_base::goodbit;
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

    value->tm_year = year_;
    value->tm_mon  = 6;
    value->tm_mday = 20;
    value->tm_hour = 13;
    value->tm_min  = 45;
    value->tm_sec  = 30;
    if (first == last)
      err |= std::ios_base::eofbit;
    if (fail_)
      err |= std::ios_base::failbit;
    return first;
  }
};

template <class CharT>
void test() {
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
      {ST("@@@"), ST("%Ec"), 'c', 126, date + 13h + 45min + 30s},
      {ST("@@@"), ST("%Ex"), 'x', 126, date},
      {ST("2026-07-20 @@@"), ST("%F %EX"), 'X', 126, date + 13h + 45min + 30s},
      {ST("07-20 @@@"), ST("%m-%d %Ey"), 'y', 126, date},
      {ST("19 07-20 @@@"), ST("%EC %m-%d %Ey"), 'y', 126, sys_days{1926y / July / 20}},
      {ST("07-20 @@@"), ST("%m-%d %Ey"), 'y', 68, sys_days{2068y / July / 20}},
      {ST("07-20 @@@"), ST("%m-%d %Ey"), 'y', 69, sys_days{1969y / July / 20}},
      {ST("07-20 @@@"), ST("%m-%d %EY"), 'Y', 126, date},
      {ST("07-20 @@@"), ST("%m-%d %EY"), 'Y', -1901, sys_days{year{-1} / July / 20}},
  };
  for (const auto& c : cases) {
    for (bool fail : {false, true}) {
      for (bool suffix : {false, true}) {
        std::basic_istringstream<CharT> stream(c.input + (suffix ? ST("!") : ST("")));
        stream.imbue(std::locale(std::locale::classic(), new alternative_time_get<CharT>(c.spec, c.year, fail)));
        const sys_seconds initial{42s};
        sys_seconds result = initial;
        from_stream(stream, c.format.c_str(), result);
        assert(stream.fail() == fail);
        assert(result == (fail ? initial : c.expected));
        if (!fail) {
          if (suffix)
            assert(stream.peek() == CharT('!'));
          else
            assert(stream.eof());
        }
      }
    }
  }

  // Failed or truncated facet input must not fall back to numeric parsing.
  for (const auto& format : {ST("%Ey"), ST("%EY")}) {
    for (const auto& input : {ST("26"), ST("@@")}) {
      std::basic_istringstream<CharT> stream(input);
      stream.imbue(std::locale(
          std::locale::classic(), new alternative_time_get<CharT>(static_cast<char>(format.back()), 126, false)));
      year result{42};
      from_stream(stream, format.c_str(), result);
      assert(stream.fail());
      assert(result == year{42});
    }
  }

  // Converting tm_year to a calendar year must not overflow int.
  {
    std::basic_istringstream<CharT> stream(ST("@@@"));
    stream.imbue(std::locale(
        std::locale::classic(), new alternative_time_get<CharT>('Y', std::numeric_limits<int>::max(), false)));
    year result{42};
    from_stream(stream, ST("%EY").c_str(), result);
    assert(stream.fail());
    assert(result == year{42});
  }

  // %EC retains its numeric fallback; %Ez uses the dedicated offset parser.
  {
    std::basic_istringstream<CharT> stream(ST("20 26-07-20 +4:30"));
    stream.imbue(std::locale(std::locale::classic(), new alternative_time_get<CharT>('?', 0, true)));
    sys_seconds result{};
    from_stream(stream, ST("%EC %y-%m-%d %Ez").c_str(), result);
    assert(!stream.fail());
    assert(result == date - 4h - 30min);
  }

  // The built-in facet accepts numeric years in the classic locale.
  for (const auto& format : {ST("%EC %Ey-%m-%d"), ST("%EC %EY-%m-%d")}) {
    std::basic_istringstream<CharT> stream(format == ST("%EC %Ey-%m-%d") ? ST("20 26-07-20") : ST("20 2026-07-20"));
    stream.imbue(std::locale::classic());
    sys_seconds result{};
    from_stream(stream, format.c_str(), result);
    assert(!stream.fail());
    assert(result == date);
  }
}

int main(int, char**) {
  test<char>();
#ifndef TEST_HAS_NO_WIDE_CHARACTERS
  test<wchar_t>();
#endif
  return 0;
}

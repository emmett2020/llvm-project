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
// from_stream: O modifiers

#include <cassert>
#include <chrono>
#include <ctime>
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
         std::tm* tm,
         char spec,
         char modifier) const override {
    if (modifier == 0)
      return Base::do_get(first, last, ios, err, tm, spec, modifier);

    assert(modifier == 'O');
    assert(spec == spec_);
    if (first == last) {
      err |= std::ios_base::failbit | std::ios_base::eofbit;
      return first;
    }
    if (*first != CharT('@')) {
      err |= std::ios_base::failbit;
      return first;
    }
    ++first;

    switch (spec) {
    case 'd':
    case 'e':
      tm->tm_mday = value_;
      break;
    case 'm':
      tm->tm_mon = value_;
      break;
    case 'H':
    case 'I':
      tm->tm_hour = value_;
      break;
    case 'M':
      tm->tm_min = value_;
      break;
    case 'w':
      tm->tm_wday = value_;
      break;
    case 'y':
      tm->tm_year = value_;
      break;
    default:
      assert(false);
    }

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
    int value;
    sys_seconds expected;
  };
  const TestCase cases[] = {
      {ST("@-07-20"), ST("%Oy-%m-%d"), 'y', 126, date},
      {ST("19 @-07-20"), ST("%C %Oy-%m-%d"), 'y', 126, sys_days{1926y / July / 20}},
      {ST("@-07-20"), ST("%Oy-%m-%d"), 'y', 68, sys_days{2068y / July / 20}},
      {ST("@-07-20"), ST("%Oy-%m-%d"), 'y', 69, sys_days{1969y / July / 20}},
      {ST("2026-@-20"), ST("%Y-%Om-%d"), 'm', 6, date},
      {ST("2026-07-@"), ST("%Y-%m-%Od"), 'd', 20, date},
      {ST("2026-07-@"), ST("%Y-%m-%Oe"), 'e', 20, date},
      {ST("2026-07-20 @"), ST("%F %Ow"), 'w', 1, date},
      {ST("2026-07-20 @"), ST("%F %OH"), 'H', 13, date + 13h},
      {ST("2026-07-20 @ PM"), ST("%F %OI %p"), 'I', 1, date + 13h},
      {ST("2026-07-20 PM @"), ST("%F %p %OI"), 'I', 1, date + 13h},
      {ST("2026-07-20 @ AM"), ST("%F %OI %p"), 'I', 0, date},
      {ST("2026-07-20 @ PM"), ST("%F %OI %p"), 'I', 0, date + 12h},
      {ST("2026-07-20 @"), ST("%F %OM"), 'M', 45, date + 45min},
  };
  for (const auto& c : cases) {
    for (bool fail : {false, true}) {
      for (bool suffix : {false, true}) {
        // Check both EOF after a field and an unconsumed following character.
        std::basic_istringstream<CharT> stream(c.input + (suffix ? ST("!") : ST("")));
        stream.imbue(std::locale(std::locale::classic(), new alternative_time_get<CharT>(c.spec, c.value, fail)));
        const sys_seconds initial{42s};
        sys_seconds result = initial;
        from_stream(stream, c.format.c_str(), result);
        assert(stream.fail() == fail);
        assert(result == (fail ? initial : c.expected));
        if (!fail && suffix)
          assert(stream.peek() == CharT('!'));
        if (!fail && !suffix && c.input.back() == CharT('@'))
          assert(stream.eof());
      }
    }
  }

  // Exercise the built-in facet, including %OI at midnight and noon.
  for (const auto& input : {ST("26-07-20 13:45 1"), ST("26-07-20 13:45 1!")}) {
    std::basic_istringstream<CharT> stream(input);
    stream.imbue(std::locale::classic());
    sys_seconds result{};
    from_stream(stream, ST("%Oy-%Om-%Od %OH:%OM %Ow").c_str(), result);
    assert(!stream.fail());
    assert(result == date + 13h + 45min);
  }
  for (bool pm : {false, true}) {
    std::basic_istringstream<CharT> stream(ST("2026-07-20 12 ") + (pm ? ST("PM") : ST("AM")));
    stream.imbue(std::locale::classic());
    sys_seconds result{};
    from_stream(stream, ST("%F %OI %p").c_str(), result);
    assert(!stream.fail());
    assert(result == date + (pm ? 12h : 0h));
  }

  // A duration's leading sign still applies when its first field uses a facet.
  {
    std::basic_istringstream<CharT> stream(ST("-@:45"));
    stream.imbue(std::locale(std::locale::classic(), new alternative_time_get<CharT>('H', 13, false)));
    minutes result{};
    from_stream(stream, ST("%OH:%M").c_str(), result);
    assert(!stream.fail());
    assert(result == -(13h + 45min));
  }

  // %OS, %OU and %OW currently use their unmodified numeric parsers, and %Oz
  // uses its own offset parser. None of these should call time_get with O.
  {
    std::basic_istringstream<CharT> stream(ST("2026-07-20 13:45:30.125 +4:30"));
    stream.imbue(std::locale(std::locale::classic(), new alternative_time_get<CharT>('?', 0, true)));
    sys_time<milliseconds> result{};
    from_stream(stream, ST("%F %H:%M:%OS %Oz").c_str(), result);
    assert(!stream.fail());
    assert(result == date + 13h + 45min + 30s + 125ms - 4h - 30min);
  }
  for (const auto& format : {ST("%Y %OU %w"), ST("%Y %OW %w")}) {
    std::basic_istringstream<CharT> stream(ST("2026 29 1"));
    stream.imbue(std::locale(std::locale::classic(), new alternative_time_get<CharT>('?', 0, true)));
    sys_seconds result{};
    from_stream(stream, format.c_str(), result);
    assert(!stream.fail());
    assert(result == date);
  }

  // O is not a valid modifier for the ISO weekday or ISO week number.
  for (const auto& format : {ST("%F %Ou"), ST("%F %OV")}) {
    std::basic_istringstream<CharT> stream(ST("2026-07-20 1"));
    const sys_seconds initial{42s};
    sys_seconds result = initial;
    from_stream(stream, format.c_str(), result);
    assert(stream.fail());
    assert(result == initial);
  }
}

int main(int, char**) {
  test<char>();
#ifndef TEST_HAS_NO_WIDE_CHARACTERS
  test<wchar_t>();
#endif
  return 0;
}

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
// parse: string and pointer formats, optional outputs, constraints, and ADL.

#include <cassert>
#include <chrono>
#include <concepts>
#include <locale>
#include <sstream>
#include <string>
#include <utility>

#include "make_string.h"
#include "test_macros.h"

#define ST(S) MAKE_STRING(CharT, S)

template <class... Args>
concept CanParse = requires(Args&&... args) { std::chrono::parse(std::forward<Args>(args)...); };

namespace custom {
template <int Arity>
struct value {
  int count = 0;
};

template <class CharT, class Traits>
std::basic_istream<CharT, Traits>& from_stream(std::basic_istream<CharT, Traits>& is, const CharT* fmt, value<3>& out) {
  assert(fmt[0] == CharT('#'));
  return is >> out.count;
}

template <class CharT, class Traits, class Alloc>
std::basic_istream<CharT, Traits>&
from_stream(std::basic_istream<CharT, Traits>& is,
            const CharT* fmt,
            value<4>& out,
            std::basic_string<CharT, Traits, Alloc>* abbrev) {
  assert(fmt[0] == CharT('#'));
  assert(abbrev != nullptr);
  *abbrev = ST("custom");
  return is >> out.count;
}

template <class CharT, class Traits, class Alloc>
std::basic_istream<CharT, Traits>&
from_stream(std::basic_istream<CharT, Traits>& is,
            const CharT* fmt,
            value<5>& out,
            std::basic_string<CharT, Traits, Alloc>* abbrev,
            std::chrono::minutes* offset) {
  assert(fmt[0] == CharT('#'));
  assert(offset != nullptr);
  if (abbrev)
    *abbrev = ST("custom");
  *offset = std::chrono::minutes{90};
  return is >> out.count;
}

struct not_parsable {};
} // namespace custom

template <class CharT, class Format, int Arity>
void test_constraints() {
  using String  = std::basic_string<CharT>;
  using Value   = custom::value<Arity>;
  using Minutes = std::chrono::minutes;
  static_assert(CanParse<const Format&, Value&> == (Arity == 3));
  static_assert(CanParse<const Format&, Value&, String&> == (Arity == 4));
  static_assert(CanParse<const Format&, Value&, Minutes&> == (Arity == 5));
  static_assert(CanParse<const Format&, Value&, String&, Minutes&> == (Arity == 5));
}

template <class CharT, class Format>
void test_adl() {
  using String = std::basic_string<CharT>;
  test_constraints<CharT, Format, 3>();
  test_constraints<CharT, Format, 4>();
  test_constraints<CharT, Format, 5>();
  static_assert(!CanParse<const Format&, custom::not_parsable&>);
  static_assert(!CanParse<const Format&, const custom::value<3>&>);
  static_assert(!CanParse<const Format&, custom::value<3>>);

  const auto format_storage = ST("#");
  const Format fmt{format_storage.c_str()};
  {
    std::basic_istringstream<CharT> is(ST("7"));
    custom::value<3> result{};
    auto& returned = is >> std::chrono::parse(fmt, result);
    assert(&returned == &is);
    assert(!is.fail());
    assert(result.count == 7);
  }
  {
    std::basic_istringstream<CharT> is(ST("7"));
    custom::value<4> result{};
    String abbrev;
    is >> std::chrono::parse(fmt, result, abbrev);
    assert(!is.fail());
    assert(result.count == 7);
    assert(abbrev == ST("custom"));
  }
  {
    std::basic_istringstream<CharT> is(ST("7"));
    custom::value<5> result{};
    std::chrono::minutes offset{};
    is >> std::chrono::parse(fmt, result, offset);
    assert(!is.fail());
    assert(result.count == 7);
    assert(offset == std::chrono::minutes{90});
  }
  {
    std::basic_istringstream<CharT> is(ST("7"));
    custom::value<5> result{};
    String abbrev;
    std::chrono::minutes offset{};
    is >> std::chrono::parse(fmt, result, abbrev, offset);
    assert(!is.fail());
    assert(result.count == 7);
    assert(abbrev == ST("custom"));
    assert(offset == std::chrono::minutes{90});
  }
}

template <class CharT, class Format>
void test_chrono() {
  using namespace std::chrono;
  const sys_seconds date    = sys_days{2026y / July / 20};
  const auto format_storage = ST("%F %Z %z");
  const Format fmt{format_storage.c_str()};
  // Both format types and all four output combinations cover the eight overloads.
  for (int outputs = 0; outputs < 4; ++outputs) {
    std::basic_istringstream<CharT> is(ST("2026-07-20 UTC +0130!"));
    is.imbue(std::locale::classic());
    sys_seconds result{42s};
    std::basic_string<CharT> abbrev;
    minutes offset{};
    switch (outputs) {
    case 0: {
      static_assert(std::same_as<decltype(is >> parse(fmt, result)), std::basic_istream<CharT>&>);
      auto& returned = is >> parse(fmt, result);
      assert(&returned == &is);
      break;
    }
    case 1:
      is >> parse(fmt, result, abbrev);
      break;
    case 2:
      is >> parse(fmt, result, offset);
      break;
    case 3:
      is >> parse(fmt, result, abbrev, offset);
      break;
    }
    assert(!is.fail());
    assert(result == date - 90min);
    assert(is.peek() == CharT('!'));
    if (outputs & 1)
      assert(abbrev == ST("UTC"));
    if (outputs & 2)
      assert(offset == 90min);
  }

  // A parsing failure is visible on the original stream and leaves the target alone.
  std::basic_istringstream<CharT> is(ST("invalid"));
  is.imbue(std::locale::classic());
  sys_seconds result{42s};
  is >> parse(fmt, result);
  assert(is.fail());
  assert(result == sys_seconds{42s});
}

template <class CharT>
void test() {
  test_adl<CharT, const CharT*>();
  test_adl<CharT, std::basic_string<CharT>>();
  test_chrono<CharT, const CharT*>();
  test_chrono<CharT, std::basic_string<CharT>>();
}

int main(int, char**) {
  test<char>();
#ifndef TEST_HAS_NO_WIDE_CHARACTERS
  test<wchar_t>();
  static_assert(!CanParse<const char*, custom::value<4>&, std::wstring&>);
  static_assert(!CanParse<const wchar_t*, custom::value<4>&, std::string&>);
#endif
  return 0;
}

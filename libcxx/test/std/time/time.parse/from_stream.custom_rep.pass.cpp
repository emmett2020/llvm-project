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
// from_stream with user-defined duration representations.

#include <cassert>
#include <chrono>
#include <limits>
#include <locale>
#include <ratio>
#include <sstream>
#include <string>
#include <type_traits>

#include "make_string.h"
#include "test_macros.h"

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

template <class CharT, class Duration>
void check(const std::basic_string<CharT>& input, const std::basic_string<CharT>& format, Duration expected) {
  std::basic_istringstream<CharT> stream(input);
  stream.imbue(std::locale::classic());
  Duration result{};
  std::chrono::from_stream(stream, format.c_str(), result);
  assert(!stream.fail());
  assert(result == expected);
}

#define ST(S) MAKE_STRING(CharT, S)

template <class CharT>
void test() {
  using namespace std::chrono;
  using Int   = rep<long long>;
  using Float = rep<double>;

  check(ST("42"), ST("%S"), duration<Int>{42});
  check(ST("0"), ST("%S"), duration<Int>{0});
  check(ST("2 01:02:03"), ST("%j %T"), duration<Int>{176523});
  check(ST("01:30"), ST("%R"), duration<Int, std::ratio<60>>{90});
  check(ST("1.250"), ST("%S"), duration<Int, std::milli>{1250});
  check(ST("42.123456789"), ST("%S"), duration<Int, std::nano>{42123456789LL});

  // Combine before truncating: 1 second and 0.5 seconds together make one tick.
  check(ST("1.5"), ST("%S"), duration<Int, std::ratio<3, 2>>{1});
  check(ST("1.4"), ST("%S"), duration<Int, std::ratio<3, 2>>{0});
  check(ST("3.5"), ST("%S"), duration<Int, std::ratio<7, 2>>{1});
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

int main(int, char**) {
  test<char>();
#ifndef TEST_HAS_NO_WIDE_CHARACTERS
  test<wchar_t>();
#endif
  return 0;
}

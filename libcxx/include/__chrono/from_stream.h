// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___CHRONO_FROM_STREAM_H
#define _LIBCPP___CHRONO_FROM_STREAM_H

#include <__config>

#if _LIBCPP_HAS_LOCALIZATION

#  include <__chrono/calendar.h>
#  include <__chrono/day.h>
#  include <__chrono/duration.h>
#  include <__chrono/file_clock.h>
#  include <__chrono/gps_clock.h>
#  include <__chrono/hh_mm_ss.h>
#  include <__chrono/month.h>
#  include <__chrono/monthday.h>
#  include <__chrono/parser_data.h>
#  include <__chrono/statically_widen.h>
#  include <__chrono/system_clock.h>
#  include <__chrono/tai_clock.h>
#  include <__chrono/time_point.h>
#  include <__chrono/utc_clock.h>
#  include <__chrono/weekday.h>
#  include <__chrono/year.h>
#  include <__chrono/year_month.h>
#  include <__chrono/year_month_day.h>
#  include <__fwd/memory.h>
#  include <__fwd/string.h>
#  include <__iterator/istreambuf_iterator.h>
#  include <__locale>
#  include <__locale_dir/time.h>
#  include <cctype>
#  include <cstdint>
#  include <ctime>
#  include <istream>
#  include <limits>

#  if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#    pragma GCC system_header
#  endif

#  if _LIBCPP_STD_VER >= 20

_LIBCPP_BEGIN_NAMESPACE_STD

namespace chrono {

// Parsing records fields, normalizes alternative representations, and then
// converts the fields to the requested type. Errors are reported with failbit.

// Target-dependent parsing options.
struct __parse_options {
  int __fractional_width_ = 0;
  bool __is_duration_     = false;
};

template <class _Tp>
inline constexpr __parse_options __parse_options_v{};

template <class _Rep, class _Period>
inline constexpr __parse_options __parse_options_v<duration<_Rep, _Period> >{
    static_cast<int>(hh_mm_ss<duration<_Rep, _Period> >::fractional_width), /*__is_duration_=*/true};

template <class _Clock, class _Duration>
inline constexpr __parse_options __parse_options_v<time_point<_Clock, _Duration> >{
    __parse_options_v<_Duration>.__fractional_width_, /*__is_duration_=*/false};

_LIBCPP_HIDE_FROM_ABI constexpr int64_t __pow10(int __exp) {
  int64_t __result = 1;
  for (int __i = 0; __i < __exp; ++__i)
    __result *= 10;
  return __result;
}

template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI bool __peek(basic_istream<_CharT, _Traits>& __is, _CharT& __c) {
  if (!__is.good())
    return false;

  typename _Traits::int_type __i = __is.peek();
  if (_Traits::eq_int_type(__i, _Traits::eof()))
    return false;

  __c = _Traits::to_char_type(__i);
  return true;
}

struct __digits_result {
  uint64_t __value  = 0;
  int __digits_read = 0;
  bool __overflow   = false;
};

// Reads at most '__max' digits without exceeding '__limit'. After overflow,
// remaining digits are still consumed up to '__max'.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI __digits_result
__read_bounded_digits(basic_istream<_CharT, _Traits>& __is, int __max, uint64_t __limit) {
  uint64_t __result = 0;
  int __digits_read = 0;
  bool __overflow   = false;

  for (_CharT __c{}; __digits_read < __max && chrono::__peek(__is, __c); ++__digits_read) {
    if (__c < _CharT('0') || __c > _CharT('9'))
      break;
    __is.get();

    const uint64_t __digit = static_cast<uint64_t>(__c - _CharT('0'));
    if (!__overflow) {
      if (__result > __limit / 10 || (__result == __limit / 10 && __digit > __limit % 10))
        __overflow = true;
      else
        __result = __result * 10 + __digit;
    }
  }

  return {__result, __digits_read, __overflow};
}

// Reading no digits is not an error. Overflow sets failbit and leaves
// '__value' unchanged.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI int __read_digits(basic_istream<_CharT, _Traits>& __is, int __max, int& __value) {
  auto __result = chrono::__read_bounded_digits(__is, __max, (numeric_limits<int>::max)());

  if (__result.__overflow)
    __is.setstate(ios_base::failbit);
  else if (__result.__digits_read != 0)
    __value = static_cast<int>(__result.__value);

  return __result.__digits_read;
}

// Sets failbit if no digits are read.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI void __read_int(basic_istream<_CharT, _Traits>& __is, int __max, int& __value) {
  if (chrono::__read_digits(__is, __max, __value) == 0)
    __is.setstate(ios_base::failbit);
}

// Reads an optional sign followed by digits; the sign does not count towards '__max'.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI void __read_signed_int(basic_istream<_CharT, _Traits>& __is, int __max, int& __value) {
  bool __negative = false;
  if (_CharT __c{}; chrono::__peek(__is, __c) && (_Traits::eq(__c, _CharT('-')) || _Traits::eq(__c, _CharT('+')))) {
    __negative = _Traits::eq(__c, _CharT('-'));
    __is.get();
  }

  const uint64_t __positive_limit = static_cast<uint64_t>((numeric_limits<int>::max)());
  const uint64_t __negative_limit = __positive_limit + 1;
  const uint64_t __limit          = __negative ? __negative_limit : __positive_limit;

  auto __result = chrono::__read_bounded_digits(__is, __max, __limit);
  if (__result.__digits_read == 0 || __result.__overflow) {
    __is.setstate(ios_base::failbit);
    return;
  }

  if (!__negative)
    __value = static_cast<int>(__result.__value);
  else if (__result.__value == __negative_limit)
    __value = (numeric_limits<int>::min)();
  else
    __value = -static_cast<int>(__result.__value);
}

// Parses one locale-dependent conversion specifier with time_get.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI bool
__read_with_facet(basic_istream<_CharT, _Traits>& __is, tm& __tm, char __spec, char __modifier = 0) {
  using _Iter  = istreambuf_iterator<_CharT, _Traits>;
  using _Facet = time_get<_CharT, _Iter>;

  ios_base::iostate __err = ios_base::goodbit;
  const _Facet& __tf      = std::use_facet<_Facet>(__is.getloc());
  __tf.get(_Iter(__is), _Iter(), __is, __err, std::addressof(__tm), __spec, __modifier);

  // Propagate the facet's state (eofbit alone does not make the stream fail()).
  if (__err != ios_base::goodbit)
    __is.setstate(__err);

  return !(__err & ios_base::failbit);
}

// Parses a locale-dependent month name into a one-based month.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI void __read_month_name(basic_istream<_CharT, _Traits>& __is, int& __value) {
  tm __tm{};
  if (chrono::__read_with_facet(__is, __tm, 'b'))
    __value = __tm.tm_mon + 1; // tm_mon is 0-based [0, 11].
}

// Parses a locale-dependent weekday name using the chrono::weekday convention.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI void __read_weekday_name(basic_istream<_CharT, _Traits>& __is, int& __value) {
  tm __tm{};
  if (chrono::__read_with_facet(__is, __tm, 'a'))
    __value = __tm.tm_wday; // tm_wday is already [0, 6], Sunday == 0.
}

// time_get reports %p through tm_hour. Starting at zero leaves AM as 0
// and changes PM to 12.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI void __read_am_pm(basic_istream<_CharT, _Traits>& __is, bool& __is_pm) {
  tm __tm{};
  __tm.tm_hour = 0;
  if (chrono::__read_with_facet(__is, __tm, 'p'))
    __is_pm = __tm.tm_hour == 12;
}

// Parses %S and its optional fractional part. '__width' includes the decimal point.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI void
__read_seconds(basic_istream<_CharT, _Traits>& __is, int __width, int __fractional_width, __fields_storage& __f) {
  int __seconds     = 0;
  int __digits_read = chrono::__read_digits(__is, __width, __seconds);
  if (__digits_read == 0 || __is.fail()) {
    if (__digits_read == 0)
      __is.setstate(ios_base::failbit);
    return;
  }

  __f.__seconds_ = __seconds;
  __f.__set(__fields_set::__seconds);

  // A fractional part needs a decimal point and at least one digit.
  int __remaining = __width - __digits_read;

  // Do not consume a fractional part when the target has no subsecond
  // precision or the field width cannot hold a decimal point and one digit.
  if (__fractional_width == 0 || __remaining < 2)
    return;

  _CharT __c{};
  if (!chrono::__peek(__is, __c) || !_Traits::eq(__c, std::use_facet<numpunct<_CharT> >(__is.getloc()).decimal_point()))
    return;
  __is.get();
  --__remaining;

  auto __fraction = chrono::__read_bounded_digits(
      __is, __remaining < __fractional_width ? __remaining : __fractional_width, (numeric_limits<int64_t>::max)());
  if (__fraction.__digits_read == 0 || __fraction.__overflow) {
    __is.setstate(ios_base::failbit);
    return;
  }

  // The fraction is stored scaled to attoseconds, so that the conversion to the
  // target's precision does not depend on the number of digits that were read.
  __f.__subseconds_ = static_cast<int64_t>(__fraction.__value) * chrono::__pow10(18 - __fraction.__digits_read);
}

// Parses %z as [+|-]hh[mm], and %Ez/%Oz as [+|-]h[h][:mm].
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI void __read_utc_offset(basic_istream<_CharT, _Traits>& __is, bool __is_modified, int& __value) {
  _CharT __c{};
  if (!chrono::__peek(__is, __c) || (!_Traits::eq(__c, _CharT('+')) && !_Traits::eq(__c, _CharT('-')))) {
    __is.setstate(ios_base::failbit);
    return;
  }
  int __sign = _Traits::eq(__c, _CharT('-')) ? -1 : 1;
  __is.get();

  int __hours       = 0;
  int __digits_read = chrono::__read_digits(__is, 2, __hours);

  // %z requires exactly two hour digits, while %Ez and %Oz allow one or two.
  if (__digits_read == 0 || (!__is_modified && __digits_read != 2)) {
    __is.setstate(ios_base::failbit);
    return;
  }

  // The minutes are optional in both forms, but the modified form requires the
  // colon before them, and a colon requires the minutes to follow.
  int __minutes = 0;
  bool __has_minutes{};
  if (__is_modified) {
    __has_minutes = chrono::__peek(__is, __c) && _Traits::eq(__c, _CharT(':'));
    if (__has_minutes)
      __is.get();
  } else {
    __has_minutes = chrono::__peek(__is, __c) && __c >= _CharT('0') && __c <= _CharT('9');
  }

  if (__has_minutes && (chrono::__read_digits(__is, 2, __minutes) != 2 || __minutes > 59)) {
    __is.setstate(ios_base::failbit);
    return;
  }

  __value = __sign * (__hours * 60 + __minutes);
}

// Parses a nonempty %Z token containing alphanumerics or '_', '/', '-', and '+'.
template <class _CharT, class _Traits, class _Alloc>
_LIBCPP_HIDE_FROM_ABI void
__read_time_zone_abbrev(basic_istream<_CharT, _Traits>& __is, basic_string<_CharT, _Traits, _Alloc>* __abbrev) {
  const auto& __ctype = std::use_facet<ctype<_CharT> >(__is.getloc());

  int __count = 0;
  for (_CharT __c{}; chrono::__peek(__is, __c); ++__count) {
    char __narrow = __ctype.narrow(__c, '\0');
    if (!std::isdigit(static_cast<unsigned char>(__narrow)) && !('a' <= __narrow && __narrow <= 'z') &&
        !('A' <= __narrow && __narrow <= 'Z') && __narrow != '_' && __narrow != '/' && __narrow != '-' &&
        __narrow != '+')
      break;

    __is.get();
    if (__abbrev) {
      // The word replaces whatever the string held, but only once it is known
      // there is a word: a failed %Z leaves the string alone.
      if (__count == 0)
        __abbrev->clear();
      __abbrev->push_back(__c);
    }
  }

  if (__count == 0)
    __is.setstate(ios_base::failbit);
}

_LIBCPP_HIDE_FROM_ABI constexpr bool __width_allowed(char __spec) {
  switch (__spec) {
  case 'C':
  case 'd':
  case 'e':
  case 'g':
  case 'G':
  case 'H':
  case 'I':
  case 'j':
  case 'm':
  case 'M':
  case 'S':
  case 'u':
  case 'U':
  case 'V':
  case 'w':
  case 'W':
  case 'y':
  case 'Y':
    return true;
  default:
    return false;
  }
}

// Returns whether [time.parse] permits '__modifier' for '__spec'.
_LIBCPP_HIDE_FROM_ABI constexpr bool __modifier_allowed(char __modifier, char __spec) {
  if (__modifier == 'E')
    switch (__spec) {
    case 'c':
    case 'C':
    case 'x':
    case 'X':
    case 'y':
    case 'Y':
    case 'z':
      return true;
    default:
      return false;
    }

  switch (__spec) {
  case 'd':
  case 'e':
  case 'H':
  case 'I':
  case 'm':
  case 'M':
  case 'S':
  case 'u':
  case 'U':
  case 'V':
  case 'w':
  case 'W':
  case 'y':
  case 'z':
    return true;
  default:
    return false;
  }
}

// Parses '__fmt' into '__f', setting failbit on a mismatch.
template <class _CharT, class _Traits, class _Alloc>
_LIBCPP_HIDE_FROM_ABI void __parse_from_stream(
    basic_istream<_CharT, _Traits>& __is,
    const _CharT* __fmt,
    __fields_storage& __f,
    basic_string<_CharT, _Traits, _Alloc>* __abbrev,
    minutes* __offset,
    __parse_options __options) {
  const auto& __ctype = std::use_facet<ctype<_CharT> >(__is.getloc());

  auto __skip_whitespace = [&] {
    for (_CharT __c{}; chrono::__peek(__is, __c) && __ctype.is(ctype_base::space, __c);)
      __is.get();
  };

  auto __skip_one_whitespace = [&] {
    _CharT __c{};
    if (!chrono::__peek(__is, __c) || !__ctype.is(ctype_base::space, __c))
      return false;
    __is.get();
    return true;
  };

  auto __match = [&](_CharT __expected) {
    _CharT __c{};
    if (!__is.get(__c) || !_Traits::eq(__c, __expected))
      __is.setstate(ios_base::failbit);
  };

  // A duration may be negative, and is written with a single minus sign in
  // front of the whole value rather than one per field ("{:%T}" of -90min is
  // "-01:30:00"). The sign is therefore accepted in front of the first field
  // that is read, wherever the format puts it, and negates the result.
  auto __read_minus_sign = [&] {
    if (!__options.__is_duration_ || __f.__present_ != __fields_set::__none)
      return;

    if (_CharT __c{}; chrono::__peek(__is, __c) && _Traits::eq(__c, _CharT('-'))) {
      __is.get();
      __f.__negative_ = true;
    }
  };

  int __width      = 0;
  bool __has_width = false;

  auto __read = [&](int __default_width, auto& __field, __fields_set __part) {
    __read_minus_sign();
    chrono::__read_int(__is, __has_width ? __width : __default_width, __field);
    if (!__is.fail())
      __f.__set(__part);
  };

  auto __read_signed = [&](int __default_width, auto& __field, __fields_set __part) {
    chrono::__read_signed_int(__is, __has_width ? __width : __default_width, __field);
    if (!__is.fail())
      __f.__set(__part);
  };

  auto __read_locale_format = [&](char __spec, char __modifier, bool __date, bool __time) {
    tm __tm{};
    if (!chrono::__read_with_facet(__is, __tm, __spec, __modifier))
      return;

    if (__date) {
      __f.__year_  = __tm.tm_year + 1900;
      __f.__month_ = __tm.tm_mon + 1;
      __f.__day_   = __tm.tm_mday;
      __f.__set(__fields_set::__year | __fields_set::__month | __fields_set::__day);
    }
    if (__time) {
      __f.__hours_   = __tm.tm_hour;
      __f.__minutes_ = __tm.tm_min;
      __f.__seconds_ = __tm.tm_sec;
      __f.__set(__fields_set::__hours | __fields_set::__minutes | __fields_set::__seconds);
    }
  };

  while (*__fmt != _CharT('\0')) {
    if (__is.fail())
      return;

    if (__ctype.is(ctype_base::space, *__fmt)) {
      __skip_whitespace();
      ++__fmt;
      continue;
    }

    if (*__fmt != _CharT('%')) {
      __match(*__fmt);
      ++__fmt;
      continue;
    }

    ++__fmt;

    __has_width = false;
    __width     = 0;
    while (_CharT('0') <= *__fmt && *__fmt <= _CharT('9')) {
      __has_width       = true;
      const int __digit = static_cast<int>(*__fmt - _CharT('0'));
      if (__width > ((numeric_limits<int>::max)() - __digit) / 10) {
        __is.setstate(ios_base::failbit);
        return;
      }
      __width = __width * 10 + __digit;
      ++__fmt;
    }

    char __modifier = 0;
    if (*__fmt == _CharT('E') || *__fmt == _CharT('O')) {
      __modifier = static_cast<char>(*__fmt == _CharT('E') ? 'E' : 'O');
      ++__fmt;
    }

    char __spec = __ctype.narrow(*__fmt, '\0');

    if ((__has_width && !chrono::__width_allowed(__spec)) ||
        (__modifier != 0 && !chrono::__modifier_allowed(__modifier, __spec))) {
      __is.setstate(ios_base::failbit);
      return;
    }

    switch (__spec) {
    case '%':
      __match(_CharT('%'));
      break;
    case 'n':
      // %n matches exactly one white space character, %t at most one. Combining
      // them and a literal space matches a range, e.g. "%n%t%t" matches one to
      // three white space characters.
      if (!__skip_one_whitespace())
        __is.setstate(ios_base::failbit);
      break;
    case 't':
      __skip_one_whitespace();
      break;

    case 'b':
    case 'B':
    case 'h':
      chrono::__read_month_name(__is, __f.__month_);
      if (!__is.fail())
        __f.__set(__fields_set::__month);
      break;
    case 'a':
    case 'A':
      chrono::__read_weekday_name(__is, __f.__weekday_);
      if (!__is.fail())
        __f.__set(__fields_set::__weekday);
      break;
    case 'p':
      chrono::__read_am_pm(__is, __f.__is_pm_);
      if (!__is.fail())
        __f.__set(__fields_set::__am_pm);
      break;

    case 'c':
      __read_locale_format('c', __modifier, /*__date=*/true, /*__time=*/true);
      break;
    case 'x':
      __read_locale_format('x', __modifier, /*__date=*/true, /*__time=*/false);
      break;
    case 'X':
      __read_locale_format('X', __modifier, /*__date=*/false, /*__time=*/true);
      break;
    case 'r':
      __read_locale_format('r', __modifier, /*__date=*/false, /*__time=*/true);
      break;

    case 'C':
      __read_signed(2, __f.__century_, __fields_set::__century);
      break;
    case 'y':
      __read(2, __f.__year_of_century_, __fields_set::__year_of_century);
      if (!__is.fail() && (__f.__year_of_century_ < 0 || __f.__year_of_century_ > 99))
        __is.setstate(ios_base::failbit);
      break;
    case 'Y':
      __read_signed(4, __f.__year_, __fields_set::__year);
      break;
    case 'G':
      __read_signed(4, __f.__iso_year_, __fields_set::__iso_year);
      break;
    case 'g': {
      int __yy = 0;
      chrono::__read_int(__is, __has_width ? __width : 2, __yy);
      if (!__is.fail()) {
        if (__yy < 0 || __yy > 99)
          __is.setstate(ios_base::failbit);
        else {
          __f.__iso_year_ = __yy <= 68 ? 2000 + __yy : 1900 + __yy;
          __f.__set(__fields_set::__iso_year);
        }
      }
      break;
    }

    case 'm':
      __read(2, __f.__month_, __fields_set::__month);
      break;
    case 'e':
      // %e is the space padded day of month, so the digits may be preceded by a
      // space; %d is the zero padded form.
      if (_CharT __c{}; chrono::__peek(__is, __c) && __ctype.is(ctype_base::space, __c))
        __is.get();
      [[fallthrough]];
    case 'd':
      __read(2, __f.__day_, __fields_set::__day);
      break;
    case 'j':
      // The day of the year for a calendar type; a plain number of days when
      // the target is a duration, in which case it is not limited to [1, 366].
      __read(3, __f.__day_of_year_, __fields_set::__day_of_year);
      break;
    case 'U':
      __read(2, __f.__week_sun_, __fields_set::__week_sun);
      break;
    case 'W':
      __read(2, __f.__week_mon_, __fields_set::__week_mon);
      break;
    case 'V':
      __read(2, __f.__iso_week_, __fields_set::__iso_week);
      break;
    case 'u': {
      int __wd = 0;
      chrono::__read_int(__is, __has_width ? __width : 1, __wd);
      if (!__is.fail()) {
        if (__wd < 1 || __wd > 7)
          __is.setstate(ios_base::failbit);
        else {
          __f.__weekday_ = __wd % 7;
          __f.__set(__fields_set::__weekday);
        }
      }
      break;
    }
    case 'w': {
      int __wd = 0;
      chrono::__read_int(__is, __has_width ? __width : 1, __wd);
      if (!__is.fail()) {
        if (__wd < 0 || __wd > 6)
          __is.setstate(ios_base::failbit);
        else {
          __f.__weekday_ = __wd;
          __f.__set(__fields_set::__weekday);
        }
      }
      break;
    }

    case 'H':
      __read(2, __f.__hours_, __fields_set::__hours);
      break;
    case 'I':
      __read(2, __f.__hour12_, __fields_set::__hour12);
      if (!__is.fail() && (__f.__hour12_ < 1 || __f.__hour12_ > 12))
        __is.setstate(ios_base::failbit);
      break;
    case 'M':
      __read(2, __f.__minutes_, __fields_set::__minutes);
      break;
    case 'S': {
      // Without an explicit width the field is two digits, plus the decimal
      // point and the fractional digits the target can represent.
      int __fractional_width = __options.__fractional_width_;
      int __default_width    = __fractional_width == 0 ? 2 : 3 + __fractional_width;
      __read_minus_sign();
      chrono::__read_seconds(__is, __has_width ? __width : __default_width, __fractional_width, __f);
      break;
    }

    case 'z':
      chrono::__read_utc_offset(__is, __modifier != 0, __f.__utc_offset_);
      if (!__is.fail()) {
        __f.__set(__fields_set::__utc_offset);
        if (__offset)
          *__offset = minutes{__f.__utc_offset_};
      }
      break;
    case 'Z':
      chrono::__read_time_zone_abbrev(__is, __abbrev);
      break;

    case 'D':
      chrono::__parse_from_stream(
          __is, _LIBCPP_STATICALLY_WIDEN(_CharT, "%m/%d/%y"), __f, __abbrev, __offset, __options);
      break;
    case 'F':
      chrono::__parse_from_stream(
          __is, _LIBCPP_STATICALLY_WIDEN(_CharT, "%Y-%m-%d"), __f, __abbrev, __offset, __options);
      break;
    case 'T':
      chrono::__parse_from_stream(
          __is, _LIBCPP_STATICALLY_WIDEN(_CharT, "%H:%M:%S"), __f, __abbrev, __offset, __options);
      break;
    case 'R':
      chrono::__parse_from_stream(__is, _LIBCPP_STATICALLY_WIDEN(_CharT, "%H:%M"), __f, __abbrev, __offset, __options);
      break;

    default:
      __is.setstate(ios_base::failbit);
      return;
    }
    ++__fmt;
  }
}

// Normalizes %C/%y and %I/%p, rejecting inconsistent fields.
_LIBCPP_HIDE_FROM_ABI inline bool __finalize(__fields_storage& __f) {
  // %y is the year without its century. With %C the two are concatenated;
  // without it, [69, 99] refers to 1969-1999 and [00, 68] to 2000-2068.
  if (__f.__has(__fields_set::__year_of_century)) {
    int __year{};
    if (__f.__has(__fields_set::__century)) {
      if (__f.__century_ < (numeric_limits<int>::min)() / 100 ||
          __f.__century_ > ((numeric_limits<int>::max)() - __f.__year_of_century_) / 100)
        return false;
      __year = __f.__century_ * 100 + __f.__year_of_century_;
    } else
      __year = __f.__year_of_century_ <= 68 ? 2000 + __f.__year_of_century_ : 1900 + __f.__year_of_century_;

    if (__f.__has(__fields_set::__year) && __f.__year_ != __year)
      return false;

    __f.__year_ = __year;
    __f.__set(__fields_set::__year);
  } else if (__f.__has(__fields_set::__century | __fields_set::__year) && __f.__year_ / 100 != __f.__century_)
    return false;

  // %I is the hour on the 12-hour clock, which %p disambiguates. Without %p the
  // hour is taken as it was written.
  if (__f.__has(__fields_set::__hour12)) {
    int __hours = __f.__hour12_;
    if (__f.__has(__fields_set::__am_pm))
      __hours = __f.__hour12_ % 12 + (__f.__is_pm_ ? 12 : 0);

    if (__f.__has(__fields_set::__hours) && __f.__hours_ != __hours)
      return false;

    __f.__hours_ = __hours;
    __f.__set(__fields_set::__hours);
  } else if (__f.__has(__fields_set::__am_pm | __fields_set::__hours) && (__f.__hours_ >= 12) != __f.__is_pm_)
    return false;

  return true;
}

// An absent field is considered in range.
_LIBCPP_HIDE_FROM_ABI constexpr bool
__in_range(const __fields_storage& __f, __fields_set __part, int __value, int __lo, int __hi) {
  return !__f.__has(__part) || (__lo <= __value && __value <= __hi);
}

_LIBCPP_HIDE_FROM_ABI constexpr bool __year_in_range(int __value) {
  return static_cast<int>(year::min()) <= __value && __value <= static_cast<int>(year::max());
}

// Converts an ISO week date to sys_days.
_LIBCPP_HIDE_FROM_ABI inline bool __iso_week_to_sys_days(int __g, int __v, weekday __wd, sys_days& __out) {
  if (!chrono::__year_in_range(__g) || __v < 1 || __v > 53)
    return false;

  // ISO week 1 is the week containing 4 January; start from that week's Monday.
  sys_days __jan4 = static_cast<sys_days>(year_month_day{year{__g}, month{1}, day{4}});
  weekday __jan4_wd{__jan4};
  sys_days __week1_mon = __jan4 - days{static_cast<int>(__jan4_wd.iso_encoding()) - 1};
  sys_days __result    = __week1_mon + weeks{__v - 1} + days{static_cast<int>(__wd.iso_encoding()) - 1};

  // Reject a nonexistent week: the Thursday of the result's week must fall in
  // the ISO year '__g'.
  sys_days __thursday = __result + days{4 - static_cast<int>(__wd.iso_encoding())};
  if (year_month_day{__thursday}.year() != year{__g})
    return false;

  __out = __result;
  return true;
}

// Converts a %U/%W week and weekday to sys_days, including week zero.
_LIBCPP_HIDE_FROM_ABI inline bool
__week_to_sys_days(int __year, int __week, weekday __first, weekday __wd, sys_days& __out) {
  if (!chrono::__year_in_range(__year) || __week < 0 || __week > 53)
    return false;

  sys_days __jan1{year{__year} / January / 1};
  sys_days __first_day = __jan1 + (__first - weekday{__jan1});
  sys_days __result    = __first_day + weeks{__week - 1} + (__wd - __first);
  if (year_month_day{__result}.year() != year{__year})
    return false;

  __out = __result;
  return true;
}

// Converts complete, consistent date fields to sys_days.
_LIBCPP_HIDE_FROM_ABI inline bool __to_sys_days(const __fields_storage& __f, sys_days& __out) {
  sys_days __date{};
  bool __have_date = false;

  // Records a candidate date, or reports that it contradicts an earlier one.
  auto __combine = [&](sys_days __candidate) {
    if (__have_date && __candidate != __date)
      return false;

    __date      = __candidate;
    __have_date = true;
    return true;
  };

  if (__f.__has(__fields_set::__year | __fields_set::__month | __fields_set::__day)) {
    if (!chrono::__year_in_range(__f.__year_) || __f.__month_ < 1 || __f.__month_ > 12 || __f.__day_ < 1 ||
        __f.__day_ > 31)
      return false;

    year_month_day __ymd{
        year{__f.__year_}, month{static_cast<unsigned>(__f.__month_)}, day{static_cast<unsigned>(__f.__day_)}};
    if (!__ymd.ok() || !__combine(static_cast<sys_days>(__ymd)))
      return false;
  }

  if (__f.__has(__fields_set::__iso_year | __fields_set::__iso_week | __fields_set::__weekday)) {
    sys_days __iso_date{};
    if (!chrono::__iso_week_to_sys_days(
            __f.__iso_year_, __f.__iso_week_, weekday{static_cast<unsigned>(__f.__weekday_)}, __iso_date) ||
        !__combine(__iso_date))
      return false;
  }

  if (__f.__has(__fields_set::__year | __fields_set::__day_of_year)) {
    if (!chrono::__year_in_range(__f.__year_) || __f.__day_of_year_ < 1 || __f.__day_of_year_ > 366)
      return false;

    sys_days __ordinal = sys_days{year{__f.__year_} / January / 1} + days{__f.__day_of_year_ - 1};
    // Catches day 366 of a common year.
    if (year_month_day{__ordinal}.year() != year{__f.__year_} || !__combine(__ordinal))
      return false;
  }

  if (__f.__has(__fields_set::__year | __fields_set::__week_sun | __fields_set::__weekday)) {
    sys_days __week_date{};
    if (!chrono::__week_to_sys_days(
            __f.__year_, __f.__week_sun_, Sunday, weekday{static_cast<unsigned>(__f.__weekday_)}, __week_date) ||
        !__combine(__week_date))
      return false;
  }

  if (__f.__has(__fields_set::__year | __fields_set::__week_mon | __fields_set::__weekday)) {
    sys_days __week_date{};
    if (!chrono::__week_to_sys_days(
            __f.__year_, __f.__week_mon_, Monday, weekday{static_cast<unsigned>(__f.__weekday_)}, __week_date) ||
        !__combine(__week_date))
      return false;
  }

  if (!__have_date)
    return false;

  // A weekday that was parsed next to a date is redundant, and must agree with
  // it: "2026-07-20 Tue" is not a date.
  if (__f.__has(__fields_set::__weekday) && weekday{__date} != weekday{static_cast<unsigned>(__f.__weekday_)})
    return false;

  __out = __date;
  return true;
}

template <class _Duration>
_LIBCPP_HIDE_FROM_ABI _Duration __to_time_of_day(const __fields_storage& __f) {
  _Duration __result =
      chrono::duration_cast<_Duration>(hours{__f.__hours_} + minutes{__f.__minutes_} + seconds{__f.__seconds_});

  // A target that cannot hold a fraction of a second never parses one, and
  // converting attoseconds to such a coarse period would overflow the ratio
  // arithmetic, so the conversion is not even instantiated.
  if constexpr (__parse_options_v<_Duration>.__fractional_width_ != 0)
    if (__f.__subseconds_ != 0)
      __result += chrono::duration_cast<_Duration>(duration<int64_t, atto>{__f.__subseconds_});

  return __result;
}

// Validates a clock time, allowing seconds through '__max_seconds'.
_LIBCPP_HIDE_FROM_ABI inline bool __time_of_day_ok(const __fields_storage& __f, int __max_seconds) {
  return chrono::__in_range(__f, __fields_set::__hours, __f.__hours_, 0, 23) &&
         chrono::__in_range(__f, __fields_set::__minutes, __f.__minutes_, 0, 59) &&
         chrono::__in_range(__f, __fields_set::__seconds, __f.__seconds_, 0, __max_seconds);
}

// Builders validate parsed fields and convert them to the requested type.

template <class _Rep, class _Period>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(const __fields_storage& __f, duration<_Rep, _Period>& __out) {
  // A duration is the sum of its components and is not a point in time: they
  // are not range checked, and %j is a number of days rather than the day of a
  // year. At least one of them is required.
  if (!__f.__has_any(
          __fields_set::__day_of_year | __fields_set::__hours | __fields_set::__minutes | __fields_set::__seconds))
    return false;

  using _Duration = duration<_Rep, _Period>;
  _Duration __result =
      chrono::duration_cast<_Duration>(days{__f.__day_of_year_}) + chrono::__to_time_of_day<_Duration>(__f);

  // The minus sign belongs to the value as a whole, not to one of its
  // components, which is also how the formatter writes it.
  __out = __f.__negative_ ? -__result : __result;
  return true;
}

template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(const __fields_storage& __f, sys_time<_Duration>& __out) {
  sys_days __date{};
  if (!chrono::__to_sys_days(__f, __date))
    return false;

  // Seconds are capped at 59 because sys_time (system_clock) is leap-second
  // oblivious; the utc_time builder allows 60.
  if (!chrono::__time_of_day_ok(__f, 59))
    return false;

  // %z gives the offset of the parsed time from UTC, so it is subtracted to
  // arrive at the UTC time sys_time holds. It is zero when %z was not used.
  // A target coarser than the parsed value (a sys_days parsed with "%F %T") is
  // rounded down, so that the day is the day that was written.
  __out = chrono::floor<_Duration>(__date + chrono::__to_time_of_day<_Duration>(__f) - minutes{__f.__utc_offset_});
  return true;
}

// A parsed UTC offset is not applied to local_time.
template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(const __fields_storage& __f, local_time<_Duration>& __out) {
  sys_days __date{};
  if (!chrono::__to_sys_days(__f, __date))
    return false;

  if (!chrono::__time_of_day_ok(__f, 59))
    return false;

  __out = chrono::floor<_Duration>(local_days{__date.time_since_epoch()} + chrono::__to_time_of_day<_Duration>(__f));
  return true;
}

template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(const __fields_storage& __f, file_time<_Duration>& __out) {
  sys_time<_Duration> __st{};
  if (!chrono::__from_fields(__f, __st))
    return false;

  __out = file_clock::from_sys(__st);
  return true;
}

#    if _LIBCPP_HAS_EXPERIMENTAL_TZDB
// utc_time permits a leap second.
template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(const __fields_storage& __f, utc_time<_Duration>& __out) {
  sys_days __date{};
  if (!chrono::__to_sys_days(__f, __date))
    return false;

  if (!chrono::__time_of_day_ok(__f, 60))
    return false;

  // Converting the date before adding the time of day keeps a 60th second
  // inside the leap second instead of overflowing the day.
  __out = chrono::floor<_Duration>(
      utc_clock::from_sys(__date) + chrono::__to_time_of_day<_Duration>(__f) - minutes{__f.__utc_offset_});
  return true;
}

template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(const __fields_storage& __f, tai_time<_Duration>& __out) {
  sys_days __date{};
  if (!chrono::__to_sys_days(__f, __date))
    return false;

  if (!chrono::__time_of_day_ok(__f, 59))
    return false;

  constexpr sys_days __tai_epoch{-days{4383}}; // 1958-01-01.
  __out = chrono::floor<_Duration>(
      tai_time<days>{__date - __tai_epoch} + chrono::__to_time_of_day<_Duration>(__f) - minutes{__f.__utc_offset_});
  return true;
}

template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(const __fields_storage& __f, gps_time<_Duration>& __out) {
  sys_days __date{};
  if (!chrono::__to_sys_days(__f, __date))
    return false;

  if (!chrono::__time_of_day_ok(__f, 59))
    return false;

  constexpr sys_days __gps_epoch{days{3657}}; // 1980-01-06.
  __out = chrono::floor<_Duration>(
      gps_time<days>{__date - __gps_epoch} + chrono::__to_time_of_day<_Duration>(__f) - minutes{__f.__utc_offset_});
  return true;
}
#    endif // _LIBCPP_HAS_EXPERIMENTAL_TZDB

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, day& __out) {
  if (!__f.__has(__fields_set::__day) || __f.__day_ < 1 || __f.__day_ > 31)
    return false;

  day __d{static_cast<unsigned>(__f.__day_)};
  if (!__d.ok())
    return false;

  __out = __d;
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, month& __out) {
  if (!__f.__has(__fields_set::__month) || __f.__month_ < 1 || __f.__month_ > 12)
    return false;

  month __m{static_cast<unsigned>(__f.__month_)};
  if (!__m.ok())
    return false;

  __out = __m;
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, year& __out) {
  if (!__f.__has(__fields_set::__year) || !chrono::__year_in_range(__f.__year_))
    return false;

  year __y{__f.__year_}; // year's constructor takes int; no cast needed.
  if (!__y.ok())
    return false;

  __out = __y;
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, weekday& __out) {
  if (!__f.__has(__fields_set::__weekday) || __f.__weekday_ < 0 || __f.__weekday_ > 6)
    return false;

  weekday __wd{static_cast<unsigned>(__f.__weekday_)}; // stored as [0, 6], Sunday == 0.
  if (!__wd.ok())
    return false;

  __out = __wd;
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, month_day& __out) {
  if (!__f.__has(__fields_set::__month | __fields_set::__day) || __f.__month_ < 1 || __f.__month_ > 12 ||
      __f.__day_ < 1 || __f.__day_ > 31)
    return false;

  month_day __md{month{static_cast<unsigned>(__f.__month_)}, day{static_cast<unsigned>(__f.__day_)}};
  if (!__md.ok())
    return false;

  __out = __md;
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, year_month& __out) {
  if (!__f.__has(__fields_set::__year | __fields_set::__month) || !chrono::__year_in_range(__f.__year_) ||
      __f.__month_ < 1 || __f.__month_ > 12)
    return false;

  year_month __ym{year{__f.__year_}, month{static_cast<unsigned>(__f.__month_)}};
  if (!__ym.ok())
    return false;

  __out = __ym;
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, year_month_day& __out) {
  // Uses the shared date logic, so every spelling of a date works here too. Any
  // time of day that was parsed is ignored.
  sys_days __date{};
  if (!chrono::__to_sys_days(__f, __date))
    return false;

  __out = year_month_day{__date};
  return true;
}

// Shared implementation for all from_stream overloads.
template <class _Tp, class _CharT, class _Traits, class _Alloc>
_LIBCPP_HIDE_FROM_ABI basic_istream<_CharT, _Traits>&
__from_stream(basic_istream<_CharT, _Traits>& __is,
              const _CharT* __fmt,
              _Tp& __value,
              basic_string<_CharT, _Traits, _Alloc>* __abbrev,
              minutes* __offset) {
  constexpr bool __noskipws = true;
  typename basic_istream<_CharT, _Traits>::sentry __s{__is, __noskipws};

  if (__s) {
    __fields_storage __f{};
    chrono::__parse_from_stream(__is, __fmt, __f, __abbrev, __offset, __parse_options_v<_Tp>);
    if (!__is.fail()) {
      _Tp __out{};
      if (chrono::__finalize(__f) && chrono::__from_fields(__f, __out))
        __value = __out;
      else
        __is.setstate(ios_base::failbit);
    }
  }

  return __is;
}

template <class _CharT, class _Traits, class _Rep, class _Period, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is,
            const _CharT* __fmt,
            duration<_Rep, _Period>& __d,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset                               = nullptr) {
  return chrono::__from_stream(__is, __fmt, __d, __abbrev, __offset);
}

template <class _CharT, class _Traits, class _Duration, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is,
            const _CharT* __fmt,
            sys_time<_Duration>& __tp,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset                               = nullptr) {
  return chrono::__from_stream(__is, __fmt, __tp, __abbrev, __offset);
}

template <class _CharT, class _Traits, class _Duration, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is,
            const _CharT* __fmt,
            local_time<_Duration>& __tp,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset                               = nullptr) {
  return chrono::__from_stream(__is, __fmt, __tp, __abbrev, __offset);
}

template <class _CharT, class _Traits, class _Duration, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is,
            const _CharT* __fmt,
            file_time<_Duration>& __tp,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset                               = nullptr) {
  return chrono::__from_stream(__is, __fmt, __tp, __abbrev, __offset);
}

#    if _LIBCPP_HAS_EXPERIMENTAL_TZDB
template <class _CharT, class _Traits, class _Duration, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is,
            const _CharT* __fmt,
            utc_time<_Duration>& __tp,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset                               = nullptr) {
  return chrono::__from_stream(__is, __fmt, __tp, __abbrev, __offset);
}

template <class _CharT, class _Traits, class _Duration, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is,
            const _CharT* __fmt,
            tai_time<_Duration>& __tp,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset                               = nullptr) {
  return chrono::__from_stream(__is, __fmt, __tp, __abbrev, __offset);
}

template <class _CharT, class _Traits, class _Duration, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is,
            const _CharT* __fmt,
            gps_time<_Duration>& __tp,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset                               = nullptr) {
  return chrono::__from_stream(__is, __fmt, __tp, __abbrev, __offset);
}
#    endif // _LIBCPP_HAS_EXPERIMENTAL_TZDB

template <class _CharT, class _Traits, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is,
            const _CharT* __fmt,
            day& __d,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset                               = nullptr) {
  return chrono::__from_stream(__is, __fmt, __d, __abbrev, __offset);
}

template <class _CharT, class _Traits, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is,
            const _CharT* __fmt,
            month& __m,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset                               = nullptr) {
  return chrono::__from_stream(__is, __fmt, __m, __abbrev, __offset);
}

template <class _CharT, class _Traits, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is,
            const _CharT* __fmt,
            year& __y,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset                               = nullptr) {
  return chrono::__from_stream(__is, __fmt, __y, __abbrev, __offset);
}

template <class _CharT, class _Traits, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is,
            const _CharT* __fmt,
            weekday& __wd,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset                               = nullptr) {
  return chrono::__from_stream(__is, __fmt, __wd, __abbrev, __offset);
}

template <class _CharT, class _Traits, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is,
            const _CharT* __fmt,
            month_day& __md,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset                               = nullptr) {
  return chrono::__from_stream(__is, __fmt, __md, __abbrev, __offset);
}

template <class _CharT, class _Traits, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is,
            const _CharT* __fmt,
            year_month& __ym,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset                               = nullptr) {
  return chrono::__from_stream(__is, __fmt, __ym, __abbrev, __offset);
}

template <class _CharT, class _Traits, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is,
            const _CharT* __fmt,
            year_month_day& __ymd,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset                               = nullptr) {
  return chrono::__from_stream(__is, __fmt, __ymd, __abbrev, __offset);
}

} // namespace chrono

_LIBCPP_END_NAMESPACE_STD

#  endif

#endif

#endif //_LIBCPP___CHRONO_FROM_STREAM_H

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
#  include <__type_traits/common_type.h>
#  include <__type_traits/is_floating_point.h>
#  include <__type_traits/is_integral.h>
#  include <__type_traits/make_unsigned.h>
#  include <cctype>
#  include <cstdint>
#  include <ctime>
#  include <istream>
#  include <limits>
#  include <string>

#  if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#    pragma GCC system_header
#  endif

#  if _LIBCPP_STD_VER >= 20

_LIBCPP_BEGIN_NAMESPACE_STD

namespace chrono {
// __fractional_width_ is the number of fractional digits read by %S.
struct __parse_options {
  unsigned __fractional_width_ = 0;
};

template <class _Tp>
inline constexpr __parse_options __parse_options_v{};

template <class _Rep, class _Period>
inline constexpr __parse_options __parse_options_v<duration<_Rep, _Period> >{
    hh_mm_ss<duration<_Rep, _Period> >::fractional_width};

template <class _Clock, class _Duration>
inline constexpr __parse_options __parse_options_v<time_point<_Clock, _Duration> >{
    __parse_options_v<_Duration>.__fractional_width_};

_LIBCPP_HIDE_FROM_ABI constexpr int64_t __pow10(unsigned __exp) {
  int64_t __result = 1;
  for (unsigned __i = 0; __i < __exp; ++__i)
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

struct __read_digits_result {
  uint64_t __value       = 0;
  unsigned __digits_read = 0;
  bool __overflow        = false;
};

// Extracts digits and reports their value, count, and overflow status.
// Stops after consuming the first digit that would overflow '__max_value'.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI __read_digits_result
__read_digits(basic_istream<_CharT, _Traits>& __is, unsigned __max_digits, uint64_t __max_value) {
  uint64_t __result      = 0;
  unsigned __digits_read = 0;

  for (_CharT __c{}; __digits_read < __max_digits && chrono::__peek(__is, __c);) {
    if (__c < _CharT('0') || __c > _CharT('9'))
      break;
    __is.get();
    ++__digits_read;

    const uint64_t __digit = static_cast<uint64_t>(__c - _CharT('0'));
    if (__result > __max_value / 10 || (__result == __max_value / 10 && __digit > __max_value % 10))
      return {__result, __digits_read, true};
    __result = __result * 10 + __digit;
  }

  return {__result, __digits_read, false};
}

// Reads an integer without a sign into int. Returns the number of digits read.
// Failure leaves '__value' unchanged.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI unsigned
__read_unsigned(basic_istream<_CharT, _Traits>& __is, unsigned __max_digits, int& __value) {
  auto __result = chrono::__read_digits(__is, __max_digits, (numeric_limits<int>::max)());

  if (__result.__digits_read == 0 || __result.__overflow)
    __is.setstate(ios_base::failbit);
  else
    __value = static_cast<int>(__result.__value);

  return __result.__digits_read;
}

// Reads an integer with an optional '+' or '-'. Failure leaves '__value' unchanged.
// The width includes an optional sign.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI void __read_signed(basic_istream<_CharT, _Traits>& __is, unsigned __max_digits, int& __value) {
  bool __negative = false;
  if (_CharT __c{}; __max_digits != 0 && chrono::__peek(__is, __c) &&
                    (_Traits::eq(__c, _CharT('-')) || _Traits::eq(__c, _CharT('+')))) {
    __negative = _Traits::eq(__c, _CharT('-'));
    __is.get();
    --__max_digits;
  }

  const uint64_t __positive_limit = static_cast<uint64_t>((numeric_limits<int>::max)());
  const uint64_t __negative_limit = __positive_limit + 1;
  const uint64_t __limit          = __negative ? __negative_limit : __positive_limit;

  auto __result = chrono::__read_digits(__is, __max_digits, __limit);
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
__read_with_time_get(basic_istream<_CharT, _Traits>& __is, tm& __tm, char __spec, char __modifier = 0) {
  using _Iter  = istreambuf_iterator<_CharT, _Traits>;
  using _Facet = time_get<_CharT, _Iter>;

  ios_base::iostate __err = ios_base::goodbit;
  const _Facet& __tf      = std::use_facet<_Facet>(__is.getloc());
  __tf.get(_Iter(__is), _Iter(), __is, __err, std::addressof(__tm), __spec, __modifier);

  // Propagate eofbit as well as parse failures. Reaching EOF after a
  // successful match is not itself a parsing failure.
  if (__err != ios_base::goodbit)
    __is.setstate(__err);

  return !(__err & ios_base::failbit);
}

// Parses a locale-dependent month name into a one-based month.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI void __read_month_name(basic_istream<_CharT, _Traits>& __is, int& __value) {
  tm __tm{};
  if (chrono::__read_with_time_get(__is, __tm, 'b')) {
    if (__tm.tm_mon == (numeric_limits<int>::max)())
      __is.setstate(ios_base::failbit);
    else
      __value = __tm.tm_mon + 1; // tm_mon is 0-based [0, 11].
  }
}

// Parses a locale-dependent weekday name using the chrono::weekday convention.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI void __read_weekday_name(basic_istream<_CharT, _Traits>& __is, int& __value) {
  tm __tm{};
  if (chrono::__read_with_time_get(__is, __tm, 'a'))
    __value = __tm.tm_wday; // tm_wday is already [0, 6], Sunday == 0.
}

// time_get reports %p through tm_hour. Starting at zero leaves AM as 0
// and changes PM to 12.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI void __read_am_pm(basic_istream<_CharT, _Traits>& __is, bool& __is_pm) {
  tm __tm{};
  __tm.tm_hour = 0;
  if (chrono::__read_with_time_get(__is, __tm, 'p'))
    __is_pm = __tm.tm_hour == 12;
}

// Parses %S and its optional fractional part. '__width' includes the decimal point.
template <class _CharT, class _Traits>
_LIBCPP_HIDE_FROM_ABI void __read_seconds(
    basic_istream<_CharT, _Traits>& __is, unsigned __width, unsigned __fractional_width, __fields_storage& __f) {
  int __seconds          = 0;
  unsigned __digits_read = chrono::__read_unsigned(__is, __width, __seconds);
  if (__is.fail())
    return;

  __f.__seconds_ = __seconds;

  // A fractional part needs a decimal point and at least one digit.
  unsigned __remaining = __width - __digits_read;

  // Do not consume a fractional part when the target has no subsecond
  // precision or the field width cannot hold a decimal point and one digit.
  if (__fractional_width == 0 || __remaining < 2)
    return;

  _CharT __c{};
  if (!chrono::__peek(__is, __c) || !_Traits::eq(__c, std::use_facet<numpunct<_CharT> >(__is.getloc()).decimal_point()))
    return;
  __is.get();
  --__remaining;

  auto __fraction = chrono::__read_digits(
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
  if (!chrono::__peek(__is, __c)) {
    __is.setstate(ios_base::failbit);
    return;
  }
  int __sign = _Traits::eq(__c, _CharT('-')) ? -1 : 1;
  if (_Traits::eq(__c, _CharT('+')) || _Traits::eq(__c, _CharT('-')))
    __is.get();

  int __hours            = 0;
  unsigned __digits_read = chrono::__read_unsigned(__is, 2, __hours);

  // %z requires exactly two hour digits, while %Ez and %Oz allow one or two.
  if (__is.fail() || (!__is_modified && __digits_read != 2)) {
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

  if (__has_minutes && chrono::__read_unsigned(__is, 2, __minutes) != 2) {
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
    if (!std::isdigit(static_cast<unsigned char>(__narrow)) && ('a' > __narrow || __narrow > 'z') &&
        ('A' > __narrow || __narrow > 'Z') && __narrow != '_' && __narrow != '/' && __narrow != '-' && __narrow != '+')
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
  case 'F':
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
  case 'U':
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

  unsigned __width = 0;
  bool __has_width = false;

  // Parses an O-modified field through time_get and converts its tm member.
  auto __read_alternative_field = [&](char __spec, int& __field) {
    tm __tm{};
    if (!chrono::__read_with_time_get(__is, __tm, __spec, 'O'))
      return;

    switch (__spec) {
    case 'd':
    case 'e':
      __field = __tm.tm_mday;
      break;
    case 'm':
      if (__tm.tm_mon == (numeric_limits<int>::max)())
        __is.setstate(ios_base::failbit);
      else
        __field = __tm.tm_mon + 1;
      break;
    case 'H':
      __field = __tm.tm_hour;
      break;
    case 'I':
      // Keep the 12-hour value separate from %p until the fields are combined.
      __field = __tm.tm_hour == 0 ? 12 : __tm.tm_hour;
      break;
    case 'M':
      __field = __tm.tm_min;
      break;
    case 'w':
      __field = __tm.tm_wday;
      break;
    case 'y': {
      // tm_year is relative to 1900; %Oy supplies only the last two digits.
      int64_t __year = static_cast<int64_t>(__tm.tm_year) + 1900;
      __field        = static_cast<int>((__year < 0 ? -__year : __year) % 100);
      break;
    }
    default:
      __is.setstate(ios_base::failbit);
      return;
    }
  };

  auto __assign_date = [&](const tm& __tm) {
    const int64_t __year = static_cast<int64_t>(__tm.tm_year) + 1900;
    if (__year > (numeric_limits<int>::max)() || __tm.tm_mon == (numeric_limits<int>::max)()) {
      __is.setstate(ios_base::failbit);
      return;
    }
    __f.__year_  = static_cast<int>(__year);
    __f.__month_ = __tm.tm_mon + 1;
    __f.__day_   = __tm.tm_mday;
    __f.__set(__fields_set::__year | __fields_set::__month | __fields_set::__day);
  };

  auto __assign_time = [&](const tm& __tm) {
    __f.__hours_   = __tm.tm_hour;
    __f.__minutes_ = __tm.tm_min;
    __f.__seconds_ = __tm.tm_sec;
    __f.__set(__fields_set::__hours | __fields_set::__minutes | __fields_set::__seconds);
  };

  while (*__fmt != _CharT('\0')) {
    if (__is.fail())
      return;

    if (__ctype.is(ctype_base::space, *__fmt)) {
      while (__skip_one_whitespace()) {
      }
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
      __has_width            = true;
      const unsigned __digit = static_cast<unsigned>(*__fmt - _CharT('0'));
      if (__width > ((numeric_limits<unsigned>::max)() - __digit) / 10) {
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
    case 'a':
    case 'A':
      chrono::__read_weekday_name(__is, __f.__weekday_);
      if (!__is.fail())
        __f.__set(__fields_set::__weekday);
      break;

    case 'b':
    case 'h':
    case 'B':
      chrono::__read_month_name(__is, __f.__month_);
      if (!__is.fail())
        __f.__set(__fields_set::__month);
      break;

    case 'c': {
      tm __tm{};
      if (chrono::__read_with_time_get(__is, __tm, __spec, __modifier)) {
        __assign_date(__tm);
        if (!__is.fail())
          __assign_time(__tm);
      }
      break;
    }

    case 'C':
      if (__modifier == 'E') {
        // TODO: Parse the locale's alternative century representation for %EC.
        // time_get does not support %C yet, so retain the numeric fallback.
        chrono::__read_signed(__is, 2, __f.__century_);
      } else {
        chrono::__read_signed(__is, __has_width ? __width : 2, __f.__century_);
      }
      if (!__is.fail())
        __f.__set(__fields_set::__century);
      break;

    case 'd':
    case 'e':
      if (__modifier == 'O')
        __read_alternative_field(__spec, __f.__day_);
      else
        chrono::__read_unsigned(__is, __has_width ? __width : 2, __f.__day_);
      if (!__is.fail())
        __f.__set(__fields_set::__day);
      break;

    case 'D':
      chrono::__parse_from_stream(
          __is, _LIBCPP_STATICALLY_WIDEN(_CharT, "%m/%d/%y"), __f, __abbrev, __offset, __options);
      break;

    case 'F':
      // A width on %F applies only to %Y.
      chrono::__read_signed(__is, __has_width ? __width : 4, __f.__year_);
      if (!__is.fail()) {
        __f.__set(__fields_set::__year);
        chrono::__parse_from_stream(
            __is, _LIBCPP_STATICALLY_WIDEN(_CharT, "-%m-%d"), __f, __abbrev, __offset, __options);
      }
      break;

    case 'g': {
      int __year_of_century = 0;
      chrono::__read_unsigned(__is, __has_width ? __width : 2, __year_of_century);
      if (!__is.fail()) {
        if (__year_of_century < 0 || __year_of_century > 99)
          __is.setstate(ios_base::failbit);
        else {
          __f.__iso_year_ = __year_of_century <= 68 ? 2000 + __year_of_century : 1900 + __year_of_century;
          __f.__set(__fields_set::__iso_year);
        }
      }
      break;
    }

    case 'G':
      chrono::__read_signed(__is, __has_width ? __width : 4, __f.__iso_year_);
      if (!__is.fail())
        __f.__set(__fields_set::__iso_year);
      break;

    case 'H':
      if (__modifier == 'O')
        __read_alternative_field(__spec, __f.__hours_);
      else
        chrono::__read_unsigned(__is, __has_width ? __width : 2, __f.__hours_);
      if (!__is.fail())
        __f.__set(__fields_set::__hours);
      break;

    case 'I':
      if (__modifier == 'O')
        __read_alternative_field(__spec, __f.__hour12_);
      else
        chrono::__read_unsigned(__is, __has_width ? __width : 2, __f.__hour12_);
      if (!__is.fail())
        __f.__set(__fields_set::__hour12);
      break;

    case 'j':
      // The day of the year for a calendar type; a plain number of days when
      // the target is a duration, in which case it is not limited to [1, 366].
      chrono::__read_unsigned(__is, __has_width ? __width : 3, __f.__day_of_year_);
      if (!__is.fail())
        __f.__set(__fields_set::__day_of_year);
      break;

    case 'm':
      if (__modifier == 'O')
        __read_alternative_field(__spec, __f.__month_);
      else
        chrono::__read_unsigned(__is, __has_width ? __width : 2, __f.__month_);
      if (!__is.fail())
        __f.__set(__fields_set::__month);
      break;

    case 'M':
      if (__modifier == 'O')
        __read_alternative_field(__spec, __f.__minutes_);
      else
        chrono::__read_unsigned(__is, __has_width ? __width : 2, __f.__minutes_);
      if (!__is.fail())
        __f.__set(__fields_set::__minutes);
      break;

    case 'p':
      chrono::__read_am_pm(__is, __f.__is_pm_);
      if (!__is.fail())
        __f.__set(__fields_set::__am_pm);
      break;

    case 'r': {
      tm __tm{};
      if (chrono::__read_with_time_get(__is, __tm, __spec, __modifier))
        __assign_time(__tm);
      break;
    }

    case 'R':
      chrono::__parse_from_stream(__is, _LIBCPP_STATICALLY_WIDEN(_CharT, "%H:%M"), __f, __abbrev, __offset, __options);
      break;

    case 'T':
      chrono::__parse_from_stream(
          __is, _LIBCPP_STATICALLY_WIDEN(_CharT, "%H:%M:%S"), __f, __abbrev, __offset, __options);
      break;

    case 'S': {
      // Without an explicit width the field is two digits, plus the decimal
      // point and the fractional digits the target can represent.
      unsigned __fractional_width = __options.__fractional_width_;
      unsigned __default_width    = __fractional_width == 0 ? 2 : 3 + __fractional_width;
      // TODO: Parse the locale's alternative seconds representation for %OS.
      chrono::__read_seconds(__is, __has_width ? __width : __default_width, __fractional_width, __f);
      if (!__is.fail())
        __f.__set(__fields_set::__seconds);
      break;
    }

    case 'u': {
      int __weekday = 0;
      chrono::__read_unsigned(__is, __has_width ? __width : 1, __weekday);
      if (!__is.fail()) {
        if (__weekday < 1 || __weekday > 7)
          __is.setstate(ios_base::failbit);
        else {
          __f.__weekday_ = __weekday % 7;
          __f.__set(__fields_set::__weekday);
        }
      }
      break;
    }

    case 'w': {
      if (__modifier == 'O')
        __read_alternative_field(__spec, __f.__weekday_);
      else
        chrono::__read_unsigned(__is, __has_width ? __width : 1, __f.__weekday_);
      if (!__is.fail())
        __f.__set(__fields_set::__weekday);
      break;
    }

    case 'U':
      // TODO: Parse the locale's alternative week number for %OU.
      chrono::__read_unsigned(__is, __has_width ? __width : 2, __f.__week_sun_);
      if (!__is.fail())
        __f.__set(__fields_set::__week_sun);
      break;

    case 'V':
      chrono::__read_unsigned(__is, __has_width ? __width : 2, __f.__iso_week_);
      if (!__is.fail())
        __f.__set(__fields_set::__iso_week);
      break;

    case 'W':
      // TODO: Parse the locale's alternative week number for %OW.
      chrono::__read_unsigned(__is, __has_width ? __width : 2, __f.__week_mon_);
      if (!__is.fail())
        __f.__set(__fields_set::__week_mon);
      break;

    case 'x': {
      tm __tm{};
      if (chrono::__read_with_time_get(__is, __tm, __spec, __modifier))
        __assign_date(__tm);
      break;
    }

    case 'X': {
      tm __tm{};
      if (chrono::__read_with_time_get(__is, __tm, __spec, __modifier))
        __assign_time(__tm);
      break;
    }

    case 'y':
      if (__modifier == 'O')
        __read_alternative_field(__spec, __f.__year_of_century_);
      else if (__modifier == 'E') {
        tm __tm{};
        if (chrono::__read_with_time_get(__is, __tm, __spec, __modifier)) {
          const int64_t __year   = static_cast<int64_t>(__tm.tm_year) + 1900;
          __f.__year_of_century_ = static_cast<int>((__year < 0 ? -__year : __year) % 100);
        }
      } else
        chrono::__read_unsigned(__is, __has_width ? __width : 2, __f.__year_of_century_);
      if (!__is.fail())
        __f.__set(__fields_set::__year_of_century);
      break;

    case 'Y':
      if (__modifier == 'E') {
        tm __tm{};
        if (chrono::__read_with_time_get(__is, __tm, __spec, __modifier)) {
          const int64_t __year = static_cast<int64_t>(__tm.tm_year) + 1900;
          if (__year > (numeric_limits<int>::max)())
            __is.setstate(ios_base::failbit);
          else
            __f.__year_ = static_cast<int>(__year);
        }
      } else
        chrono::__read_signed(__is, __has_width ? __width : 4, __f.__year_);
      if (!__is.fail())
        __f.__set(__fields_set::__year);
      break;

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
    case '%':
      __match(_CharT('%'));
      break;
    default:
      __is.setstate(ios_base::failbit);
      return;
    }
    ++__fmt;
  }
}

_LIBCPP_HIDE_FROM_ABI constexpr bool __year_in_range(int64_t __value) {
  return static_cast<int>(year::min()) <= __value && __value <= static_cast<int>(year::max());
}

// Obtains a valid year from %Y or %C/%y, checking conflicts before filling it in.
_LIBCPP_HIDE_FROM_ABI inline bool __try_get_year(__fields_storage& __f) {
  int64_t __year = __f.__year_;
  // %y is the year without its century. With %C the two are concatenated;
  // without it, [69, 99] refers to 1969-1999 and [00, 68] to 2000-2068.
  if (__f.__has(__fields_set::__year_of_century)) {
    if (__f.__year_of_century_ < 0 || __f.__year_of_century_ > 99)
      return false;
    if (__f.__has(__fields_set::__century)) {
      // %C uses floored division, while %y is the absolute last two digits.
      __year = static_cast<int64_t>(__f.__century_) * 100;
      if (__f.__century_ < 0 && __f.__year_of_century_ != 0)
        __year += 100 - __f.__year_of_century_;
      else
        __year += __f.__year_of_century_;
    } else {
      __year = (__f.__year_of_century_ <= 68 ? 2000 : 1900) + __f.__year_of_century_;
    }
    if (__f.__has(__fields_set::__year) && __f.__year_ != __year)
      return false;
  } else {
    if (!__f.__has(__fields_set::__year))
      return false;
    if (__f.__has(__fields_set::__century) && __f.__century_ != __year / 100 - (__year % 100 < 0))
      return false;
  }

  if (!chrono::__year_in_range(__year))
    return false;
  __f.__year_ = static_cast<int>(__year);
  __f.__set(__fields_set::__year);
  return true;
}

// Checks both a computed calendar year and the original %Y/%C/%y constraints.
_LIBCPP_HIDE_FROM_ABI inline bool __validate_year(const __fields_storage& __f, int64_t __year) {
  if (!chrono::__year_in_range(__year))
    return false;
  if (__f.__has(__fields_set::__year) && __f.__year_ != __year)
    return false;
  if (__f.__has(__fields_set::__century) && __f.__century_ != __year / 100 - (__year % 100 < 0))
    return false;
  if (__f.__has(__fields_set::__year_of_century) && __f.__year_of_century_ != (__year < 0 ? -__year : __year) % 100)
    return false;
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __validate_month(const __fields_storage& __f, int __month) {
  return 1 <= __month && __month <= 12 && (!__f.__has(__fields_set::__month) || __f.__month_ == __month);
}

_LIBCPP_HIDE_FROM_ABI inline bool __validate_day(const __fields_storage& __f, int __day) {
  return 1 <= __day && __day <= 31 && (!__f.__has(__fields_set::__day) || __f.__day_ == __day);
}

_LIBCPP_HIDE_FROM_ABI inline bool __validate_weekday(const __fields_storage& __f, int __weekday) {
  return 0 <= __weekday && __weekday <= 6 && (!__f.__has(__fields_set::__weekday) || __f.__weekday_ == __weekday);
}

// Computes a candidate hour without overwriting %H or adding a presence flag.
_LIBCPP_HIDE_FROM_ABI inline int __compute_hour(const __fields_storage& __f) {
  // %I is the hour on the 12-hour clock, which %p disambiguates. Without %p the
  // hour is taken as it was written.
  if (__f.__has(__fields_set::__hour12)) {
    if (__f.__has(__fields_set::__am_pm))
      return __f.__hour12_ % 12 + (__f.__is_pm_ ? 12 : 0);
    return __f.__hour12_;
  }
  return __f.__hours_;
}

// Durations allow hours beyond 23; clock times do not.
_LIBCPP_HIDE_FROM_ABI inline bool __validate_hour(const __fields_storage& __f, int __hour, int __max_hour) {
  if (__hour < 0 || __hour > __max_hour)
    return false;
  if (__f.__has(__fields_set::__hours) && __f.__hours_ != __hour)
    return false;
  if (__f.__has(__fields_set::__hour12)) {
    if (__f.__hour12_ < 1 || __f.__hour12_ > 12 || chrono::__compute_hour(__f) != __hour)
      return false;
  } else if (__f.__has(__fields_set::__am_pm | __fields_set::__hours) && (__hour >= 12) != __f.__is_pm_)
    return false;
  return true;
}

// An absent field is considered in range.
_LIBCPP_HIDE_FROM_ABI constexpr bool
__in_range(const __fields_storage& __f, __fields_set __part, int __value, int __lo, int __hi) {
  return !__f.__has(__part) || (__lo <= __value && __value <= __hi);
}

_LIBCPP_HIDE_FROM_ABI inline bool __validate_minute(const __fields_storage& __f, int __max_minute) {
  return chrono::__in_range(__f, __fields_set::__minutes, __f.__minutes_, 0, __max_minute);
}

_LIBCPP_HIDE_FROM_ABI inline bool __validate_second(const __fields_storage& __f, int __max_second) {
  return chrono::__in_range(__f, __fields_set::__seconds, __f.__seconds_, 0, __max_second) && __f.__subseconds_ >= 0;
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

_LIBCPP_HIDE_FROM_ABI inline bool __validate_date_fields(const __fields_storage& __f, sys_days __date) {
  // Check every supplied date field, including those that did not form a
  // complete candidate: "%Y %j %m" must not specify a contradictory month.
  auto __matches = [&](__fields_set __part, int __parsed, int __expected) {
    return !__f.__has(__part) || __parsed == __expected;
  };
  const year_month_day __ymd{__date};
  const int __year = static_cast<int>(__ymd.year());
  const weekday __weekday{__date};
  if (!chrono::__validate_year(__f, __year) || !chrono::__validate_month(__f, static_cast<unsigned>(__ymd.month())) ||
      !chrono::__validate_day(__f, static_cast<unsigned>(__ymd.day())) ||
      !chrono::__validate_weekday(__f, __weekday.c_encoding()))
    return false;

  const sys_days __jan1{__ymd.year() / January / 1};
  const int __day_of_year = static_cast<int>((__date - __jan1).count()) + 1;
  if (!__matches(__fields_set::__day_of_year, __f.__day_of_year_, __day_of_year) ||
      !__matches(__fields_set::__week_sun,
                 __f.__week_sun_,
                 (__day_of_year + 6 - static_cast<int>(__weekday.c_encoding())) / 7) ||
      !__matches(__fields_set::__week_mon,
                 __f.__week_mon_,
                 (__day_of_year + 7 - static_cast<int>(__weekday.iso_encoding())) / 7))
    return false;

  if (__f.__has_any(__fields_set::__iso_year | __fields_set::__iso_week)) {
    // The week's Thursday determines its ISO year. Keep that year as an int:
    // near year::min()/max(), it can fall outside chrono::year's valid range.
    const sys_days __thursday  = __date + days{4 - static_cast<int>(__weekday.iso_encoding())};
    const sys_days __next_jan1 = __jan1 + days{__ymd.year().is_leap() ? 366 : 365};
    int __iso_year             = __year;
    sys_days __iso_jan1        = __jan1;
    if (__thursday < __jan1) {
      --__iso_year;
      const bool __is_leap = __iso_year % 4 == 0 && (__iso_year % 100 != 0 || __iso_year % 400 == 0);
      __iso_jan1 -= days{__is_leap ? 366 : 365};
    } else if (__thursday >= __next_jan1) {
      ++__iso_year;
      __iso_jan1 = __next_jan1;
    }
    const int __iso_week = static_cast<int>((__thursday - __iso_jan1).count()) / 7 + 1;
    if (!__matches(__fields_set::__iso_year, __f.__iso_year_, __iso_year) ||
        !__matches(__fields_set::__iso_week, __f.__iso_week_, __iso_week))
      return false;
  }

  return true;
}

// Obtains one date from a complete representation, validates all supplied date
// fields against it, and fills in any missing calendar fields.
_LIBCPP_HIDE_FROM_ABI inline bool __try_get_date(__fields_storage& __f, sys_days& __out) {
  const bool __have_year = __f.__has_any(__fields_set::__year | __fields_set::__year_of_century);
  if (__have_year && !chrono::__try_get_year(__f))
    return false;

  sys_days __date{};
  if (__have_year && __f.__has(__fields_set::__month | __fields_set::__day)) {
    if (__f.__month_ < 1 || __f.__month_ > 12 || __f.__day_ < 1 || __f.__day_ > 31)
      return false;

    year_month_day __ymd{
        year{__f.__year_}, month{static_cast<unsigned>(__f.__month_)}, day{static_cast<unsigned>(__f.__day_)}};
    if (!__ymd.ok())
      return false;
    __date = static_cast<sys_days>(__ymd);
  } else if (__f.__has(__fields_set::__iso_year | __fields_set::__iso_week | __fields_set::__weekday)) {
    if (__f.__weekday_ < 0 || __f.__weekday_ > 6 ||
        !chrono::__iso_week_to_sys_days(
            __f.__iso_year_, __f.__iso_week_, weekday{static_cast<unsigned>(__f.__weekday_)}, __date))
      return false;
  } else if (__have_year && __f.__has(__fields_set::__day_of_year)) {
    if (__f.__day_of_year_ < 1 || __f.__day_of_year_ > 366)
      return false;

    __date = sys_days{year{__f.__year_} / January / 1} + days{__f.__day_of_year_ - 1};
    // Catches day 366 of a common year.
    if (year_month_day{__date}.year() != year{__f.__year_})
      return false;
  } else if (__have_year && __f.__has(__fields_set::__weekday) &&
             __f.__has_any(__fields_set::__week_sun | __fields_set::__week_mon)) {
    if (__f.__weekday_ < 0 || __f.__weekday_ > 6)
      return false;
    const bool __use_sunday = __f.__has(__fields_set::__week_sun);
    if (!chrono::__week_to_sys_days(
            __f.__year_,
            __use_sunday ? __f.__week_sun_ : __f.__week_mon_,
            __use_sunday ? Sunday : Monday,
            weekday{static_cast<unsigned>(__f.__weekday_)},
            __date))
      return false;
  } else {
    return false;
  }

  if (!chrono::__validate_date_fields(__f, __date))
    return false;

  const year_month_day __ymd{__date};
  __f.__year_  = static_cast<int>(__ymd.year());
  __f.__month_ = static_cast<unsigned>(__ymd.month());
  __f.__day_   = static_cast<unsigned>(__ymd.day());
  __f.__set(__fields_set::__year | __fields_set::__month | __fields_set::__day);
  __out = __date;
  return true;
}

template <class _Duration>
_LIBCPP_HIDE_FROM_ABI _Duration __to_time_of_day(const __fields_storage& __f, int __hour) {
  auto __result = chrono::duration_cast<_Duration>(hours{__hour} + minutes{__f.__minutes_} + seconds{__f.__seconds_});

  // A target that cannot hold a fraction of a second never parses one, and
  // converting attoseconds to such a coarse period would overflow the ratio
  // arithmetic, so the conversion is not even instantiated.
  if constexpr (__parse_options_v<_Duration>.__fractional_width_ != 0)
    if (__f.__subseconds_ != 0)
      __result += chrono::duration_cast<_Duration>(duration<int64_t, atto>{__f.__subseconds_});

  return __result;
}

// Validates a clock time, allowing seconds through '__max_seconds'.
_LIBCPP_HIDE_FROM_ABI inline bool __time_of_day_ok(const __fields_storage& __f, int __hour, int __max_seconds) {
  return chrono::__validate_hour(__f, __hour, 23) && chrono::__validate_minute(__f, 59) &&
         chrono::__validate_second(__f, __max_seconds);
}

// Builders validate parsed fields and convert them to the requested type.

// Computes value * multiplier / divisor and its remainder without overflowing
// the intermediate product. Split value into a multiple of divisor and a rest:
//
//   q = value / divisor, r = value % divisor
//   value = q * divisor + r
//   value * multiplier / divisor
//     = (q * divisor + r) * multiplier / divisor
//     = q * multiplier + r * multiplier / divisor
//
// All divisions are integer divisions. The first term becomes __base_quotient; the
// second becomes __quotient. Their sum is written to __result. The unscaled
// remainder r is stored in __input_remainder; the final __remainder is
// (r * multiplier) % divisor.
// This avoids forming value * multiplier, which may overflow even when the
// quotient fits. If __base_quotient overflows, the final quotient cannot fit either,
// since __quotient is nonnegative. If r * multiplier overflows, long division
// below computes __quotient and __remainder without forming that product.
// All inputs are nonnegative; divisor is positive and at most INTMAX_MAX, so
// doubling a remainder is representable in uint64_t.
template <class _UInt>
_LIBCPP_HIDE_FROM_ABI bool
__scale_duration(_UInt __value, uint64_t __multiplier, uint64_t __divisor, _UInt& __result, uint64_t& __remainder) {
  _UInt __base_quotient{};
  if (__builtin_mul_overflow(__value / __divisor, __multiplier, std::addressof(__base_quotient)))
    return false;

  const uint64_t __input_remainder = static_cast<uint64_t>(__value % __divisor);
  _UInt __product{};
  _UInt __quotient{};
  if (!__builtin_mul_overflow(static_cast<_UInt>(__input_remainder), __multiplier, std::addressof(__product))) {
    __quotient  = __product / __divisor;
    __remainder = static_cast<uint64_t>(__product % __divisor);
  } else {
    // Long division of the product, without constructing a double-width integer.
    __remainder = 0;
    for (unsigned __bit = 64; __bit != 0; --__bit) {
      __quotient *= 2;
      __remainder *= 2;
      if (__remainder >= __divisor) {
        __remainder -= __divisor;
        ++__quotient;
      }
      if ((__multiplier >> (__bit - 1)) & 1) {
        __remainder += __input_remainder;
        if (__remainder >= __divisor) {
          __remainder -= __divisor;
          ++__quotient;
        }
      }
    }
  }
  return !__builtin_add_overflow(__base_quotient, __quotient, std::addressof(__result));
}

template <class _Rep, class _Period>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(const __fields_storage& __f, duration<_Rep, _Period>& __out) {
  // Durations can represent only elapsed days and time-of-day fields.
  // UTC offsets and time zone abbreviations are auxiliary outputs.
  constexpr auto __allowed =
      __fields_set::__day_of_year | __fields_set::__hours | __fields_set::__hour12 | __fields_set::__am_pm |
      __fields_set::__minutes | __fields_set::__seconds | __fields_set::__utc_offset;
  if (!__f.__has_only(__allowed))
    return false;

  // A duration is the sum of its components, which are not restricted to
  // clock-time ranges. %j is a number of days rather than the day of a year.
  // At least one component is required.
  if (!__f.__has_any(__fields_set::__day_of_year | __fields_set::__hours | __fields_set::__hour12 |
                     __fields_set::__minutes | __fields_set::__seconds))
    return false;

  const int __hour              = chrono::__compute_hour(__f);
  constexpr int __max_component = (numeric_limits<int>::max)();
  if (__f.__day_of_year_ < 0 || !chrono::__validate_hour(__f, __hour, __max_component) ||
      !chrono::__validate_minute(__f, __max_component) || !chrono::__validate_second(__f, __max_component))
    return false;

  constexpr uint64_t __seconds_per_minute = 60;
  constexpr uint64_t __seconds_per_hour   = 60 * __seconds_per_minute;
  constexpr uint64_t __seconds_per_day    = 24 * __seconds_per_hour;

  // Every whole-number field fits in int, so their sum in seconds fits in uint64_t.
  const uint64_t __seconds =
      static_cast<uint64_t>(__f.__day_of_year_) * __seconds_per_day +
      static_cast<uint64_t>(__hour) * __seconds_per_hour +
      static_cast<uint64_t>(__f.__minutes_) * __seconds_per_minute + __f.__seconds_;
  if constexpr (is_integral_v<_Rep>) {
    using _UInt = make_unsigned_t<common_type_t<_Rep, uint64_t>>;
    _UInt __ticks{};
    uint64_t __remainder{};
    if (!chrono::__scale_duration(static_cast<_UInt>(__seconds), _Period::den, _Period::num, __ticks, __remainder))
      return false;

    _UInt __fraction{};
    uint64_t __fraction_remainder{};
    if (!chrono::__scale_duration(
            static_cast<_UInt>(__f.__subseconds_),
            _Period::den,
            1000000000000000000ULL,
            __fraction,
            __fraction_remainder))
      return false;
    // Combine before truncating, including the fractional tick left by whole seconds.
    const _UInt __extra = (__remainder + __fraction) / _Period::num;
    if (__builtin_add_overflow(__ticks, __extra, std::addressof(__ticks)))
      return false;

    if (__ticks > static_cast<_UInt>((numeric_limits<_Rep>::max)()))
      return false;

    __out = duration<_Rep, _Period>{static_cast<_Rep>(__ticks)};
  } else if constexpr (is_floating_point_v<_Rep>) {
    long double __ticks =
        (static_cast<long double>(__seconds) + static_cast<long double>(__f.__subseconds_) / 1000000000000000000.0L) *
        _Period::den / _Period::num;
    if (__ticks < numeric_limits<_Rep>::lowest() || __ticks > (numeric_limits<_Rep>::max)())
      return false;
    __out = duration<_Rep, _Period>{static_cast<_Rep>(__ticks)};
  } else {
    using _Duration  = duration<_Rep, _Period>;
    using _HMS       = hh_mm_ss<_Duration>;
    using _Precision = typename _HMS::precision;

    // Combine in the parsed precision before converting to the target period.
    // Use the representation's arithmetic rather than built-in overflow operations.
    _Precision __value       = chrono::duration_cast<_Precision>(seconds{static_cast<seconds::rep>(__seconds)});
    const int64_t __fraction = __f.__subseconds_ / chrono::__pow10(18 - _HMS::fractional_width);
    __value += _Precision{static_cast<typename _Precision::rep>(__fraction)};
    __out = chrono::duration_cast<_Duration>(__value);
  }
  return true;
}

template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(__fields_storage& __f, sys_time<_Duration>& __out) {
  sys_days __date{};
  if (!chrono::__try_get_date(__f, __date))
    return false;

  // Seconds are capped at 59 because sys_time (system_clock) is leap-second
  // oblivious; the utc_time builder allows 60.
  const int __hour = chrono::__compute_hour(__f);
  if (!chrono::__time_of_day_ok(__f, __hour, 59))
    return false;

  // %z gives the offset of the parsed time from UTC, so it is subtracted to
  // arrive at the UTC time sys_time holds. It is zero when %z was not used.
  // A target coarser than the parsed value (a sys_days parsed with "%F %T") is
  // rounded down, so that the day is the day that was written.
  __out =
      chrono::floor<_Duration>(__date + chrono::__to_time_of_day<_Duration>(__f, __hour) - minutes{__f.__utc_offset_});
  return true;
}

// A parsed UTC offset is not applied to local_time.
template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(__fields_storage& __f, local_time<_Duration>& __out) {
  sys_days __date{};
  if (!chrono::__try_get_date(__f, __date))
    return false;

  const int __hour = chrono::__compute_hour(__f);
  if (!chrono::__time_of_day_ok(__f, __hour, 59))
    return false;

  __out = chrono::floor<_Duration>(
      local_days{__date.time_since_epoch()} + chrono::__to_time_of_day<_Duration>(__f, __hour));
  return true;
}

template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(__fields_storage& __f, file_time<_Duration>& __out) {
  sys_time<_Duration> __st{};
  if (!chrono::__from_fields(__f, __st))
    return false;

  __out = file_clock::from_sys(__st);
  return true;
}

#    if _LIBCPP_HAS_EXPERIMENTAL_TZDB
// utc_time permits a leap second.
template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(__fields_storage& __f, utc_time<_Duration>& __out) {
  sys_days __date{};
  if (!chrono::__try_get_date(__f, __date))
    return false;

  const int __hour = chrono::__compute_hour(__f);
  if (!chrono::__time_of_day_ok(__f, __hour, 60))
    return false;

  // Converting the date before adding the time of day keeps a 60th second
  // inside the leap second instead of overflowing the day.
  __out = chrono::floor<_Duration>(
      utc_clock::from_sys(__date) + chrono::__to_time_of_day<_Duration>(__f, __hour) - minutes{__f.__utc_offset_});
  return true;
}

template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(__fields_storage& __f, tai_time<_Duration>& __out) {
  sys_days __date{};
  if (!chrono::__try_get_date(__f, __date))
    return false;

  const int __hour = chrono::__compute_hour(__f);
  if (!chrono::__time_of_day_ok(__f, __hour, 59))
    return false;

  constexpr sys_days __tai_epoch{-days{4383}}; // 1958-01-01.
  __out = chrono::floor<_Duration>(tai_time<days>{__date - __tai_epoch} +
                                   chrono::__to_time_of_day<_Duration>(__f, __hour) - minutes{__f.__utc_offset_});
  return true;
}

template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(__fields_storage& __f, gps_time<_Duration>& __out) {
  sys_days __date{};
  if (!chrono::__try_get_date(__f, __date))
    return false;

  const int __hour = chrono::__compute_hour(__f);
  if (!chrono::__time_of_day_ok(__f, __hour, 59))
    return false;

  constexpr sys_days __gps_epoch{days{3657}}; // 1980-01-06.
  __out = chrono::floor<_Duration>(gps_time<days>{__date - __gps_epoch} +
                                   chrono::__to_time_of_day<_Duration>(__f, __hour) - minutes{__f.__utc_offset_});
  return true;
}
#    endif // _LIBCPP_HAS_EXPERIMENTAL_TZDB

// Calendrical results reject fields they cannot represent. UTC offsets remain
// allowed as an auxiliary output; time zone abbreviations are stored separately.
_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, day& __out) {
  if (__f.__used_fields() != __fields_set::__day)
    return false;

  if (__f.__day_ < 1 || __f.__day_ > 31)
    return false;

  __out = day{static_cast<unsigned>(__f.__day_)};
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, month& __out) {
  if (__f.__used_fields() != __fields_set::__month)
    return false;

  if (__f.__month_ < 1 || __f.__month_ > 12)
    return false;

  __out = month{static_cast<unsigned>(__f.__month_)};
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(__fields_storage& __f, year& __out) {
  constexpr auto __allowed =
      __fields_set::__year | __fields_set::__century | __fields_set::__year_of_century | __fields_set::__utc_offset;
  if (!__f.__has_only(__allowed))
    return false;

  if (!chrono::__try_get_year(__f))
    return false;

  __out = year{__f.__year_};
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, weekday& __out) {
  if (__f.__used_fields() != __fields_set::__weekday)
    return false;

  if (__f.__weekday_ < 0 || __f.__weekday_ > 6)
    return false;

  __out = weekday{static_cast<unsigned>(__f.__weekday_)};
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, month_day& __out) {
  if (__f.__used_fields() != (__fields_set::__month | __fields_set::__day))
    return false;

  if (__f.__month_ < 1 || __f.__month_ > 12 || __f.__day_ < 1 || __f.__day_ > 31)
    return false;

  month_day __md{month{static_cast<unsigned>(__f.__month_)}, day{static_cast<unsigned>(__f.__day_)}};
  if (!__md.ok())
    return false;

  __out = __md;
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(__fields_storage& __f, year_month& __out) {
  constexpr auto __allowed =
      __fields_set::__year | __fields_set::__century | __fields_set::__year_of_century | __fields_set::__month |
      __fields_set::__utc_offset;
  if (!__f.__has_only(__allowed))
    return false;

  if (!__f.__has(__fields_set::__month))
    return false;

  if (!chrono::__try_get_year(__f) || !chrono::__validate_month(__f, __f.__month_))
    return false;

  __out = year_month{year{__f.__year_}, month{static_cast<unsigned>(__f.__month_)}};
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(__fields_storage& __f, year_month_day& __out) {
  constexpr auto __allowed =
      __fields_set::__year | __fields_set::__century | __fields_set::__year_of_century | __fields_set::__month |
      __fields_set::__day | __fields_set::__iso_year | __fields_set::__iso_week | __fields_set::__weekday |
      __fields_set::__day_of_year | __fields_set::__week_sun | __fields_set::__week_mon | __fields_set::__utc_offset;
  if (!__f.__has_only(__allowed))
    return false;

  // Resolve the date and check that all supplied date fields agree.
  sys_days __date{};
  if (!chrono::__try_get_date(__f, __date))
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
    basic_string<_CharT, _Traits> __parsed_abbrev;

    // Parse the input according to the format and collect the fields.
    chrono::__parse_from_stream(
        __is, __fmt, __f, __abbrev ? &__parsed_abbrev : nullptr, nullptr, __parse_options_v<_Tp>);
    if (__is.fail())
      return __is;

    // Resolve and validate the fields needed to construct the requested result.
    _Tp __out{};
    if (!chrono::__from_fields(__f, __out)) {
      __is.setstate(ios_base::failbit);
      return __is;
    }

    if (__abbrev && !__parsed_abbrev.empty())
      __abbrev->assign(__parsed_abbrev.data(), __parsed_abbrev.size());
    if (__offset && __f.__has(__fields_set::__utc_offset))
      *__offset = minutes{__f.__utc_offset_};
    __value = __out;
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

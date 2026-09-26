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
#  include <string>

#  if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#    pragma GCC system_header
#  endif

#  if _LIBCPP_STD_VER >= 20

_LIBCPP_BEGIN_NAMESPACE_STD

namespace chrono {
// The number of fractional digits read by %S for the target type.
template <class _Tp>
inline constexpr unsigned __fractional_width_v = 0;

template <class _Rep, class _Period>
inline constexpr unsigned __fractional_width_v<duration<_Rep, _Period> > =
    hh_mm_ss<duration<_Rep, _Period> >::fractional_width;

template <class _Clock, class _Duration>
inline constexpr unsigned __fractional_width_v<time_point<_Clock, _Duration> > = __fractional_width_v<_Duration>;

// Check an inclusive range without narrowing parsed int or int64_t fields.
_LIBCPP_HIDE_FROM_ABI constexpr bool __in_range(int64_t __value, int64_t __lo, int64_t __hi) {
  return __lo <= __value && __value <= __hi;
}

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
  __f.__subseconds_ = static_cast<int64_t>(__fraction.__value) * __pow10(18 - __fraction.__digits_read);
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
    unsigned __fractional_width) {
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

    if ((__has_width && !__width_allowed(__spec)) || (__modifier != 0 && !__modifier_allowed(__modifier, __spec))) {
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
          __is, _LIBCPP_STATICALLY_WIDEN(_CharT, "%m/%d/%y"), __f, __abbrev, __offset, __fractional_width);
      break;

    case 'F':
      // A width on %F applies only to %Y.
      chrono::__read_signed(__is, __has_width ? __width : 4, __f.__year_);
      if (!__is.fail()) {
        __f.__set(__fields_set::__year);
        chrono::__parse_from_stream(
            __is, _LIBCPP_STATICALLY_WIDEN(_CharT, "-%m-%d"), __f, __abbrev, __offset, __fractional_width);
      }
      break;

    case 'g': {
      int __year_of_century = 0;
      chrono::__read_unsigned(__is, __has_width ? __width : 2, __year_of_century);
      if (!__is.fail()) {
        if (!__in_range(__year_of_century, 0, 99))
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
      chrono::__parse_from_stream(
          __is, _LIBCPP_STATICALLY_WIDEN(_CharT, "%H:%M"), __f, __abbrev, __offset, __fractional_width);
      break;

    case 'T':
      chrono::__parse_from_stream(
          __is, _LIBCPP_STATICALLY_WIDEN(_CharT, "%H:%M:%S"), __f, __abbrev, __offset, __fractional_width);
      break;

    case 'S': {
      // Without an explicit width the field is two digits, plus the decimal
      // point and the fractional digits the target can represent.
      unsigned __default_width = __fractional_width == 0 ? 2 : 3 + __fractional_width;
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
        if (!__in_range(__weekday, 1, 7))
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

// Construct a candidate year from %C/%y or %Y, then check consistency with any
// other supplied year fields. Store the result only if it is representable.
_LIBCPP_HIDE_FROM_ABI inline bool __try_get_year(const __fields_storage& __f, int& __out) {
  int64_t __year{};

  if (__f.__has(__fields_set::__year_of_century)) {
    if (!__in_range(__f.__year_of_century_, 0, 99))
      return false;

    // Construct the year from %y and either %C or the default century.
    if (__f.__has(__fields_set::__century)) {
      __year = static_cast<int64_t>(__f.__century_) * 100;
      // %C uses floored division; %y contains the absolute last two digits.
      if (__f.__century_ < 0 && __f.__year_of_century_ != 0)
        __year += 100 - __f.__year_of_century_;
      else
        __year += __f.__year_of_century_;
    } else {
      __year = (__f.__year_of_century_ <= 68 ? 2000 : 1900) + __f.__year_of_century_;
    }

    // Check the constructed year against %Y, if supplied.
    if (__f.__has(__fields_set::__year) && __f.__year_ != __year)
      return false;
  } else if (__f.__has(__fields_set::__year)) {
    __year = __f.__year_;

    // Check the year obtained from %Y against %C, if supplied.
    if (__f.__has(__fields_set::__century) && __f.__century_ != __year / 100 - (__year % 100 < 0))
      return false;
  } else {
    return false;
  }

  if (!__in_range(__year, static_cast<int>(year::min()), static_cast<int>(year::max())))
    return false;

  __out = static_cast<int>(__year);
  return true;
}

// Validate and combine %H, %I, and %p, storing the hour only on success.
_LIBCPP_HIDE_FROM_ABI inline bool __try_get_hour(const __fields_storage& __f, int& __out) {
  int __hour{};
  if (__f.__has(__fields_set::__hours)) {
    // %H determines the hour; check agreement with %I and %p if supplied.
    __hour = __f.__hours_;
    if (!__in_range(__hour, 0, 23))
      return false;
    if (__f.__has(__fields_set::__hour12)) {
      const int __hour12 = __hour == 0 ? 12 : (__hour > 12 ? __hour - 12 : __hour);
      if (__f.__hour12_ != __hour12)
        return false;
    }
    if (__f.__has(__fields_set::__am_pm) && (__hour >= 12) != __f.__is_pm_)
      return false;
  } else if (__f.__has(__fields_set::__hour12)) {
    // Without %H, %I requires %p to distinguish AM from PM.
    if (!__in_range(__f.__hour12_, 1, 12) || !__f.__has(__fields_set::__am_pm))
      return false;
    __hour = (__f.__hour12_ == 12 ? 0 : __f.__hour12_) + (__f.__is_pm_ ? 12 : 0);
  } else {
    // Default to zero when neither hour field was supplied.
    __hour = 0;
  }

  __out = __hour;
  return true;
}

// An absent field is considered in range.
_LIBCPP_HIDE_FROM_ABI constexpr bool
__in_range(const __fields_storage& __f, __fields_set __part, int __value, int __lo, int __hi) {
  return !__f.__has(__part) || __in_range(__value, __lo, __hi);
}

_LIBCPP_HIDE_FROM_ABI inline bool __validate_minute(const __fields_storage& __f, int __max_minute) {
  return __in_range(__f, __fields_set::__minutes, __f.__minutes_, 0, __max_minute);
}

_LIBCPP_HIDE_FROM_ABI inline bool __validate_second(const __fields_storage& __f, int __max_second) {
  return __in_range(__f, __fields_set::__seconds, __f.__seconds_, 0, __max_second) && __f.__subseconds_ >= 0;
}

// Converts an ISO year (%G, or expanded %g), week (%V), and weekday (%u/%w)
// to sys_days.
_LIBCPP_HIDE_FROM_ABI inline bool __iso_week_to_sys_days(int __g, int __v, weekday __wd, sys_days& __out) {
  if (!__in_range(__g, static_cast<int>(year::min()), static_cast<int>(year::max())) || !__in_range(__v, 1, 53))
    return false;

  // ISO week 1 contains __g-01-04 and starts on Monday.
  // Compute days from 1970-01-01 to __g/__v/__wd in four parts:
  // 1. Days from 1970-01-01 to __g-01-04.
  // 2. Subtract the initial partial week from that week's Monday to __g-01-04.
  // 3. (__v - 1) complete weeks preceding the requested week.
  // 4. The final partial week from Monday to __wd.
  sys_days __jan4 = static_cast<sys_days>(year_month_day{year{__g}, month{1}, day{4}});
  weekday __jan4_wd{__jan4};
  sys_days __week1_start = __jan4 - days{static_cast<int>(__jan4_wd.iso_encoding()) - 1};
  sys_days __result      = __week1_start + weeks{__v - 1} + days{static_cast<int>(__wd.iso_encoding()) - 1};

  // Reject a nonexistent week: the Thursday of the result's week must fall in
  // the ISO year '__g'.
  sys_days __thursday = __result + days{4 - static_cast<int>(__wd.iso_encoding())};
  if (year_month_day{__thursday}.year() != year{__g})
    return false;

  __out = __result;
  return true;
}

// Converts a calendar year (%Y or %C/%y), week (%U/%W), and weekday (%u/%w)
// to sys_days, including week zero.
// The caller must supply a valid year.
_LIBCPP_HIDE_FROM_ABI inline bool
__week_to_sys_days(int __year, int __week, weekday __first, weekday __wd, sys_days& __out) {
  if (!__in_range(__week, 0, 53))
    return false;

  // Compute days from 1970-01-01 to __year/__week/__wd in four parts:
  // 1. Days from 1970-01-01 to __year-01-01.
  // 2. The initial partial week from __year-01-01 to the next week start (__first).
  // 3. (__week - 1) complete weeks preceding the requested week.
  // 4. The final partial week from the week's start (__first) to __wd.
  sys_days __jan1{year{__year} / January / 1};
  sys_days __week1_start = __jan1 + (__first - weekday{__jan1});
  sys_days __result      = __week1_start + weeks{__week - 1} + (__wd - __first);
  if (year_month_day{__result}.year() != year{__year})
    return false;

  __out = __result;
  return true;
}

// Compare all parsed date fields with a valid date, including fields that do
// not form a complete representation on their own, e.g. %F followed by only %G.
_LIBCPP_HIDE_FROM_ABI inline bool __validate_date(const __fields_storage& __f, const year_month_day& __ymd) {
  auto __matches = [&](__fields_set __part, int __parsed, int __expected) {
    return !__f.__has(__part) || __parsed == __expected;
  };

  // Calendar year: %Y, %C, and %y, including a standalone %C.
  const auto __year = static_cast<int>(__ymd.year());
  if (!__matches(__fields_set::__year, __f.__year_, __year) ||
      !__matches(__fields_set::__century, __f.__century_, __year / 100 - (__year % 100 < 0)) ||
      !__matches(__fields_set::__year_of_century, __f.__year_of_century_, (__year < 0 ? -__year : __year) % 100))
    return false;

  // Without %C, %y denotes a year in [1969, 2068], not just matching last digits.
  if (__f.__has(__fields_set::__year_of_century) && !__f.__has(__fields_set::__century) &&
      !__in_range(__year, 1969, 2068))
    return false;

  // Month (%m/%b/%B/%h), day (%d/%e), and weekday (%a/%A/%u/%w).
  const sys_days __date{__ymd};
  const weekday __weekday{__date};
  if (!__matches(__fields_set::__month, __f.__month_, static_cast<unsigned>(__ymd.month())) ||
      !__matches(__fields_set::__day, __f.__day_, static_cast<unsigned>(__ymd.day())) ||
      !__matches(__fields_set::__weekday, __f.__weekday_, __weekday.c_encoding()))
    return false;

  // Day of year (%j) and Sunday-/Monday-based week numbers (%U/%W).
  const sys_days __jan1{__ymd.year() / January / 1};
  const int __day_of_year = static_cast<int>((__date - __jan1).count()) + 1;
  if (!__matches(__fields_set::__day_of_year, __f.__day_of_year_, __day_of_year) ||
      !__matches(__fields_set::__week_sun,
                 __f.__week_sun_,
                 (__day_of_year - static_cast<int>(__weekday.c_encoding()) + 6) / 7) ||
      !__matches(__fields_set::__week_mon,
                 __f.__week_mon_,
                 (__day_of_year - (static_cast<int>(__weekday.iso_encoding()) - 1) + 6) / 7))
    return false;

  if (__f.__has_any(__fields_set::__iso_year | __fields_set::__iso_week)) {
    // Find the Thursday of the week containing __date.
    // Its calendar year is the ISO year: adjust __iso_year and __iso_jan1
    // if that Thursday falls in the previous or next calendar year.
    // Compute the ISO week number from that Thursday's distance from __iso_jan1,
    // then compare the ISO year and week with any supplied %G/%g and %V fields.
    // Keep __iso_year as int because it can exceed chrono::year's valid range
    // near year::min()/max().
    const sys_days __thursday  = __date + days{4 - static_cast<int>(__weekday.iso_encoding())};
    const sys_days __next_jan1 = __jan1 + days{__ymd.year().is_leap() ? 366 : 365};
    int __iso_year             = __year;
    sys_days __iso_jan1        = __jan1;
    if (__thursday < __jan1) {
      // This week's Thursday falls in the previous calendar year, so the ISO year is __year - 1.
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

// Construct a valid date from the first complete combination of parsed fields.
_LIBCPP_HIDE_FROM_ABI inline bool __make_date(const __fields_storage& __f, year_month_day& __out) {
  // Calendar-year combinations use %Y or a year obtained from %C/%y.
  if (int __year{}; __try_get_year(__f, __year)) {
    // Calendar date: %Y %m %d, also supplied by formats such as %F, %D, or %x.
    if (__f.__has(__fields_set::__month | __fields_set::__day)) {
      if (!__in_range(__f.__month_, 1, 12) || !__in_range(__f.__day_, 1, 31))
        return false;

      const year_month_day __ymd{
          year{__year}, month{static_cast<unsigned>(__f.__month_)}, day{static_cast<unsigned>(__f.__day_)}};
      if (!__ymd.ok())
        return false;
      __out = __ymd;
      return true;
    }

    // Ordinal date: %Y %j.
    if (__f.__has(__fields_set::__day_of_year)) {
      if (!__in_range(__f.__day_of_year_, 1, year{__year}.is_leap() ? 366 : 365))
        return false;

      __out = year_month_day{sys_days{year{__year} / January / 1} + days{__f.__day_of_year_ - 1}};
      return true;
    }

    // Week date: %Y with %U or %W and a weekday (%a/%A/%u/%w).
    if (__f.__has(__fields_set::__weekday) && __f.__has_any(__fields_set::__week_sun | __fields_set::__week_mon)) {
      if (!__in_range(__f.__weekday_, 0, 6))
        return false;
      const bool __use_sunday = __f.__has(__fields_set::__week_sun);
      sys_days __date{};
      if (!__week_to_sys_days(
              __year,
              __use_sunday ? __f.__week_sun_ : __f.__week_mon_,
              __use_sunday ? Sunday : Monday,
              weekday{static_cast<unsigned>(__f.__weekday_)},
              __date))
        return false;

      __out = year_month_day{__date};
      return true;
    }
  }

  // ISO week date: %G (or expanded %g), %V, and a weekday (%a/%A/%u/%w).
  if (__f.__has(__fields_set::__iso_year | __fields_set::__iso_week | __fields_set::__weekday)) {
    sys_days __date{};
    if (!__in_range(__f.__weekday_, 0, 6) ||
        !__iso_week_to_sys_days(
            __f.__iso_year_, __f.__iso_week_, weekday{static_cast<unsigned>(__f.__weekday_)}, __date))
      return false;

    // The calendar year can lie outside chrono::year's range.
    const year_month_day __ymd{__date};
    if (!__ymd.ok())
      return false;
    __out = __ymd;
    return true;
  }

  return false;
}

_LIBCPP_HIDE_FROM_ABI inline bool __try_get_date(const __fields_storage& __f, sys_days& __out) {
  // First construct a valid candidate, e.g. from %F/%x, %Y %j, or %G %V %u.
  year_month_day __ymd{};
  if (!__make_date(__f, __ymd))
    return false;

  // Then check all parsed date fields, including incomplete representations
  // such as a standalone %G or %V alongside %F.
  if (!__validate_date(__f, __ymd))
    return false;

  __out = sys_days{__ymd};
  return true;
}

template <class _Duration>
_LIBCPP_HIDE_FROM_ABI _Duration __to_time_of_day(const __fields_storage& __f, int __hour) {
  auto __result = chrono::duration_cast<_Duration>(hours{__hour} + minutes{__f.__minutes_} + seconds{__f.__seconds_});

  // A target that cannot hold a fraction of a second never parses one, and
  // converting attoseconds to such a coarse period would overflow the ratio
  // arithmetic, so the conversion is not even instantiated.
  if constexpr (__fractional_width_v<_Duration> != 0)
    if (__f.__subseconds_ != 0)
      __result += chrono::duration_cast<_Duration>(duration<int64_t, atto>{__f.__subseconds_});

  return __result;
}

// Builders validate parsed fields and convert them to the requested type.

template <class _Rep, class _Period>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(const __fields_storage& __f, duration<_Rep, _Period>& __out) {
  // Durations can represent only elapsed days and time-of-day fields.
  // UTC offsets and time zone abbreviations are allowed but do not contribute to the duration.
  constexpr auto __duration_components =
      __fields_set::__day_of_year | __fields_set::__hours | __fields_set::__hour12 | __fields_set::__minutes |
      __fields_set::__seconds;
  if (!__f.__has_only(__duration_components | __fields_set::__am_pm))
    return false;

  // Require at least one numeric component; %p alone is insufficient.
  if (!__f.__has_any(__duration_components))
    return false;

  int __hour{};
  // Use clock-time ranges for %H, %M, and %S; %j supplies any additional days.
  if (__f.__day_of_year_ < 0 || !__try_get_hour(__f, __hour) || !__validate_minute(__f, 59) ||
      !__validate_second(__f, 59))
    return false;

  constexpr uint64_t __seconds_per_minute = 60;
  constexpr uint64_t __seconds_per_hour   = 60 * __seconds_per_minute;
  constexpr uint64_t __seconds_per_day    = 24 * __seconds_per_hour;

  // Sum the components in seconds; when parsing a duration, %j denotes a day count, not a day of the year.
  // Every whole-number field fits in int, so their sum in seconds fits in uint64_t.
  const uint64_t __seconds =
      static_cast<uint64_t>(__f.__day_of_year_) * __seconds_per_day +
      static_cast<uint64_t>(__hour) * __seconds_per_hour +
      static_cast<uint64_t>(__f.__minutes_) * __seconds_per_minute + __f.__seconds_;

  using _Duration  = duration<_Rep, _Period>;
  using _HMS       = hh_mm_ss<_Duration>;
  using _Precision = typename _HMS::precision;

  // Combine in the parsed precision before converting to the target period.
  // TODO: Detect overflow in duration conversion and accumulation.
  _Precision __value       = chrono::duration_cast<_Precision>(seconds{static_cast<seconds::rep>(__seconds)});
  const int64_t __fraction = __f.__subseconds_ / __pow10(18 - _HMS::fractional_width);
  __value += _Precision{static_cast<typename _Precision::rep>(__fraction)};
  __out = chrono::duration_cast<_Duration>(__value);
  return true;
}

template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(const __fields_storage& __f, sys_time<_Duration>& __out) {
  sys_days __date{};
  if (!__try_get_date(__f, __date))
    return false;

  // sys_time does not represent leap seconds, so seconds must be in [0, 59].
  int __hour{};
  if (!__try_get_hour(__f, __hour) || !__validate_minute(__f, 59) || !__validate_second(__f, 59))
    return false;

  // %z gives the offset of the parsed time from UTC, so it is subtracted to
  // arrive at the UTC time sys_time holds. It is zero when %z was not used.
  // A target coarser than the parsed value (a sys_days parsed with "%F %T") is
  // rounded down, so that the day is the day that was written.
  __out = chrono::floor<_Duration>(__date + __to_time_of_day<_Duration>(__f, __hour) - minutes{__f.__utc_offset_});
  return true;
}

// A parsed UTC offset is not applied to local_time.
template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(const __fields_storage& __f, local_time<_Duration>& __out) {
  sys_days __date{};
  if (!__try_get_date(__f, __date))
    return false;

  int __hour{};
  if (!__try_get_hour(__f, __hour) || !__validate_minute(__f, 59) || !__validate_second(__f, 59))
    return false;

  __out = chrono::floor<_Duration>(local_days{__date.time_since_epoch()} + __to_time_of_day<_Duration>(__f, __hour));
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
template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(const __fields_storage& __f, utc_time<_Duration>& __out) {
  sys_days __date{};
  if (!__try_get_date(__f, __date))
    return false;

  // utc_time can represent leap seconds, so the seconds field may be 60.
  int __hour{};
  if (!__try_get_hour(__f, __hour) || !__validate_minute(__f, 59) || !__validate_second(__f, 60))
    return false;

  // Converting the date before adding the time of day keeps a 60th second
  // inside the leap second instead of overflowing the day.
  __out = chrono::floor<_Duration>(
      utc_clock::from_sys(__date) + __to_time_of_day<_Duration>(__f, __hour) - minutes{__f.__utc_offset_});
  return true;
}

template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(const __fields_storage& __f, tai_time<_Duration>& __out) {
  sys_days __date{};
  if (!__try_get_date(__f, __date))
    return false;

  int __hour{};
  if (!__try_get_hour(__f, __hour) || !__validate_minute(__f, 59) || !__validate_second(__f, 59))
    return false;

  constexpr sys_days __tai_epoch{-days{4383}}; // 1958-01-01.
  __out = chrono::floor<_Duration>(
      tai_time<days>{__date - __tai_epoch} + __to_time_of_day<_Duration>(__f, __hour) - minutes{__f.__utc_offset_});
  return true;
}

template <class _Duration>
_LIBCPP_HIDE_FROM_ABI bool __from_fields(const __fields_storage& __f, gps_time<_Duration>& __out) {
  sys_days __date{};
  if (!__try_get_date(__f, __date))
    return false;

  int __hour{};
  if (!__try_get_hour(__f, __hour) || !__validate_minute(__f, 59) || !__validate_second(__f, 59))
    return false;

  constexpr sys_days __gps_epoch{days{3657}}; // 1980-01-06.
  __out = chrono::floor<_Duration>(
      gps_time<days>{__date - __gps_epoch} + __to_time_of_day<_Duration>(__f, __hour) - minutes{__f.__utc_offset_});
  return true;
}
#    endif // _LIBCPP_HAS_EXPERIMENTAL_TZDB

// Calendrical results reject fields they cannot represent. UTC offsets are
// excluded from these checks; time zone abbreviations are stored separately.
_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, day& __out) {
  if (!__f.__has_exactly(__fields_set::__day))
    return false;

  if (!__in_range(__f.__day_, 1, 31))
    return false;

  __out = day{static_cast<unsigned>(__f.__day_)};
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, month& __out) {
  if (!__f.__has_exactly(__fields_set::__month))
    return false;

  if (!__in_range(__f.__month_, 1, 12))
    return false;

  __out = month{static_cast<unsigned>(__f.__month_)};
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, year& __out) {
  constexpr auto __year_fields = __fields_set::__year | __fields_set::__century | __fields_set::__year_of_century;
  if (!__f.__has_only(__year_fields))
    return false;

  int __year{};
  if (!__try_get_year(__f, __year))
    return false;

  __out = year{__year};
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, weekday& __out) {
  if (!__f.__has_exactly(__fields_set::__weekday))
    return false;

  if (!__in_range(__f.__weekday_, 0, 6))
    return false;

  __out = weekday{static_cast<unsigned>(__f.__weekday_)};
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, month_day& __out) {
  if (!__f.__has_exactly(__fields_set::__month | __fields_set::__day))
    return false;

  if (!__in_range(__f.__month_, 1, 12) || !__in_range(__f.__day_, 1, 31))
    return false;

  month_day __md{month{static_cast<unsigned>(__f.__month_)}, day{static_cast<unsigned>(__f.__day_)}};
  if (!__md.ok())
    return false;

  __out = __md;
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, year_month& __out) {
  constexpr auto __year_fields = __fields_set::__year | __fields_set::__century | __fields_set::__year_of_century;
  if (!__f.__has_exactly(__fields_set::__month, __year_fields))
    return false;

  int __year{};
  if (!__try_get_year(__f, __year))
    return false;

  if (!__in_range(__f.__month_, 1, 12))
    return false;

  __out = year_month{year{__year}, month{static_cast<unsigned>(__f.__month_)}};
  return true;
}

_LIBCPP_HIDE_FROM_ABI inline bool __from_fields(const __fields_storage& __f, year_month_day& __out) {
  constexpr auto __date_fields =
      __fields_set::__year | __fields_set::__century | __fields_set::__year_of_century | __fields_set::__month |
      __fields_set::__day | __fields_set::__iso_year | __fields_set::__iso_week | __fields_set::__weekday |
      __fields_set::__day_of_year | __fields_set::__week_sun | __fields_set::__week_mon;
  if (!__f.__has_only(__date_fields))
    return false;

  // Resolve the date and check that all supplied date fields agree.
  sys_days __date{};
  if (!__try_get_date(__f, __date))
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
        __is, __fmt, __f, __abbrev ? &__parsed_abbrev : nullptr, nullptr, __fractional_width_v<_Tp>);
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

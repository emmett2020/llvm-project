// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___CHRONO_PARSE_H
#define _LIBCPP___CHRONO_PARSE_H

#include <__config>

#if _LIBCPP_HAS_LOCALIZATION

#  include <__chrono/duration.h>
#  include <__chrono/from_stream.h>
#  include <__fwd/istream.h>
#  include <__fwd/memory.h>
#  include <__fwd/string.h>
#  include <__memory/addressof.h>

#  if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#    pragma GCC system_header
#  endif

#  if _LIBCPP_STD_VER >= 20

_LIBCPP_BEGIN_NAMESPACE_STD

namespace chrono {

// [time.parse]: a Parsable is anything from_stream can read, with the trailing
// arguments the selected parse overload passes on. The call is unqualified, so
// a user-defined type that provides its own from_stream is parsable as well.
template <class _Parsable, class _CharT, class _Traits, class... _Args>
concept __parsable_with =
    requires(basic_istream<_CharT, _Traits>& __is, const _CharT* __fmt, _Parsable& __tp, _Args*... __args) {
      from_stream(__is, __fmt, __tp, __args...);
    };

// The manipulators returned by parse. They store the format and where to put
// the result and do their work in the extractor; user code never names them.
// The format is kept as a pointer, so the string a parse call was given has to
// outlive the extraction -- which it does in the intended
// "is >> parse(fmt, tp)" usage.
//
// There is one manipulator per parse overload rather than a single one with
// null pointers, because the number of arguments in the resulting from_stream
// call is what tells the four forms apart.

template <class _CharT, class _Parsable>
struct __parse_manip {
  const _CharT* __fmt_;
  _Parsable* __tp_;

  template <class _Traits>
  _LIBCPP_HIDE_FROM_ABI friend basic_istream<_CharT, _Traits>&
  operator>>(basic_istream<_CharT, _Traits>& __is, const __parse_manip& __manip) {
    return from_stream(__is, __manip.__fmt_, *__manip.__tp_);
  }
};

template <class _CharT, class _Parsable>
struct __parse_manip_offset {
  const _CharT* __fmt_;
  _Parsable* __tp_;
  minutes* __offset_;

  template <class _Traits>
  _LIBCPP_HIDE_FROM_ABI friend basic_istream<_CharT, _Traits>&
  operator>>(basic_istream<_CharT, _Traits>& __is, const __parse_manip_offset& __manip) {
    return from_stream(
        __is,
        __manip.__fmt_,
        *__manip.__tp_,
        static_cast<basic_string<_CharT, _Traits>*>(nullptr),
        __manip.__offset_);
  }
};

template <class _CharT, class _Traits, class _Alloc, class _Parsable>
struct __parse_manip_abbrev {
  const _CharT* __fmt_;
  _Parsable* __tp_;
  basic_string<_CharT, _Traits, _Alloc>* __abbrev_;

  _LIBCPP_HIDE_FROM_ABI friend basic_istream<_CharT, _Traits>&
  operator>>(basic_istream<_CharT, _Traits>& __is, const __parse_manip_abbrev& __manip) {
    return from_stream(__is, __manip.__fmt_, *__manip.__tp_, __manip.__abbrev_);
  }
};

template <class _CharT, class _Traits, class _Alloc, class _Parsable>
struct __parse_manip_abbrev_offset {
  const _CharT* __fmt_;
  _Parsable* __tp_;
  basic_string<_CharT, _Traits, _Alloc>* __abbrev_;
  minutes* __offset_;

  _LIBCPP_HIDE_FROM_ABI friend basic_istream<_CharT, _Traits>&
  operator>>(basic_istream<_CharT, _Traits>& __is, const __parse_manip_abbrev_offset& __manip) {
    return from_stream(__is, __manip.__fmt_, *__manip.__tp_, __manip.__abbrev_, __manip.__offset_);
  }
};

template <class _CharT, class _Parsable>
  requires __parsable_with<_Parsable, _CharT, char_traits<_CharT>>
_LIBCPP_HIDE_FROM_ABI __parse_manip<_CharT, _Parsable> parse(const _CharT* __fmt, _Parsable& __tp) {
  return {__fmt, std::addressof(__tp)};
}

template <class _CharT, class _Traits, class _Alloc, class _Parsable>
  requires __parsable_with<_Parsable, _CharT, _Traits>
_LIBCPP_HIDE_FROM_ABI __parse_manip<_CharT, _Parsable>
parse(const basic_string<_CharT, _Traits, _Alloc>& __fmt, _Parsable& __tp) {
  return {__fmt.c_str(), std::addressof(__tp)};
}

template <class _CharT, class _Traits, class _Alloc, class _Parsable>
  requires __parsable_with<_Parsable, _CharT, _Traits, basic_string<_CharT, _Traits, _Alloc>>
_LIBCPP_HIDE_FROM_ABI __parse_manip_abbrev<_CharT, _Traits, _Alloc, _Parsable>
parse(const _CharT* __fmt, _Parsable& __tp, basic_string<_CharT, _Traits, _Alloc>& __abbrev) {
  return {__fmt, std::addressof(__tp), std::addressof(__abbrev)};
}

template <class _CharT, class _Traits, class _Alloc, class _Parsable>
  requires __parsable_with<_Parsable, _CharT, _Traits, basic_string<_CharT, _Traits, _Alloc>>
_LIBCPP_HIDE_FROM_ABI __parse_manip_abbrev<_CharT, _Traits, _Alloc, _Parsable>
parse(const basic_string<_CharT, _Traits, _Alloc>& __fmt,
      _Parsable& __tp,
      basic_string<_CharT, _Traits, _Alloc>& __abbrev) {
  return {__fmt.c_str(), std::addressof(__tp), std::addressof(__abbrev)};
}

template <class _CharT, class _Parsable>
  requires __parsable_with<_Parsable, _CharT, char_traits<_CharT>, basic_string<_CharT>, minutes>
_LIBCPP_HIDE_FROM_ABI __parse_manip_offset<_CharT, _Parsable>
parse(const _CharT* __fmt, _Parsable& __tp, minutes& __offset) {
  return {__fmt, std::addressof(__tp), std::addressof(__offset)};
}

template <class _CharT, class _Traits, class _Alloc, class _Parsable>
  requires __parsable_with<_Parsable, _CharT, _Traits, basic_string<_CharT, _Traits>, minutes>
_LIBCPP_HIDE_FROM_ABI __parse_manip_offset<_CharT, _Parsable>
parse(const basic_string<_CharT, _Traits, _Alloc>& __fmt, _Parsable& __tp, minutes& __offset) {
  return {__fmt.c_str(), std::addressof(__tp), std::addressof(__offset)};
}

template <class _CharT, class _Traits, class _Alloc, class _Parsable>
  requires __parsable_with<_Parsable, _CharT, _Traits, basic_string<_CharT, _Traits, _Alloc>, minutes>
_LIBCPP_HIDE_FROM_ABI __parse_manip_abbrev_offset<_CharT, _Traits, _Alloc, _Parsable>
parse(const _CharT* __fmt, _Parsable& __tp, basic_string<_CharT, _Traits, _Alloc>& __abbrev, minutes& __offset) {
  return {__fmt, std::addressof(__tp), std::addressof(__abbrev), std::addressof(__offset)};
}

template <class _CharT, class _Traits, class _Alloc, class _Parsable>
  requires __parsable_with<_Parsable, _CharT, _Traits, basic_string<_CharT, _Traits, _Alloc>, minutes>
_LIBCPP_HIDE_FROM_ABI __parse_manip_abbrev_offset<_CharT, _Traits, _Alloc, _Parsable>
parse(const basic_string<_CharT, _Traits, _Alloc>& __fmt,
      _Parsable& __tp,
      basic_string<_CharT, _Traits, _Alloc>& __abbrev,
      minutes& __offset) {
  return {__fmt.c_str(), std::addressof(__tp), std::addressof(__abbrev), std::addressof(__offset)};
}

} // namespace chrono

_LIBCPP_END_NAMESPACE_STD

#  endif

#endif

#endif //_LIBCPP___CHRONO_PARSE_H

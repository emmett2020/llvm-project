//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17
// UNSUPPORTED: no-localization

// Date/time construction must leave the parsed fields unchanged on success and failure.

#include <cassert>
#include <chrono>
#include <limits>

using namespace std::chrono;
using Fields = std::chrono::__fields_storage;
using Parts  = std::chrono::__fields_set;

static_assert(std::chrono::__fractional_width_v<year_month_day> == 0);
static_assert(std::chrono::__fractional_width_v<seconds> == 0);
static_assert(std::chrono::__fractional_width_v<hours> == 0);
static_assert(std::chrono::__fractional_width_v<milliseconds> == 3);
static_assert(std::chrono::__fractional_width_v<microseconds> == 6);
static_assert(std::chrono::__fractional_width_v<nanoseconds> == 9);
static_assert(std::chrono::__fractional_width_v<sys_days> == 0);
static_assert(std::chrono::__fractional_width_v<sys_time<milliseconds>> == 3);
static_assert(std::chrono::__fractional_width_v<local_time<microseconds>> == 6);
static_assert(std::chrono::__fractional_width_v<file_time<nanoseconds>> == 9);

constexpr bool test_in_range() {
  assert(std::chrono::__in_range(1, 1, 12));
  assert(std::chrono::__in_range(12, 1, 12));
  assert(!std::chrono::__in_range(0, 1, 12));
  assert(!std::chrono::__in_range(13, 1, 12));
  assert(std::chrono::__in_range(-1, -2, 0));
  assert(std::chrono::__in_range(0, 0, 0));

  const int64_t min = std::numeric_limits<int64_t>::min();
  const int64_t max = std::numeric_limits<int64_t>::max();
  assert(std::chrono::__in_range(min, min, max));
  assert(std::chrono::__in_range(max, min, max));
  assert(!std::chrono::__in_range(min, -32767, 32767));
  assert(!std::chrono::__in_range(max, -32767, 32767));
  assert(!std::chrono::__in_range(int64_t{1} << 32, 0, 12));

  Fields fields;
  fields.__minutes_ = 60;
  assert(std::chrono::__in_range(fields, Parts::__minutes, fields.__minutes_, 0, 59));
  fields.__set(Parts::__minutes);
  assert(!std::chrono::__in_range(fields, Parts::__minutes, fields.__minutes_, 0, 59));
  fields.__minutes_ = 59;
  assert(std::chrono::__in_range(fields, Parts::__minutes, fields.__minutes_, 0, 59));
  return true;
}

constexpr bool test_field_queries() {
  Fields fields;
  assert(fields.__has(Parts::__none));
  assert(!fields.__has(Parts::__day));
  assert(!fields.__has_any(Parts::__none));
  assert(!fields.__has_any(Parts::__day | Parts::__month));
  assert(!fields.__has(Parts::__utc_offset));
  assert(fields.__has_exactly(Parts::__none));
  assert(fields.__has_only(Parts::__day));
  assert(!fields.__has_exactly(Parts::__day));
  assert(!fields.__has_exactly(Parts::__day, Parts::__month));
  fields.__utc_offset_ = 480;
  fields.__set(Parts::__utc_offset);
  assert(fields.__has(Parts::__utc_offset));
  assert(fields.__has_any(Parts::__utc_offset));
  assert(fields.__has_only(Parts::__none));
  assert(fields.__has_exactly(Parts::__none));
  assert(!fields.__has_exactly(Parts::__day));
  fields.__set(Parts::__day);
  assert(fields.__has(Parts::__day | Parts::__utc_offset));
  assert(!fields.__has(Parts::__day | Parts::__month));
  assert(fields.__has_any(Parts::__day | Parts::__month));
  assert(!fields.__has_only(Parts::__month));
  assert(!fields.__has_only(Parts::__none));
  assert(fields.__has_exactly(Parts::__day));
  assert(fields.__has_exactly(Parts::__day, Parts::__month));
  assert(fields.__has_only(Parts::__day | Parts::__month));
  assert(!fields.__has_exactly(Parts::__month, Parts::__day));
  assert(!fields.__has_exactly(Parts::__day | Parts::__month));
  fields.__set(Parts::__month);
  assert(fields.__has(Parts::__day | Parts::__month));
  assert(fields.__has_any(Parts::__month | Parts::__year));
  assert(!fields.__has_any(Parts::__year | Parts::__hours));
  assert(!fields.__has_only(Parts::__day));
  assert(fields.__has_only(Parts::__day | Parts::__month));
  assert(!fields.__has_exactly(Parts::__day));
  assert(fields.__has_exactly(Parts::__day, Parts::__month));
  assert(fields.__has_exactly(Parts::__day | Parts::__month));
  assert(!fields.__has_exactly(Parts::__year, Parts::__day | Parts::__month));
  assert(fields.__present_ == (Parts::__day | Parts::__month | Parts::__utc_offset));
  assert(fields.__has(Parts::__utc_offset));
  assert(fields.__utc_offset_ == 480);
  fields.__set(Parts::__hours);
  assert(!fields.__has_exactly(Parts::__day, Parts::__month));
  assert(!fields.__has_only(Parts::__day | Parts::__month));
  return true;
}

template <class T>
void check(const Fields& fields, T expected, bool success = true) {
  const T initial{};
  T result              = initial;
  const Fields snapshot = fields;
  assert(std::chrono::__from_fields(snapshot, result) == success);
  assert(result == (success ? expected : initial));
  assert(snapshot.__present_ == fields.__present_);
  assert(snapshot.__year_ == fields.__year_);
  assert(snapshot.__month_ == fields.__month_);
  assert(snapshot.__day_ == fields.__day_);
}

void test_year() {
  Fields fields;
  fields.__century_         = 20;
  fields.__year_of_century_ = 26;
  fields.__set(Parts::__century | Parts::__year_of_century);
  const Fields snapshot = fields;
  int parsed_year{};
  assert(std::chrono::__try_get_year(snapshot, parsed_year));
  assert(parsed_year == 2026);
  assert(snapshot.__present_ == fields.__present_);
  assert(snapshot.__year_ == fields.__year_);
  assert(snapshot.__century_ == 20);
  assert(snapshot.__year_of_century_ == 26);
  check(fields, 2026y);
  assert(fields.__year_ == 0);
  assert(!fields.__has(Parts::__year));

  fields.__month_ = 7;
  fields.__set(Parts::__month);
  check(fields, 2026y / July);
  fields.__year_ = 2025;
  fields.__set(Parts::__year);
  // Obtaining a year must not erase a conflicting explicit value.
  assert(!std::chrono::__try_get_year(fields, parsed_year));
  assert(parsed_year == 2026);
  check(fields, 2026y / July, false);
  assert(fields.__year_ == 2025);

  fields.__present_         = Parts::__century | Parts::__year_of_century;
  fields.__century_         = -20;
  fields.__year_of_century_ = 76;
  check(fields, year{-1976});
  fields.__year_of_century_ = 0;
  check(fields, year{-2000});

  fields.__century_         = std::numeric_limits<int>::max();
  fields.__year_of_century_ = 99;
  assert(!std::chrono::__try_get_year(fields, parsed_year));
  assert(parsed_year == 2026);
  check(fields, year{1}, false);
  fields.__century_         = std::numeric_limits<int>::min();
  fields.__year_of_century_ = std::numeric_limits<int>::min();
  check(fields, year{1}, false);

  fields            = {};
  fields.__century_ = 20;
  fields.__set(Parts::__century);
  assert(!std::chrono::__try_get_year(fields, parsed_year));
  assert(parsed_year == 2026);
  assert(!fields.__has(Parts::__year));
  fields.__year_ = 2026;
  fields.__set(Parts::__year);
  assert(std::chrono::__try_get_year(fields, parsed_year));
  assert(parsed_year == 2026);
  assert(fields.__year_ == 2026);
  fields.__century_ = 19;
  assert(!std::chrono::__try_get_year(fields, parsed_year));
  assert(parsed_year == 2026);
  assert(fields.__year_ == 2026);

  fields                    = {};
  fields.__year_of_century_ = 68;
  fields.__set(Parts::__year_of_century);
  check(fields, 2068y);
  fields.__year_of_century_ = 69;
  check(fields, 1969y);
  fields.__year_of_century_ = 100;
  check(fields, year{1}, false);
}

void test_date() {
  Fields fields;
  fields.__century_         = 20;
  fields.__year_of_century_ = 26;
  fields.__day_of_year_     = 32;
  fields.__month_           = 2;
  fields.__day_             = 1;
  fields.__set(Parts::__century | Parts::__year_of_century | Parts::__day_of_year | Parts::__month | Parts::__day);
  const year_month_day expected = 2026y / February / 1;
  check(fields, expected);
  check(fields, sys_days{expected});
  check(fields, local_days{sys_days{expected}.time_since_epoch()});
  assert(fields.__year_ == 0);
  assert(!fields.__has(Parts::__year));

  fields.__month_ = 3;
  check(fields, expected, false);
  fields.__month_ = 2;
  fields.__day_   = 2;
  check(fields, expected, false);

  fields             = {};
  fields.__iso_year_ = 2020;
  fields.__iso_week_ = 53;
  fields.__weekday_  = 5;
  fields.__year_     = 2021;
  fields.__set(Parts::__iso_year | Parts::__iso_week | Parts::__weekday | Parts::__year);
  check(fields, 2021y / January / 1);
  fields.__year_ = 2020;
  check(fields, 2021y / January / 1, false);
}

void test_hour() {
  for (bool is_pm : {false, true}) {
    Fields fields;
    fields.__is_pm_ = is_pm;
    fields.__set(Parts::__hour12 | Parts::__am_pm);
    for (int hour = 1; hour <= 12; ++hour) {
      fields.__hour12_   = hour;
      const int expected = (hour == 12 ? 0 : hour) + (is_pm ? 12 : 0);
      int result         = -1;
      assert(std::chrono::__try_get_hour(fields, 23, result));
      assert(result == expected);
      check(fields, hours{expected});
    }
    for (int hour : {std::numeric_limits<int>::min(), -1, 0, 13, std::numeric_limits<int>::max()}) {
      fields.__hour12_ = hour;
      int result       = -1;
      assert(!std::chrono::__try_get_hour(fields, 23, result));
      assert(result == -1);
      check(fields, 0h, false);
    }
  }

  Fields fields;
  fields.__hour12_ = 1;
  fields.__is_pm_  = true;
  fields.__set(Parts::__hour12 | Parts::__am_pm);
  int result = -1;
  assert(std::chrono::__try_get_hour(fields, 23, result));
  assert(result == 13);
  check(fields, 13h);
  assert(fields.__hours_ == 0);
  assert(!fields.__has(Parts::__hours));

  fields.__hours_ = 12;
  fields.__set(Parts::__hours);
  result = -1;
  assert(!std::chrono::__try_get_hour(fields, 23, result));
  assert(result == -1);
  check(fields, 13h, false);
  assert(fields.__hours_ == 12);

  fields.__hours_ = 13;
  check(fields, 13h);
  fields.__year_  = 2026;
  fields.__month_ = 7;
  fields.__day_   = 20;
  fields.__set(Parts::__year | Parts::__month | Parts::__day);
  const sys_seconds expected = sys_days{2026y / July / 20} + 13h;
  check(fields, expected);
  check(fields, local_seconds{expected.time_since_epoch()});
  check(fields, file_clock::from_sys(expected));
  fields.__hours_ = 12;
  check(fields, expected, false);
  check(fields, local_seconds{expected.time_since_epoch()}, false);
  check(fields, file_clock::from_sys(expected), false);

  fields          = {};
  fields.__hours_ = 25;
  fields.__set(Parts::__hours);
  check(fields, 25h);
  assert(!std::chrono::__try_get_hour(fields, 23, result));
  assert(result == -1);
  fields.__minutes_ = 90;
  fields.__seconds_ = 90;
  fields.__set(Parts::__minutes | Parts::__seconds);
  check(fields, 25h + 90min + 90s);
  assert(!std::chrono::__validate_minute(fields, 59));
  assert(!std::chrono::__validate_second(fields, 59));

  fields = {};
  assert(std::chrono::__try_get_hour(fields, 23, result));
  assert(result == 0);

  fields.__hour12_ = 12;
  fields.__set(Parts::__hour12);
  result = -1;
  assert(!std::chrono::__try_get_hour(fields, 23, result));
  assert(result == -1);
  check(fields, 0h, false);

  fields.__set(Parts::__hours);
  for (int hour = 0; hour <= 23; ++hour) {
    fields.__hours_  = hour;
    fields.__hour12_ = hour == 0 ? 12 : (hour > 12 ? hour - 12 : hour);
    assert(std::chrono::__try_get_hour(fields, 23, result));
    assert(result == hour);
    check(fields, hours{hour});

    fields.__hour12_ = fields.__hour12_ == 12 ? 1 : fields.__hour12_ + 1;
    result           = -1;
    assert(!std::chrono::__try_get_hour(fields, 23, result));
    assert(result == -1);
    check(fields, 0h, false);
  }
  fields.__hours_  = 25;
  fields.__hour12_ = 1;
  check(fields, 0h, false);

  fields          = {};
  fields.__hours_ = 13;
  fields.__set(Parts::__hours | Parts::__am_pm);
  result = -1;
  assert(!std::chrono::__try_get_hour(fields, 23, result)); // 13 conflicts with AM.
  assert(result == -1);
  fields.__is_pm_ = true;
  assert(std::chrono::__try_get_hour(fields, 23, result));
  assert(result == 13);
}

void test_try_get_date() {
  const sys_days initial{2000y / January / 1};
  auto check_candidate = [&](const Fields& fields, sys_days expected, bool consistent) {
    const Fields snapshot = fields;
    sys_days result       = initial;
    assert(std::chrono::__try_get_date(snapshot, result) == consistent);
    assert(result == (consistent ? expected : initial));
    assert(snapshot.__present_ == fields.__present_);
    assert(snapshot.__year_ == fields.__year_);
    assert(snapshot.__month_ == fields.__month_);
    assert(snapshot.__day_ == fields.__day_);
    if (consistent) {
      const year_month_day ymd{expected};
      assert(std::chrono::__validate_date(snapshot, ymd));
    }
  };

  Fields fields;
  fields.__year_        = 2026;
  fields.__month_       = 2;
  fields.__day_         = 1;
  fields.__day_of_year_ = 33;
  fields.__set(Parts::__year | Parts::__month | Parts::__day | Parts::__day_of_year);
  // The calendar date supplies the candidate; the contradictory ordinal date
  // must be rejected by validation, not by computing a second candidate.
  check_candidate(fields, sys_days{2026y / February / 1}, false);
  fields.__day_of_year_ = 32;
  check_candidate(fields, sys_days{2026y / February / 1}, true);

  fields.__century_         = 20;
  fields.__year_of_century_ = 26;
  fields.__year_            = 2025;
  fields.__set(Parts::__century | Parts::__year_of_century);
  // Reject contradictory year fields before using them to derive a date.
  check_candidate(fields, sys_days{2026y / February / 1}, false);

  fields             = {};
  fields.__iso_year_ = 2020;
  fields.__iso_week_ = 53;
  fields.__weekday_  = 5;
  fields.__year_     = 2020;
  fields.__set(Parts::__iso_year | Parts::__iso_week | Parts::__weekday | Parts::__year);
  check_candidate(fields, sys_days{2021y / January / 1}, false);
  fields.__year_ = 2021;
  check_candidate(fields, sys_days{2021y / January / 1}, true);
  // ISO can supply the candidate, but validation must still reject an invalid year.
  fields.__year_ = std::numeric_limits<int>::max();
  check_candidate(fields, sys_days{2021y / January / 1}, false);
  fields.__present_ = Parts::__iso_year | Parts::__iso_week | Parts::__weekday;
  check_candidate(fields, sys_days{2021y / January / 1}, true);

  fields                    = {};
  fields.__century_         = 20;
  fields.__year_of_century_ = 26;
  fields.__day_of_year_     = 32;
  fields.__set(Parts::__century | Parts::__year_of_century | Parts::__day_of_year);
  check_candidate(fields, sys_days{2026y / February / 1}, true);
  fields.__month_ = 3;
  fields.__set(Parts::__month);
  check_candidate(fields, sys_days{2026y / February / 1}, false);

  // %y still implies a century even when ISO fields supply the candidate.
  fields                    = {};
  fields.__year_of_century_ = 21;
  fields.__iso_year_        = 2020;
  fields.__iso_week_        = 53;
  fields.__weekday_         = 5;
  fields.__set(Parts::__year_of_century | Parts::__iso_year | Parts::__iso_week | Parts::__weekday);
  check_candidate(fields, sys_days{2021y / January / 1}, true);
  fields.__year_of_century_ = 20;
  check_candidate(fields, sys_days{2021y / January / 1}, false);

  fields                = {};
  fields.__year_        = 2026;
  fields.__day_of_year_ = 32;
  fields.__month_       = 3;
  fields.__set(Parts::__year | Parts::__day_of_year | Parts::__month);
  check_candidate(fields, sys_days{2026y / February / 1}, false);

  fields             = {};
  fields.__year_     = 2026;
  fields.__week_sun_ = 5;
  fields.__week_mon_ = 5;
  fields.__weekday_  = 0;
  fields.__set(Parts::__year | Parts::__week_sun | Parts::__week_mon | Parts::__weekday);
  check_candidate(fields, sys_days{2026y / February / 1}, false);
  fields.__week_mon_ = 4;
  check_candidate(fields, sys_days{2026y / February / 1}, true);
  fields.__present_ = Parts::__year | Parts::__week_mon | Parts::__weekday;
  check_candidate(fields, sys_days{2026y / February / 1}, true);

  // No complete date and invalid values must leave the output unchanged.
  fields.__present_ = Parts::__year;
  sys_days result   = initial;
  assert(!std::chrono::__try_get_date(fields, result));
  assert(result == initial);
  fields.__month_ = 258; // Must not narrow to February.
  fields.__day_   = 1;
  fields.__set(Parts::__month | Parts::__day);
  assert(!std::chrono::__try_get_date(fields, result));
  assert(result == initial);
}

void test_validate_year_fields() {
  for (int y : {-32767, -2000, -1976, -1, 0, 1968, 1969, 1999, 2000, 2068, 2069, 32767}) {
    const year_month_day date = year{y} / July / 1;
    Fields fields;
    assert(std::chrono::__validate_date(fields, date));

    fields.__year_of_century_ = (y < 0 ? -y : y) % 100;
    fields.__set(Parts::__year_of_century);
    assert(std::chrono::__validate_date(fields, date) == (1969 <= y && y <= 2068));

    fields.__century_ = y / 100 - (y % 100 < 0);
    fields.__set(Parts::__century);
    assert(std::chrono::__validate_date(fields, date));
    ++fields.__century_;
    assert(!std::chrono::__validate_date(fields, date));
    --fields.__century_;

    fields.__year_ = y;
    fields.__set(Parts::__year);
    assert(std::chrono::__validate_date(fields, date));
    ++fields.__year_;
    assert(!std::chrono::__validate_date(fields, date));
    --fields.__year_;

    ++fields.__year_of_century_;
    assert(!std::chrono::__validate_date(fields, date));

    // A standalone century constrains the candidate even without %Y or %y.
    fields.__present_ = Parts::__century;
    assert(std::chrono::__validate_date(fields, date));
    ++fields.__century_;
    assert(!std::chrono::__validate_date(fields, date));
  }
}

void test_make_date() {
  const year_month_day expected = 2021y / January / 1;
  Fields fields;
  fields.__year_        = 2021;
  fields.__month_       = 1;
  fields.__day_         = 1;
  fields.__day_of_year_ = 1;
  fields.__week_sun_    = 0;
  fields.__week_mon_    = 0;
  fields.__iso_year_    = 2020;
  fields.__iso_week_    = 53;
  fields.__weekday_     = 5;
  fields.__set(Parts::__year | Parts::__month | Parts::__day | Parts::__day_of_year | Parts::__week_sun |
               Parts::__week_mon | Parts::__iso_year | Parts::__iso_week | Parts::__weekday);

  auto check = [&] {
    year_month_day result{};
    assert(std::chrono::__make_date(fields, result));
    assert(result == expected);
    assert(std::chrono::__validate_date(fields, result));
  };

  // Exercise each complete representation while retaining redundant constraints.
  check();
  fields.__present_ = fields.__present_ & ~(Parts::__month | Parts::__day);
  check();
  fields.__present_ = fields.__present_ & ~Parts::__day_of_year;
  check();
  fields.__present_ = fields.__present_ & ~Parts::__week_sun;
  check();
  fields.__present_ = fields.__present_ & ~Parts::__week_mon;
  check();
}

int main(int, char**) {
  static_assert(test_in_range());
  test_in_range();
  static_assert(test_field_queries());
  test_field_queries();
  test_year();
  test_date();
  test_hour();
  test_try_get_date();
  test_validate_year_fields();
  test_make_date();
  return 0;
}

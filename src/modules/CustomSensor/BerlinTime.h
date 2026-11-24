#pragma once

// Europe/Berlin local time from UTC, without touching the global TZ setting.
// Header-only and free of firmware dependencies, so it can be tested on a PC.

#include <stdint.h>

namespace berlintime
{

/// Days since 1970-01-01 for a civil date (proleptic Gregorian). Algorithm by Howard Hinnant.
inline int32_t daysFromCivil(int32_t y, uint32_t m, uint32_t d)
{
    y -= m <= 2;
    const int32_t era = (y >= 0 ? y : y - 399) / 400;
    const uint32_t yoe = (uint32_t)(y - era * 400);
    const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int32_t)doe - 719468;
}

/// Year of a day count since 1970-01-01 (inverse of daysFromCivil, year only).
inline int32_t yearFromDays(int32_t z)
{
    z += 719468;
    const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    const uint32_t doe = (uint32_t)(z - era * 146097);
    const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const uint32_t mp = (5 * doy + 2) / 153;
    const uint32_t m = mp < 10 ? mp + 3 : mp - 9;
    return (int32_t)yoe + era * 400 + (m <= 2);
}

/// Day count of the last Sunday on or before the given day count (1970-01-01 was a Thursday).
inline int32_t lastSundayOnOrBefore(int32_t days)
{
    int32_t weekday = (days + 4) % 7; // 0 = Sunday
    return days - weekday;
}

/// Offset of Europe/Berlin from UTC in seconds: 7200 (CEST) from the last Sunday in March 01:00 UTC
/// to the last Sunday in October 01:00 UTC, otherwise 3600 (CET).
inline int32_t berlinOffsetSecs(uint32_t utc)
{
    int32_t year = yearFromDays((int32_t)(utc / 86400));
    int64_t start = (int64_t)lastSundayOnOrBefore(daysFromCivil(year, 3, 31)) * 86400 + 3600;
    int64_t end = (int64_t)lastSundayOnOrBefore(daysFromCivil(year, 10, 31)) * 86400 + 3600;
    return ((int64_t)utc >= start && (int64_t)utc < end) ? 7200 : 3600;
}

} // namespace berlintime

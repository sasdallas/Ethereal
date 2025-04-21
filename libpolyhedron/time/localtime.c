/**
 * @file libpolyhedron/time/localtime.c
 * @brief Local time functions
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <time.h>
#include <sys/time.h>
#include <stdbool.h>



static bool is_year_leap(int year) {
    return ((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0)));
}

static long get_days_in_month(int month, int year) {
    switch (month) {
        case 1:
            return 31; // January
        case 2:
            return is_year_leap(year) ? 29 : 28; // February
        case 3:
            return 31; // March
        case 4:
            return 30; // April
        case 5:
            return 31; // May
        case 6:
            return 30; // June
        case 7:
            return 31; // July
        case 8:
            return 31; // August
        case 9:
            return 30; // September
        case 10:
            return 31; // October
        case 11:
            return 30; // November
        case 12:
            return 31; // December
    }

    return 0; // ??? 
}

static int get_day_of_week(long seconds) {
    long day = seconds / 86400; // 86400 seconds in a day
    day += 4;                   // starts at Thursday
    return day % 7;
}

static long get_seconds_of_months(int months, int year) {
    long days = 0;
    for (int i = 1; i < months; i++) {
        // tremble in fear at the non-zero index
        days += get_days_in_month(i, year);
    }

    return days * 86400;
}

static unsigned int get_seconds_of_years(int years) {
    unsigned int days = 0;
    while (years > 1969) {
        days += 365;
        if (is_year_leap(years)) days++; // Account for leap year
        years--;
    }

    return days * 86400;
}


static struct tm *fill_time(const time_t *time_ptr, struct tm *tm, const char *tzName, int tzOffset) {
    time_t timeValue = *time_ptr + tzOffset; // tzOffset should be 0 because we don't actually have timezone support yet
    tm->_tm_zone_name = tzName;
    tm->_tm_zone_offset = tzOffset;

    long seconds = timeValue < 0 ? -2208988800L : 0;
    long year_sec = 0;

    int startingyear = timeValue < 0 ? 1900 : 1970;

    for (int year = startingyear; year < 2100; year++) {
        long added = is_year_leap(year) ? 366 : 365;
        long secs = added * 86400;

        if (seconds + secs > timeValue) {
            tm->tm_year = year - 1900;
            year_sec = seconds;
            for (int month = 1; month <= 12; month++) {
                secs = get_days_in_month(month, year) * 86400;
                if (seconds + secs > timeValue) {
                    tm->tm_mon = month - 1;
                    for (int day = 1; day <= get_days_in_month(month, year); day++) {
                        secs = 60 * 60 * 24;
                        if (seconds  + secs > timeValue) {
                            tm->tm_mday = day;
                            for (int hour = 1; hour <= 24; hour++) {
                                secs = 60 * 60;
                                if (seconds + secs > timeValue) {
                                    long remaining = timeValue - seconds;
                                    tm->tm_hour = hour - 1;
                                    tm->tm_min = remaining / 60;
                                    tm->tm_sec = remaining % 60;
                                    tm->tm_wday = get_day_of_week(timeValue);
                                    tm->tm_yday = (timeValue - year_sec) / 86400;
                                    tm->tm_isdst = 0;
                                    return tm;
                                } else {
                                    seconds += secs;
                                }
                            }
                            return NULL;
                        } else {
                            seconds += secs;
                        }
                    }
                    return NULL;
                } else {
                    seconds += secs;
                }
            }
            return NULL;
        } else {
            seconds += secs;
        }
    }

    return (void*)0;
}

// Tiny value for helper (TODO: uhhhh)
static struct tm _timevalue;

struct tm *localtime_r(const time_t *ptr, struct tm *time_val) {
    return fill_time(ptr, time_val, "UTC", 0);
}

struct tm *gmtime_r(const time_t *ptr, struct tm *time_val) {
    return fill_time(ptr, time_val, "UTC", 0);
}

struct tm *localtime(const time_t *ptr) {
    return fill_time(ptr, &_timevalue, "UTC", 0); // BAD: No timezone support.
}

struct tm *gmtime(const time_t *ptr) {
    return fill_time(ptr, &_timevalue, "UTC", 0);
}

time_t mktime(struct tm *tm) {
    return 
        (get_seconds_of_years(tm->tm_year + 1899)) +
        (get_seconds_of_months(tm->tm_mon + 1, tm->tm_year + 1900)) +
        (tm->tm_mday - 1) * 86400 + 
        (tm->tm_hour) * 3600 +
        (tm->tm_min) * 60 +
        (tm->tm_sec) - tm->_tm_zone_offset;
}
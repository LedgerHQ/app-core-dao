#include "time_helper.h"
#include <stdio.h>

static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void timestamp_to_date(unsigned long timestamp, datetime_t *dt) {
  int days = timestamp / 86400; // Days since epoch
  int seconds_remaining = timestamp % 86400;
  dt->hour = seconds_remaining / 3600;
  dt->minute = (seconds_remaining % 3600) / 60;
  dt->second = seconds_remaining % 60;

  dt->year = 1970; // Start from the epoch year
  while (days > 365) {
    if ((dt->year % 4 == 0 && dt->year % 100 != 0) || dt->year % 400 == 0) { // Leap year check
      days -= 366;
    } else {
      days -= 365;
    }
    (dt->year)++;
  }

  dt->month = 1; // Start from January
  while (days > days_in_month[dt->month - 1]) {
    days -= days_in_month[dt->month - 1];
    (dt->month)++;
  }

  dt->day = days + 1; // Add 1 to account for starting from day 1
}

void timestamp_to_string(unsigned long timestamp, char str[static DATETIME_STR_LEN]) {
  datetime_t dt;
  timestamp_to_date(timestamp, &dt);
  snprintf(str, DATETIME_STR_LEN, "%04d-%02d-%02d %02d:%02d:%02d", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
}
#ifndef RTC_H
#define RTC_H

#include <stdint.h>

void rtc_init(void);
uint8_t rtc_ready(void);
uint32_t rtc_days_since_year0(void);
uint32_t rtc_seconds_of_day(void);
void rtc_set_timezone_minutes(int16_t minutes);
int16_t rtc_timezone_minutes(void);

#endif

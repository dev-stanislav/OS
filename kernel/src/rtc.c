#include "rtc.h"
#include "io.h"

#define CMOS_INDEX 0x70
#define CMOS_DATA 0x71

static uint8_t ready;
static uint32_t days_since_year0;
static uint32_t seconds_of_day;
static int16_t timezone_minutes;

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_INDEX, (uint8_t)(0x80 | reg));
    return inb(CMOS_DATA);
}

static uint8_t bcd_to_bin(uint8_t value) {
    return (uint8_t)((value & 0x0F) + ((value >> 4) * 10));
}

static uint8_t is_leap(uint32_t year) {
    return (year % 4u == 0 && year % 100u != 0) || (year % 400u == 0);
}

static uint32_t days_before_year(uint32_t year) {
    return 365u * year + (year + 3u) / 4u - (year + 99u) / 100u + (year + 399u) / 400u;
}

static uint32_t days_before_month(uint32_t year, uint8_t month) {
    static const uint16_t offsets[] = {0,31,59,90,120,151,181,212,243,273,304,334};
    uint32_t days = offsets[month - 1];
    if (month > 2 && is_leap(year)) days++;
    return days;
}

void rtc_init(void) {
    ready = 0;
    timezone_minutes = 0;
    for (uint32_t wait = 0; wait < 1000000; wait++) if ((cmos_read(0x0A) & 0x80) == 0) break;
    uint8_t second = cmos_read(0x00);
    uint8_t minute = cmos_read(0x02);
    uint8_t hour = cmos_read(0x04);
    uint8_t day = cmos_read(0x07);
    uint8_t month = cmos_read(0x08);
    uint8_t year = cmos_read(0x09);
    uint8_t century = cmos_read(0x32);
    uint8_t status_b = cmos_read(0x0B);
    if ((status_b & 0x04) == 0) {
        second = bcd_to_bin(second);
        minute = bcd_to_bin(minute);
        hour = (uint8_t)((hour & 0x80) | bcd_to_bin((uint8_t)(hour & 0x7F)));
        day = bcd_to_bin(day);
        month = bcd_to_bin(month);
        year = bcd_to_bin(year);
        century = bcd_to_bin(century);
    }
    if ((status_b & 0x02) == 0 && (hour & 0x80)) hour = (uint8_t)(((hour & 0x7F) + 12) % 24);
    uint32_t full_year = century ? (uint32_t)century * 100u + year : 2000u + year;
    if (full_year < 2020u || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) return;
    days_since_year0 = days_before_year(full_year) + days_before_month(full_year, month) + (uint32_t)(day - 1);
    seconds_of_day = (uint32_t)hour * 3600u + (uint32_t)minute * 60u + second;
    ready = 1;
}

uint8_t rtc_ready(void) { return ready; }
uint32_t rtc_days_since_year0(void) { return days_since_year0; }
uint32_t rtc_seconds_of_day(void) { return seconds_of_day; }
void rtc_set_timezone_minutes(int16_t minutes) { timezone_minutes = minutes; }
int16_t rtc_timezone_minutes(void) { return timezone_minutes; }

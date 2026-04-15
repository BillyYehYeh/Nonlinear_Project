#include <stdio.h>
#include <time.h>
#include <svdpi.h>

const char *get_local_datetime(void) {
  static char datetime_buf[64];
  time_t now;
  struct tm tm_now;

  now = time(NULL);
  if (localtime_r(&now, &tm_now) == NULL) {
    snprintf(datetime_buf, sizeof(datetime_buf), "UNKNOWN_DATETIME");
    return datetime_buf;
  }

  if (strftime(datetime_buf, sizeof(datetime_buf), "%Y-%m-%d %H:%M:%S %Z", &tm_now) == 0) {
    snprintf(datetime_buf, sizeof(datetime_buf), "UNKNOWN_DATETIME");
  }

  return datetime_buf;
}

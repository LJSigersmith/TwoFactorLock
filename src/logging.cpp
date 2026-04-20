#include <Arduino.h>
#include <string>
#include <RTClib.h>
#include <SdFat.h>

using namespace std;

RTC_PCF8563 g_rtc;
static FsFile g_log_file;

#define LOG_FILENAME "unlock_log.txt"

/**
 * @brief Get current time as String
 *
 * @param pretty true for dot notation (looks better on display)
 * true: MM.DD.YY HH:MM:SS
 * false: YY:MM:DD:HH:MM:SS
 * @return String
 */
String getTimestamp(bool pretty) {
    DateTime now = g_rtc.now();
    char buf[23];
    if (!pretty) {
      sprintf(buf, "%02d:%02d:%02d:%02d:%02d:%02d",
          now.year() % 100,
          now.month(),
          now.day(),
          now.hour(),
          now.minute(),
          now.second()
      );
    } else {
      sprintf(buf, "%02d.%02d.%02d %02d:%02d:%02d",
          now.month(),
          now.day(),
          now.year() % 100,
          now.hour(),
          now.minute(),
          now.second()
      );
    }
    return String(buf);
}

void logging_init(SdFat& sd) {
    // Init RTC
    if (!g_rtc.begin()) { Serial.println("RTC not found!"); }
    g_rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Set every time we build

    // Clear and recreate log file
    sd.remove(LOG_FILENAME);
    g_log_file = sd.open(LOG_FILENAME, O_WRONLY | O_CREAT | O_APPEND);
}

/**
 * @brief Log event to logfile in format: YY:MM:DD:HH:MM:SS,STAGE,RESULT,DETAIL
 *
 * @param stage Stage string from enum indicating what stage we're in (i.e. KEYWORD_CORRECT)
 * @param result The result of the stage
 * @param detail Any details on the event
 */
void log_event_to_file(String stage, String result, String detail) {

    String to_log = getTimestamp(false) + "," + stage + "," + result + "," + detail + "\n";
    g_log_file.print(to_log);
    g_log_file.flush();

}
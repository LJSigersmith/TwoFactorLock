#include <Arduino.h>
#include <string>

using namespace std;

/**
 * @brief Log event to logfile in format: YY:MM:DD:HH:MM:SS,STAGE,RESULT,DETAIL
 * 
 * @param stage Stage string from enum indicating what stage we're in (i.e. KEYWORD_CORRECT)
 * @param result The result of the stage
 * @param detail Any details on the event
 */
void log_event_to_file(string logfile, string stage, string result, string detail) {

}
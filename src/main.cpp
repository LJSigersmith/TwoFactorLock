#include <Arduino.h>
#include <map>
#include <string>

#include <SdFat.h>

#include "display.h"
#include "logging.h"

#define CODE_FILENAME "lock_config.txt"
#define SD_CARD_PIN D2
SdFat g_sd;

/* –– Track State ––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––– */
enum PROGRAM_STAGE {
  START,
  WAITING_FOR_CODE,
  CODE_ENTERED,
  CODE_CORRECT,
  CODE_INCORRECT,
  WAITING_FOR_KEYWORD,
  KEYWORD_CORRECT,
  KEYWORD_INCORRECT,
  UNLOCKED
};
PROGRAM_STAGE g_current_stage = START;

/* –– Globals ––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––– */
std::map<int, std::string> g_keyword_map = {
  {1, "blue"},
  {2, "cyan"},
  {3, "green"},
  {4, "magenta"},
  {5, "red"},
  {6, "white"},
  {7, "yellow"}
};
int keyword_int = 0;
int unlock_code = 0;

/* –– Forward Declarations ––––––––––––––––––––––––––––––––––––––––––––––––––––– */
void start();
void waiting_for_code();
void read_code();
void check_code();
void code_incorrect();
void code_correct();
void waiting_for_keyword();
void keyword_correct();
void keyword_incorrect();
void unlocked();

using namespace std;

/* –– Setup & Loop –––––––––––––––––––––––––––––––––––––––––––––––––––––––––––– */
void setup() {

  // Init SD
  if (!g_sd.begin(SD_CARD_PIN)) { Serial.println("SD initialization failed!"); }

  // Init logging
  logging_init(g_sd);

  // Init microphone

  // Init IR stuff

  // Init display
  display_init();

  // Read code from lock_config.txt
  read_code();

}

/*

  Need to log every event: YY:MM:DD:HH:MM:SS,STAGE,RESULT,DETAIL
  Log to unlock_log.txt
  Display needs to be updated at every stage
  Buzzer for successful/unsuccessful attempts

*/
void loop() {
  switch (g_current_stage) {
    case START:
      start();
    case WAITING_FOR_CODE:
      waiting_for_code();
      break;
    case CODE_ENTERED :
      check_code();
      break;
    case CODE_INCORRECT :
      code_incorrect();
      break;
    case CODE_CORRECT :
      code_correct();
      break;
    case WAITING_FOR_KEYWORD :
      waiting_for_keyword();
      break;
    case KEYWORD_CORRECT :
      keyword_correct();
      break;
    case KEYWORD_INCORRECT :
      keyword_incorrect();
      break;
    case UNLOCKED :
      unlocked();
    default :
      break;
  }
}

void read_code() {
  FsFile code_file = g_sd.open(CODE_FILENAME, O_WRONLY | O_CREAT | O_APPEND);
  // TODO: Read code in to unlock_code
}

void start() {
  // Display LOCK
  display_lock_screen();
  
  g_current_stage = WAITING_FOR_CODE;
}

void waiting_for_code() {
  // Wait for IR input and process
  // Should be showing code as typed on screen

  // Once four digits have been entered (no two of the same digit back to back)
  g_current_stage = CODE_ENTERED;
}

void check_code() {
  // Correct code:
  g_current_stage = CODE_CORRECT;

  // Incorrect code:
  g_current_stage = CODE_INCORRECT;
}

void code_incorrect() {

  // Reset stage
  g_current_stage = WAITING_FOR_CODE;
}

void code_correct() {

  // Generate random int (1-7) and set voice keyword

  // Display number for keyword

  // Move to next stage
  g_current_stage = WAITING_FOR_KEYWORD;
}

void waiting_for_keyword() {
  // Should be showing keyword int
  // Process audio input, check against expected keyword

  // If correct keyword spoken
  g_current_stage = KEYWORD_CORRECT;

  // If incorrect keyword spoken (or timeout?)
  g_current_stage = KEYWORD_INCORRECT;

}

void keyword_incorrect() {

  // Reset stage
  g_current_stage = WAITING_FOR_KEYWORD;
}

void keyword_correct() {
  // Unlock
  g_current_stage = UNLOCKED;
}

void unlocked() {
  // Show unlocked?
}
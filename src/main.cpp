#include <Arduino.h>
#include <map>

// put function declarations here:
int myFunction(int, int);

void setup() {
  // Write code to lock_config.txt

  // Clear and recreate log file unlock_log.txt
}

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
PROGRAM_STAGE current_stage = START;
int unlock_code = 0;

int keyword_int = 0;
std::map<int, std::string> keywordMap = {
  {1, "blue"},
  {2, "cyan"},
  {3, "green"},
  {4, "magenta"},
  {5, "red"},
  {6, "white"},
  {7, "yellow"}
};

void start() {
  // Read code from SD card (4 digits) (lock_config.txt) -> unlock_code
  
  // Display LOCK
  
  current_stage = WAITING_FOR_CODE;
}

void waiting_for_code() {
  // Wait for IR input and process
  // Should be showing code as typed on screen

  // Once four digits have been entered (no two of the same digit back to back)
  current_stage = CODE_ENTERED;
}

void check_code() {
  // Correct code:
  current_stage = CODE_CORRECT;

  // Incorrect code:
  current_stage = CODE_INCORRECT;
}

void code_incorrect() {

  // Reset stage
  current_stage = WAITING_FOR_CODE;
}

void code_correct() {

  // Generate random int (1-7) and set voice keyword

  // Display number for keyword

  // Move to next stage
  current_stage = WAITING_FOR_KEYWORD;
}

void waiting_for_keyword() {
  // Should be showing keyword int
  // Process audio input, check against expected keyword

  // If correct keyword spoken
  current_stage = KEYWORD_CORRECT;

  // If incorrect keyword spoken (or timeout?)
  current_stage = KEYWORD_INCORRECT;

}

void keyword_incorrect() {

  // Reset stage
  current_stage = WAITING_FOR_KEYWORD;
}

void keyword_correct() {
  // Unlock
  current_stage = UNLOCKED;
}

void unlocked() {
  // Show unlocked?
}

/*

Need to log every event: YY:MM:DD:HH:MM:SS,STAGE,RESULT,DETAIL
Log to unlock_log.txt
Display needs to be updated at every stage
Buzzer for successful/unsuccessful attempts

*/
void loop() {
  switch (current_stage) {
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
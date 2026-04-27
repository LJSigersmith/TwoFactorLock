#include <Arduino.h>
#include <IRremote.hpp>
#include <map>
#include <string>
#include <array>

#include <SdFat.h>

#include "display.h"
#include "logging.h"

#define CODE_FILENAME "lock_config.txt"
#define SD_CARD_PIN D2
const byte IR_RECEIVE_PIN = A0;
SdFat g_sd;
bool g_sd_available = false;

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
const std::map<uint8_t, int> remoteDigits = 
    {{0x16, 0},
    {0x0C, 1},
    {0x18, 2},
    {0x5E, 3},
    {0x08, 4},
    {0x1C, 5},
    {0x5A, 6},
    {0x42, 7},
    {0x52, 8},
    {0x4A, 9}};
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
void CapturePassword(int currDigit);
void StartColorChallenge();

int HexToInt(uint8_t hexCode);
void SaveToSDCard(const String &data);
void DeleteCodeFile();
bool DoesFileExist();
bool HasStoredPassword();
bool IsPasswordComplete();
void ResetPasswordCapture();
std::array<int, 4> FilePassword();
bool PasswordsMatch(const std::array<int, 4> &storedPassword);
std::array<String, 2> GenerateRandomColor();
String PasswordToString(const int digits[4]);
String PasswordToString(const std::array<int, 4> &digits);


int passwordDigits[4] = {-1, -1, -1, -1}; // Array to hold the 4 digits of the password
int currentDigitIndex = 0;                // Index to track which digit is being entered
bool capturingPassword = false;
int expectedColorDigit = -1;
bool isAppLocked = true;

using namespace std;


/* –– Setup & Loop –––––––––––––––––––––––––––––––––––––––––––––––––––––––––––– */
void setup() {
  Serial.begin(115200);

  // Init SD
  g_sd_available = g_sd.begin(SD_CARD_PIN);
  if (!g_sd_available) { Serial.println("SD initialization failed!"); }

  // Init logging
  logging_init(g_sd);

  // Init microphone

  // Init IR stuff
  Serial.println(F("Enabling IRin"));
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  Serial.print(F("Ready to receive IR signals at pin "));
  Serial.println(IR_RECEIVE_PIN);

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
      break;
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
      break;
    default :
      break;
  }
}

void read_code() {
  if (HasStoredPassword()) {
    Serial.println("Existing password found on SD card.");
  } else {
    Serial.println("No existing password found on SD card.");
  }
}

void start() {
  // Display LOCK
  display_lock_screen();
  ResetPasswordCapture();
  capturingPassword = true;
  Serial.println(HasStoredPassword() ? "Enter stored password." : "Record a new password.");
  
  g_current_stage = WAITING_FOR_CODE;
}

void waiting_for_code() {
  if (!IrReceiver.decode()) {
    return;
  }

  int decimalValue = HexToInt(IrReceiver.decodedIRData.command);

  if (capturingPassword && decimalValue >= 0 && decimalValue <= 9) {
    Serial.println("Capturing password...");
    CapturePassword(decimalValue);

    if (IsPasswordComplete()) {
      g_current_stage = CODE_ENTERED;
    }
  }

  Serial.println("Decoded: " + String(decimalValue));
  delay(100);
  IrReceiver.resume();
}

void check_code() {
  const bool hasStoredPassword = HasStoredPassword();
  const String capturedPassword = PasswordToString(passwordDigits);
  Serial.println("Captured: " + capturedPassword);

  if (!hasStoredPassword) {
    SaveToSDCard("CODE: " + capturedPassword);
    isAppLocked = false;
    ResetPasswordCapture();
    Serial.println("New password saved.");
    g_current_stage = UNLOCKED;
    return;
  }

  const auto storedPassword = FilePassword();
  if (PasswordsMatch(storedPassword)) {
    ResetPasswordCapture();
    g_current_stage = CODE_CORRECT;
  } else {
    ResetPasswordCapture();
    g_current_stage = CODE_INCORRECT;
  }
}

void code_incorrect() {
  isAppLocked = true;
  Serial.println("Password mismatch.");
  display_lock_screen();
  ResetPasswordCapture();
  capturingPassword = true;
  Serial.println("Enter stored password.");
  g_current_stage = WAITING_FOR_CODE;
}

void code_correct() {
  StartColorChallenge();

  // Move to next stage
  g_current_stage = WAITING_FOR_KEYWORD;
}

void waiting_for_keyword() {
  if (!IrReceiver.decode()) {
    return;
  }

  int decimalValue = HexToInt(IrReceiver.decodedIRData.command);

  if (decimalValue >= 0 && decimalValue <= 9) {
    if (decimalValue == expectedColorDigit) {
      g_current_stage = KEYWORD_CORRECT;
    } else {
      g_current_stage = KEYWORD_INCORRECT;
    }
  } else {
    Serial.println("Waiting for color response digit.");
  }

  Serial.println("Decoded: " + String(decimalValue));
  delay(100);
  IrReceiver.resume();
}

void keyword_incorrect() {
  isAppLocked = true;
  expectedColorDigit = -1;
  Serial.println("Color mismatch. App remains locked.");
  display_lock_screen();
  ResetPasswordCapture();
  capturingPassword = true;
  Serial.println("Enter stored password.");
  g_current_stage = WAITING_FOR_CODE;
}

void keyword_correct() {
  // Unlock
  isAppLocked = false;
  expectedColorDigit = -1;
  Serial.println("Color accepted. App unlocked.");
  g_current_stage = UNLOCKED;
}

void unlocked() {
  display_unlock_screen();
  // App remains unlocked until reset
}

int HexToInt(uint8_t hexCode)
{
    auto it = remoteDigits.find(hexCode);
    return (it != remoteDigits.end()) ? it->second : hexCode;
}

void CapturePassword(int currDigit)
{
    if (currDigit < 0 || currDigit > 9)
    {
        return;
    }

    if (currentDigitIndex < 4)
    {
        passwordDigits[currentDigitIndex] = currDigit;
        currentDigitIndex++;
        
        //Convert Digits to "*" for display
        String displayPassword = "";
        for (int i = 0; i < currentDigitIndex; i++)
        {
            displayPassword += "*";
        }
        Serial.println("Captured digit: " + String(currDigit) + " | Display: " + displayPassword);
    }
}
bool DoesFileExist()
{
    if (!g_sd_available)
    {
        Serial.println("SD card not available; cannot check for file.");
        return false;
    }
    return g_sd.exists("lock_config.txt");
}

std::array<int, 4> FilePassword()
{
    std::array<int, 4> password = {-1, -1, -1, -1};
    if (!g_sd_available)
    {
        Serial.println("SD card not available; cannot read file.");
        return password;
    }

    FsFile file = g_sd.open(CODE_FILENAME, FILE_READ);
    if (file)
    {
        String line = file.readStringUntil('\n');
        file.close();
        int codeIndex = line.indexOf("CODE: ");
        if (codeIndex != -1)
        {
            String codeStr = line.substring(codeIndex + 6); // Get the part after "CODE: "
            if (codeStr.length() >= 4)
            {
                for (int i = 0; i < 4; i++)
                {
                    password[i] = codeStr.charAt(i) - '0'; // Convert char to int
                }
            }
        }
    }
    else
    {
        Serial.println("Error opening file on SD card.");
    }
    return password;
}

bool HasStoredPassword()
{
    if (!DoesFileExist())
    {
        return false;
    }

    const auto password = FilePassword();
    for (int digit : password)
    {
        if (digit < 0 || digit > 9)
        {
            return false;
        }
    }

    return true;
}

void SaveToSDCard(const String &data)
{
    if (!g_sd_available)
    {
        Serial.println("SD card not available; skipping password write.");
        return;
    }

    DeleteCodeFile();

    FsFile file = g_sd.open(CODE_FILENAME, FILE_WRITE);
    if (file)
    {
        file.println(data);
        file.close();
        Serial.println("Data saved to SD card.");
    }
    else
    {
        Serial.println("Error opening file on SD card.");
    }
}

void DeleteCodeFile()
{
    if (!g_sd_available)
    {
        return;
    }

    if (g_sd.exists(CODE_FILENAME) && g_sd.remove(CODE_FILENAME))
    {
        Serial.println("Code file cleared.");
    }
    else
    {
        Serial.println("No code file to clear or failed to remove it.");
    }
}

bool IsPasswordComplete()
{
    for (int digit : passwordDigits)
    {
        if (digit == -1)
        {
            return false;
        }
    }

    return true;
}

bool PasswordsMatch(const std::array<int, 4> &storedPassword)
{
    for (int i = 0; i < 4; i++)
    {
        if (passwordDigits[i] != storedPassword[i])
        {
            return false;
        }
    }

    return true;
}

void ResetPasswordCapture()
{
    capturingPassword = false;
    currentDigitIndex = 0;
    for (int i = 0; i < 4; i++)
    {
        passwordDigits[i] = -1;
    }
}

std::array<String, 2> GenerateRandomColor()
{
    int randomColorIndex = random(1, 8);
    return {String(randomColorIndex), String(g_keyword_map.at(randomColorIndex).c_str())};
}

void StartColorChallenge()
{
  //Has to change to listening model
    const auto generatedColor = GenerateRandomColor();
    expectedColorDigit = generatedColor[0].toInt();
    String message = generatedColor[1] + ": (" + generatedColor[0] + ")";
    Serial.println("Password accepted. Enter digit for color: " + message);
    display_message(generatedColor[1] + " : (" + generatedColor[0] + ")");
}

String PasswordToString(const int digits[4])
{
    return String(digits[0]) + String(digits[1]) + String(digits[2]) + String(digits[3]);
}

String PasswordToString(const std::array<int, 4> &digits)
{
    return String(digits[0]) + String(digits[1]) + String(digits[2]) + String(digits[3]);
}

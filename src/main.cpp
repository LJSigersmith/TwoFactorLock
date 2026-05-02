#include <Arduino.h>
#include <IRremote.hpp>
#include <map>
#include <string>
#include <array>

#include <SdFat.h>

#include "display.h"
#include "logging.h"
#include "mic_inference.h"

#define CODE_FILENAME "lock_config.txt"
#define SD_CARD_PIN D2
#define IR_RECEIVE_PIN A0
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
const std::map<uint8_t, int> g_remote_digits =
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
int g_keyword_int = 0;
int g_unlock_code = 0;

int g_password_digits[4] = {-1, -1, -1, -1}; // Array to hold the 4 digits of the password
int g_current_digit_index = 0;                // Index to track which digit is being entered
bool g_capturing_password = false;
int g_expected_color_digit = -1;
bool g_is_app_locked = true;

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

int HexToInt(uint8_t hexCode);
void SaveToSDCard(const String &data);
void DeleteCodeFile();
bool does_code_file_exist();
bool HasStoredPassword();
bool IsPasswordComplete();
void ResetPasswordCapture();
std::array<int, 4> get_file_password();
bool PasswordsMatch(const std::array<int, 4> &storedPassword);
std::pair<String, String> GenerateRandomColor();
String PasswordToString(const int digits[4]);
String PasswordToString(const std::array<int, 4> &digits);

using namespace std;


/* –– Setup & Loop –––––––––––––––––––––––––––––––––––––––––––––––––––––––––––– */
void setup() {
  Serial.begin(115200);
  while (!Serial.available()) {}

  // Init SD
  g_sd_available = g_sd.begin(SD_CARD_PIN);
  if (!g_sd_available) { Serial.println("SD initialization failed!"); }

  // Init logging
  logging_init(g_sd);

  // Init IR
  Serial.println(F("Enabling IRin"));
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  Serial.print(F("Ready to receive IR signals at pin "));
  Serial.println(IR_RECEIVE_PIN);

  // Init display
  display_init();

  // Init mic / inference
  mic_inference_init();

  // Read code from file
  read_code();
}

void loop() {
  switch (g_current_stage) {
    case START:              start();              break;
    case WAITING_FOR_CODE:   waiting_for_code();   break;
    case CODE_ENTERED:       check_code();         break;
    case CODE_INCORRECT:     code_incorrect();     break;
    case CODE_CORRECT:       code_correct();       break;
    case WAITING_FOR_KEYWORD: waiting_for_keyword(); break;
    case KEYWORD_CORRECT:    keyword_correct();    break;
    case KEYWORD_INCORRECT:  keyword_incorrect();  break;
    case UNLOCKED:           unlocked();           break;
    default:                                       break;
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
  display_lock_screen();
  ResetPasswordCapture();
  g_capturing_password = true;
  Serial.println(HasStoredPassword() ? "Enter stored password." : "Record a new password.");
  g_current_stage = WAITING_FOR_CODE;
}

void waiting_for_code() {
  if (!IrReceiver.decode()) return;

  int decimalValue = HexToInt(IrReceiver.decodedIRData.command);

  if (g_capturing_password && decimalValue >= 0 && decimalValue <= 9) {
    Serial.println("Capturing password...");
    CapturePassword(decimalValue);
    if (IsPasswordComplete()) g_current_stage = CODE_ENTERED;
  }

  Serial.println("Decoded: " + String(decimalValue));
  delay(100);
  IrReceiver.resume();
}

void check_code() {
  const bool hasStoredPassword = HasStoredPassword();
  const String capturedPassword = PasswordToString(g_password_digits);
  Serial.println("Captured: " + capturedPassword);

  if (!hasStoredPassword) {
    SaveToSDCard("CODE: " + capturedPassword);
    g_is_app_locked = false;
    ResetPasswordCapture();
    Serial.println("New password saved.");
    g_current_stage = UNLOCKED;
    return;
  }

  const auto storedPassword = get_file_password();
  if (PasswordsMatch(storedPassword)) {
    ResetPasswordCapture();
    g_current_stage = CODE_CORRECT;
  } else {
    ResetPasswordCapture();
    g_current_stage = CODE_INCORRECT;
  }
}

void code_incorrect() {
  g_is_app_locked = true;
  Serial.println("Password mismatch.");
  display_code_incorrect();
  delay(3000);
  display_lock_screen();
  ResetPasswordCapture();
  g_capturing_password = true;
  Serial.println("Enter stored password.");
  g_current_stage = WAITING_FOR_CODE;
}

void code_correct() {
  display_code_correct();
  delay(3000);

  std::pair<String, String> generatedColor = GenerateRandomColor();
  g_expected_color_digit = generatedColor.first.toInt();
  Serial.println("Password accepted. Say color: " + generatedColor.second);
  display_say_color_screen(g_expected_color_digit);

  mic_inference_reset();
  g_current_stage = WAITING_FOR_KEYWORD;
}

void waiting_for_keyword() {
  String detected;
  if (!mic_inference_run(detected)) return;

  String expected = String(g_keyword_map.at(g_expected_color_digit).c_str());
  Serial.println("Detected: " + detected + " | Expected: " + expected);
  if (expected == detected) {
    Serial.println("Keyword correct");
    g_current_stage = KEYWORD_CORRECT;
  }
}

void keyword_incorrect() {
  g_is_app_locked = true;
  g_expected_color_digit = -1;
  Serial.println("Color mismatch. App remains locked.");
  display_lock_screen();
  ResetPasswordCapture();
  g_capturing_password = true;
  Serial.println("Enter stored password.");
  g_current_stage = WAITING_FOR_CODE;
}

void keyword_correct() {
  g_is_app_locked = false;
  g_expected_color_digit = -1;
  Serial.println("Color accepted. App unlocked.");
  g_current_stage = UNLOCKED;
}

void unlocked() {
  display_unlock_screen();
}

int HexToInt(uint8_t hexCode)
{
    auto it = g_remote_digits.find(hexCode);
    return (it != g_remote_digits.end()) ? it->second : hexCode;
}

void CapturePassword(int currDigit)
{
    if (currDigit < 0 || currDigit > 9) return;

    if (g_current_digit_index < 4) {
        g_password_digits[g_current_digit_index] = currDigit;
        g_current_digit_index++;

        String displayPassword = "";
        for (int i = 0; i < g_current_digit_index; i++) {
            displayPassword += "*";
            display_capturing_password(g_current_digit_index);
        }
        Serial.println("Captured digit: " + String(currDigit) + " | Display: " + displayPassword);
    }
}

bool does_code_file_exist()
{
    if (!g_sd_available) {
        Serial.println("SD card not available; cannot check for file.");
        return false;
    }
    return g_sd.exists(CODE_FILENAME);
}

std::array<int, 4> get_file_password()
{
    std::array<int, 4> password = {-1, -1, -1, -1};
    if (!g_sd_available) {
        Serial.println("SD card not available; cannot read file.");
        return password;
    }

    FsFile file = g_sd.open(CODE_FILENAME, FILE_READ);
    if (file) {
        String line = file.readStringUntil('\n');
        file.close();
        int codeIndex = line.indexOf("CODE: ");
        if (codeIndex != -1) {
            String codeStr = line.substring(codeIndex + 6);
            if (codeStr.length() >= 4) {
                for (int i = 0; i < 4; i++)
                    password[i] = codeStr.charAt(i) - '0';
            }
        }
    } else {
        Serial.println("Error opening file on SD card.");
    }
    return password;
}

bool HasStoredPassword()
{
    if (!does_code_file_exist()) return false;
    const auto password = get_file_password();
    for (int digit : password)
        if (digit < 0 || digit > 9) return false;
    return true;
}

void SaveToSDCard(const String &data)
{
    if (!g_sd_available) {
        Serial.println("SD card not available; skipping password write.");
        return;
    }
    DeleteCodeFile();
    FsFile file = g_sd.open(CODE_FILENAME, FILE_WRITE);
    if (file) {
        file.println(data);
        file.close();
        Serial.println("Data saved to SD card.");
    } else {
        Serial.println("Error opening file on SD card.");
    }
}

void DeleteCodeFile()
{
    if (!g_sd_available) return;
    if (g_sd.exists(CODE_FILENAME) && g_sd.remove(CODE_FILENAME))
        Serial.println("Code file cleared.");
    else
        Serial.println("No code file to clear or failed to remove it.");
}

bool IsPasswordComplete()
{
    for (int digit : g_password_digits)
        if (digit == -1) return false;
    return true;
}

bool PasswordsMatch(const std::array<int, 4> &storedPassword)
{
    for (int i = 0; i < 4; i++)
        if (g_password_digits[i] != storedPassword[i]) return false;
    return true;
}

void ResetPasswordCapture()
{
    g_capturing_password = false;
    g_current_digit_index = 0;
    for (int i = 0; i < 4; i++)
        g_password_digits[i] = -1;
}

std::pair<String, String> GenerateRandomColor()
{
    int randomColorIndex = random(1, 8);
    return {String(randomColorIndex), String(g_keyword_map.at(randomColorIndex).c_str())};
}

String PasswordToString(const int digits[4])
{
    return String(digits[0]) + String(digits[1]) + String(digits[2]) + String(digits[3]);
}

String PasswordToString(const std::array<int, 4> &digits)
{
    return String(digits[0]) + String(digits[1]) + String(digits[2]) + String(digits[3]);
}

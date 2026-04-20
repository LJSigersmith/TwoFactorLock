#include <Arduino.h>

// Display
#include <U8g2lib.h>
#include <Wire.h>
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


void display_init() {

    Wire.begin();
    u8g2.begin();
    u8g2.setFlipMode(1);
    
}

void display_lock_screen() {
    
    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_7x13B_tr);
    u8g2.drawStr(0, 13, "LOCKED");

    u8g2.sendBuffer();

}
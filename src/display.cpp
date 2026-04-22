/**
 * @file display.cpp
 * @brief Methods to display different states on screen
 * @date 2026-04-22
 * 
 */
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

    // Screen border
    u8g2.drawFrame(0, 0, 128, 64);

    // Shackle: upper-half arc + two legs descending into body
    u8g2.drawCircle(64, 13, 10, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    u8g2.drawLine(54, 13, 54, 24);
    u8g2.drawLine(74, 13, 74, 24);

    // Lock body
    u8g2.drawRBox(48, 24, 32, 18, 2);

    // Keyhole cut-out
    u8g2.setDrawColor(0);
    u8g2.drawDisc(64, 30, 3);
    u8g2.drawBox(62, 33, 5, 5);
    u8g2.setDrawColor(1);

    // "LOCKED" centered at bottom
    u8g2.setFont(u8g2_font_7x13B_tr);
    u8g2.drawStr((128 - u8g2.getStrWidth("LOCKED")) / 2, 60, "LOCKED");

    u8g2.sendBuffer();
}
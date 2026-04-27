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

void display_unlock_screen() {
     u8g2.clearBuffer();

    // Screen border
    u8g2.drawFrame(0, 0, 128, 64);

    // Shackle: open — right leg stays in body, left leg lifts out
    // Right leg: connected to body
    u8g2.drawLine(74, 13, 74, 24);
    // Arc across the top (upper half only)
    u8g2.drawCircle(64, 13, 10, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    // Left leg: disconnected and angled upward-left (open position)
    //u8g2.drawLine(54, 13, 44, 4);

    // Lock body
    u8g2.drawRBox(48, 24, 32, 18, 2);

    // Keyhole cut-out
    u8g2.setDrawColor(0);
    u8g2.drawDisc(64, 30, 3);
    u8g2.drawBox(62, 33, 5, 5);
    u8g2.setDrawColor(1);

    // "UNLOCKED" centered at bottom
    u8g2.setFont(u8g2_font_7x13B_tr);
    u8g2.drawStr((128 - u8g2.getStrWidth("UNLOCKED")) / 2, 60, "UNLOCKED");

    u8g2.sendBuffer();
}

void display_message(const String &message) {
    u8g2.clearBuffer();

    // Screen border
    u8g2.drawFrame(0, 0, 128, 64);
    String sayMessage = "Say:";
    // Display message centered
    u8g2.setFont(u8g2_font_7x13B_tr);
    // Center "Say:" on the first line and the message on the second line
    u8g2.drawStr((128 - u8g2.getStrWidth(sayMessage.c_str())) / 2, 16, sayMessage.c_str());
    u8g2.drawStr((128 - u8g2.getStrWidth(message.c_str())) / 2, 32, message.c_str());

    u8g2.sendBuffer();
}
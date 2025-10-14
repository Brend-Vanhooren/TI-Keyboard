#include <stdint.h>
#include <usbdrvce.h>
#include <graphx.h>
#include <string.h>


#define USB_HID_DESCRIPTOR  0x21
#define USB_HID_REPORT_DESCRIPTOR 0x22
#define DEFAULT_LANGID 0x0409

#define LEFT_CTRL 1
#define LEFT_SHIFT 2
#define LEFT_ALT 4
#define LEFT_GUI 8
#define RIGHT_CTRL 16
#define RIGHT_SHIFT 32
#define RIGHT_ALT 64
#define RIGHT_GUI 128

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdHID;
    uint8_t  bCountryCode;
    uint8_t  bNumDescriptors;
    uint8_t  bReportDescriptorType;
    uint16_t wDescriptorLength;
} usb_hid_descriptor_t;

typedef struct {
    uint16_t row;
    uint16_t col;
    bool second;
    bool alfa;
    uint8_t hid_code;
} KeyMap;

typedef struct {
    uint8_t row;
    uint8_t col;
    uint8_t report[8];  // full HID report
} ShortcutMap;


static const KeyMap keymap[] = {
// row,col,hid_cod
    {3, 0, false, false, 0x62}, // 0 and Insert
    {3, 1, false, false, 0x59}, // 1 and End
    {4, 1, false, false, 0x5A}, // 2 and Down Arrow
    {5, 1, false, false, 0x5B}, // 3 and PageDown
    {3, 2, false, false, 0x5C}, // 4 and Left Arrow
    {4, 2, false, false, 0x5D}, // 5
    {5, 2, false, false, 0x5E}, // 6 and Right Arrow
    {3, 3, false, false, 0x5F}, // 7 and Home
    {4, 3, false, false, 0x60}, // 8 and Up Arrow
    {5, 3, false, false, 0x61}, // 9 and PageUp
    {4, 0, false, false, 0x63}, // . and Delete
    {5, 0, false, false, 0x56}, // -
    {6, 0, false, false, 0x58}, // Enter
    {6, 1, false, false, 0x57}, // +
    {6, 2, false, false, 0x56}, // -
    {6, 3, false, false, 0x55}, // *
    {6, 4, false, false, 0x54}, // /
    {2, 4, false, false, 0x35}, // Grave Accent and Tilde (should be ²)
    {3, 4, false, false, 0x10}, // M voor ,
    {4, 4, false, false, 0x22}, // (
    {5, 4, false, false, 0x2D}, // )
    {7, 0, false, false, 0x51}, // Down Arrow
    {7, 1, false, false, 0x50}, // Left Arrow}
    {7, 2, false, false, 0x4F}, // Right Arrow
    {7, 3, false, false, 0x52}, // Up Arrow
    {1, 6, false, false, 0x53}, // Mode → Num Lock
    {6, 5, false, false, 0x2F}, // ^
    {1, 7, false, false, 0x2A}, // Delete → Backspace
    {2, 6, false, true, 0x04}, // A
    {3, 6, false, true, 0x05}, // B
    {4, 6, false, true, 0x06}, // C
    {2, 5, false, true, 0x07}, // D
    {3, 5, false, true, 0x08}, // E
    {4, 5, false, true, 0x09}, // F
    {5, 5, false, true, 0x0A}, // G
    {6, 5, false, true, 0x0B}, // H
    {2, 4, false, true, 0x0C}, // I
    {3, 4, false, true, 0x0D}, // J
    {4, 4, false, true, 0x0E}, // K
    {5, 4, false, true, 0x0F}, // L
    {6, 4, false, true, 0x10}, // M
    {2, 3, false, true, 0x11}, // N
    {3, 3, false, true, 0x12}, // O
    {4, 3, false, true, 0x13}, // P
    {5, 3, false, true, 0x14}, // Q
    {6, 3, false, true, 0x15}, // R
    {2, 2, false, true, 0x16}, // S
    {3, 2, false, true, 0x17}, // T
    {4, 2, false, true, 0x18}, // U
    {5, 2, false, true, 0x19}, // V
    {6, 2, false, true, 0x1A}, // W
    {2, 1, false, true, 0x1B}, // X
    {3, 1, false, true, 0x1C}, // Y
    {4, 1, false, true, 0x1D}, // Z
    // can't type θ
    {6, 1, false, true, 0x21}, // "
    {3, 0, false, true, 0x2C}, // Space
    {4, 0, false, true, 0x37}, // :
};

static const ShortcutMap shortcutsmap[] = {
    // row, col, full 8-byte HID report
    {1, 0, {LEFT_GUI, 0, 0, 0, 0, 0, 0, 0}}, // Win
    {1, 3, {LEFT_GUI, 0, 0x07, 0, 0, 0, 0, 0}} // Win + D
};



#define KEYMAP_SIZE (sizeof(keymap) / sizeof(keymap[0]))

static uint8_t find_hid_code(uint8_t row, uint8_t col, bool second, bool alfa) {
    for (uint8_t i = 0; i < KEYMAP_SIZE; i++) {
        if (keymap[i].row == row && keymap[i].col == col && keymap[i].second == second && keymap[i].alfa == alfa)
            return keymap[i].hid_code;
    }
    return 0; // 0 means "no match / no key"
}

bool find_shortcut(uint8_t row, uint8_t col, uint8_t *out_report) {
    for (uint8_t i = 0; i < sizeof(shortcutsmap)/sizeof(shortcutsmap[0]); i++) {
        if (shortcutsmap[i].row == row && shortcutsmap[i].col == col) {
            memcpy(out_report, shortcutsmap[i].report, 8);
            return true;
        }
    }
    return false;
}



void send_report(uint8_t *report, size_t report_size,usb_endpoint_t endpoint) {
    usb_Transfer(endpoint, report, report_size, 5, NULL);
}

// Converts a byte to two hex characters and prints them
void gfx_PrintByteHex(uint8_t byte) {
    const char hex_chars[] = "0123456789ABCDEF";
    gfx_PrintChar(hex_chars[(byte >> 4) & 0x0F]);
    gfx_PrintChar(hex_chars[byte & 0x0F]);
}

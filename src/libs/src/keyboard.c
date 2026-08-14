#include <stddef.h>
#include "bool.h"
#include "x86_64/idt.h"
#include "drivers/keyboard/ps2.h"
#include "drivers/keyboard/keyboard.h"

#define KEYBOARD_EXTENDED_SCAN_CODE 0xE0

static bool key_states[0x200];

void (*keyboard_handler_user)(struct KeyboardEvent event);

static uint16_t keycode_to_index(uint16_t fat_code) {
    if ((fat_code >> 8) == KEYBOARD_EXTENDED_SCAN_CODE) {
        // Extended key: use upper 256 slots
        return 0x100 | (fat_code & 0xFF);
    }
    // Normal key: use lower 256 slots
    return fat_code & 0xFF;
}

void keyboard_handler() {
	static bool is_extended = 0;
	
	uint8_t scan_code = ps2_read_scan_code();
	
	if (scan_code == KEYBOARD_EXTENDED_SCAN_CODE) {
		is_extended = true;
		return;
	}
	
	if (keyboard_handler_user == NULL) {
		return;
	}
	
	uint16_t fat_code = scan_code & 0x7F;
	
	if (is_extended) {
		is_extended = false;
		fat_code |= KEYBOARD_EXTENDED_SCAN_CODE << 8;
	}
	
	struct KeyboardEvent event;
	
	if ((scan_code & 0x80) == 0) {
		event.type = KEYBOARD_EVENT_TYPE_MAKE;
	} else {
		event.type = KEYBOARD_EVENT_TYPE_BREAK;
	}
	
	event.code = fat_code;

	key_states[keycode_to_index(fat_code)] = (event.type == KEYBOARD_EVENT_TYPE_MAKE);
	
	keyboard_handler_user(event);
}

void keyboard_init() {
	idt_init();
	idt_set_handler_keyboard(keyboard_handler);
}

void keyboard_set_handler(void (*handler)(struct KeyboardEvent event)) {
	keyboard_handler_user = handler;	
}

bool keyboard_is_down(uint16_t keycode) {
    return key_states[keycode_to_index(keycode)];
}

bool keyboard_is_up(uint16_t keycode) {
    return !key_states[keycode_to_index(keycode)];
}

#undef NULL
#define NULL 0
//       [a][b][c][d]
// a: Language (klk // keyboard layout language)
//      - 0: German QWERTZ
//      - 1: EN-US QWERTY
// b: key modifiers (example: shift) (klm // keyboard layout modifiers)
//      - 0: None
//      - 1: Shift
//      - 2: Alt Gr
// c: keyboard rows (klr // keyboard layout rows)
// d: keys (klk // keyboard layout keys)
const int kll = 2;
const int klm = 3;
const int klr = 4;
const int klk = 13;
char keyboard_layouts[2][3][4][13] = {
    {
        {
            {'^', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', NULL, NULL},
            {'q', 'w', 'e', 'r', 't', 'z', 'u', 'i', 'o', 'p', NULL, '+'},
            {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', NULL, NULL, '#'},
            {'<', 'y', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-'}
        },
        {
            {NULL, '!', '"', NULL, '$', '%', '&', '/', '(', ')', '=', '?', '`'},
            {'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I', 'O', 'P', NULL, '*'},
            {'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', NULL, NULL, '\''},
            {'>', 'Y', 'X', 'C', 'V', 'B', 'N', 'M', ';', ':', '_'}
        },
        {
            {NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{', '[', ']', '}', '\\', NULL},
            {'@', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '~'},
            {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
            {'|', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
        }
    },
    {
        {
            {'`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '='},
            {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']'},
            {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\''},
            {'\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/'}
        },
        {
            {'~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+'},
            {'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}'},
            {'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"'},
            {'|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?'}
        }
    }
};
#undef NULL
#define NULL ((void *)0)

int keyboard_map[6][18] = {
    {0x01, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44 ,0x57, 0x58, 0xE032, 0x46, 0x45},
    {0x29, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0xE052, 0xE047, 0xE049},
    {0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0xE053, 0xE04F, 0xE051},
    {0x3A, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2B},
    {0x2A, 0x56, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0xE048},
    {0x1D, 0xE05D, 0x38, 0x29, 0xE038, 0x05D, 0x01D}
};

uint8_t keycode_to_ascii_ext(uint16_t code, bool shift_pressed, bool altgr_pressed) {
// #ifdef KEYBOARD_GERMAN
//     const char* keyboard_layout = "german";
// #endif
// #ifdef KEYBOARD_EN_US
//     const char* keyboard_layout = "en_us";
// #endif

//     for (int language = 0; language <= kll; language++) {
        
//     }

    switch (code) {
        case KEY_CODE_A: return shift_pressed ? 'A' : 'a';
        case KEY_CODE_B: return shift_pressed ? 'B' : 'b';
        case KEY_CODE_C: return shift_pressed ? 'C' : 'c';
        case KEY_CODE_D: return shift_pressed ? 'D' : 'd';
        case KEY_CODE_E: return shift_pressed ? 'E' : altgr_pressed ? 0x80 : 'e';
        case KEY_CODE_F: return shift_pressed ? 'F' : 'f';
        case KEY_CODE_G: return shift_pressed ? 'G' : 'g';
        case KEY_CODE_H: return shift_pressed ? 'H' : 'h';
        case KEY_CODE_I: return shift_pressed ? 'I' : 'i';
        case KEY_CODE_J: return shift_pressed ? 'J' : 'j';
        case KEY_CODE_K: return shift_pressed ? 'K' : 'k';
        case KEY_CODE_L: return shift_pressed ? 'L' : 'l';
        case KEY_CODE_M: return shift_pressed ? 'M' : altgr_pressed ? 0xB5 : 'm';
        case KEY_CODE_N: return shift_pressed ? 'N' : 'n';
        case KEY_CODE_O: return shift_pressed ? 'O' : 'o';
        case KEY_CODE_P: return shift_pressed ? 'P' : 'p';
        case KEY_CODE_Q: return shift_pressed ? 'Q' : altgr_pressed ? '@' : 'q';
        case KEY_CODE_R: return shift_pressed ? 'R' : 'r';
        case KEY_CODE_S: return shift_pressed ? 'S' : 's';
        case KEY_CODE_T: return shift_pressed ? 'T' : 't';
        case KEY_CODE_U: return shift_pressed ? 'U' : 'u';
        case KEY_CODE_V: return shift_pressed ? 'V' : 'v';
        case KEY_CODE_W: return shift_pressed ? 'W' : 'w';
        case KEY_CODE_X: return shift_pressed ? 'X' : 'x';
        case KEY_CODE_Y: return shift_pressed ? 'Y' : 'y';
        case KEY_CODE_Z: return shift_pressed ? 'Z' : 'z';

        case KEY_CODE_Ä: return shift_pressed ? 0xC4 : 0xE4;
        case KEY_CODE_Ö: return shift_pressed ? 0xD6 : 0xF6;
        case KEY_CODE_Ü: return shift_pressed ? 0xDC : 0xFC;

        case KEY_CODE_ß: return shift_pressed ? '?' : altgr_pressed ? '\\' : 0xDF;

        case KEY_CODE_SPACE: return ' ';
        case KEY_CODE_ENTER: return '\n';

        case KEY_CODE_0: return shift_pressed ? '=' : altgr_pressed ? '}' : '0';
        case KEY_CODE_1: return shift_pressed ? '!' : '1';
        case KEY_CODE_2: return shift_pressed ? '\"' : altgr_pressed ? 0xB2 : '2';
        case KEY_CODE_3: return shift_pressed ? 0xA7 : altgr_pressed ? 0xB3 : '3';
        case KEY_CODE_4: return shift_pressed ? '$' : '4';
        case KEY_CODE_5: return shift_pressed ? '%' : '5';
        case KEY_CODE_6: return shift_pressed ? '&' : '6';
        case KEY_CODE_7: return shift_pressed ? '/' : altgr_pressed ? '{' : '7';
        case KEY_CODE_8: return shift_pressed ? '(' : altgr_pressed ? '[' : '8';
        case KEY_CODE_9: return shift_pressed ? ')' : altgr_pressed ? ']' : '9';

        case KEY_CODE_UP_ARROW: return shift_pressed ? 0xB0 : '^';
        case KEY_CODE_ACUTE_ACCENT: return shift_pressed ? 0xB5 : '`';
        case KEY_CODE_ARROW: return shift_pressed ? '>' : altgr_pressed ? '|' : '<';
        case KEY_CODE_PLUS: return shift_pressed ? '*' : altgr_pressed ? '~' : '+';
        case KEY_CODE_HASH: return shift_pressed ? '\'' : '#';
        case KEY_CODE_COMMA: return shift_pressed ? ';' : ',';
        case KEY_CODE_DOT: return shift_pressed ? ':' : '.';
        case KEY_CODE_DASH: return shift_pressed ? '_' : '-';
    }    
    
    return 0x00;
}

/*
    switch (code) {
        case KEY_CODE_A: return shift_pressed ? 'A' : 'a';
        case KEY_CODE_B: return shift_pressed ? 'B' : 'b';
        case KEY_CODE_C: return shift_pressed ? 'C' : 'c';
        case KEY_CODE_D: return shift_pressed ? 'D' : 'd';
        case KEY_CODE_E: return shift_pressed ? 'E' : 'e';
        case KEY_CODE_F: return shift_pressed ? 'F' : 'f';
        case KEY_CODE_G: return shift_pressed ? 'G' : 'g';
        case KEY_CODE_H: return shift_pressed ? 'H' : 'h';
        case KEY_CODE_I: return shift_pressed ? 'I' : 'i';
        case KEY_CODE_J: return shift_pressed ? 'J' : 'j';
        case KEY_CODE_K: return shift_pressed ? 'K' : 'k';
        case KEY_CODE_L: return shift_pressed ? 'L' : 'l';
        case KEY_CODE_M: return shift_pressed ? 'M' : 'm';
        case KEY_CODE_N: return shift_pressed ? 'N' : 'n';
        case KEY_CODE_O: return shift_pressed ? 'O' : 'o';
        case KEY_CODE_P: return shift_pressed ? 'P' : 'p';
        case KEY_CODE_Q: return shift_pressed ? 'Q' : altgr_pressed ? '@' : 'q';
        case KEY_CODE_R: return shift_pressed ? 'R' : 'r';
        case KEY_CODE_S: return shift_pressed ? 'S' : 's';
        case KEY_CODE_T: return shift_pressed ? 'T' : 't';
        case KEY_CODE_U: return shift_pressed ? 'U' : 'u';
        case KEY_CODE_V: return shift_pressed ? 'V' : 'v';
        case KEY_CODE_W: return shift_pressed ? 'W' : 'w';
        case KEY_CODE_X: return shift_pressed ? 'X' : 'x';
        case KEY_CODE_Y: return shift_pressed ? 'Y' : 'y';
        case KEY_CODE_Z: return shift_pressed ? 'Z' : 'z';

        case KEY_CODE_ß: return shift_pressed ? '?' : altgr_pressed ? '\\' : 0x00;

        case KEY_CODE_SPACE: return ' ';
        case KEY_CODE_ENTER: return '\n';

        case KEY_CODE_0: return shift_pressed ? '=' : altgr_pressed ? '}' : '0';
        case KEY_CODE_1: return shift_pressed ? '!' : '1';
        case KEY_CODE_2: return shift_pressed ? '\"' : '2';
        case KEY_CODE_3: return '3';
        case KEY_CODE_4: return shift_pressed ? '$' : '4';
        case KEY_CODE_5: return shift_pressed ? '%' : '5';
        case KEY_CODE_6: return shift_pressed ? '&' : '6';
        case KEY_CODE_7: return shift_pressed ? '/' : altgr_pressed ? '{' : '7';
        case KEY_CODE_8: return shift_pressed ? '(' : altgr_pressed ? '[' : '8';
        case KEY_CODE_9: return shift_pressed ? ')' : altgr_pressed ? ']' : '9';

        case KEY_CODE_UP_ARROW: return '^';
        case KEY_CODE_ACUTE_ACCENT: return '`';
        case KEY_CODE_ARROW: return shift_pressed ? '>' : altgr_pressed ? '|' : '<';
        case KEY_CODE_PLUS: return shift_pressed ? '*' : altgr_pressed ? '~' : '+';
        case KEY_CODE_HASH: return shift_pressed ? '\'' : '#';
        case KEY_CODE_COMMA: return shift_pressed ? ';' : ',';
        case KEY_CODE_DOT: return shift_pressed ? ':' : '.';
        case KEY_CODE_DASH: return shift_pressed ? '_' : '-';
    }    
    
    return 0x00;
*/
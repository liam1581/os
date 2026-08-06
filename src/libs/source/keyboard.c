#include <stddef.h>
#include "bool.h"
#include "x86_64/idt.h"
#include "drivers/ps2.h"
#include "drivers/keyboard.h"

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



char keycode_to_ascii(uint16_t code) {
    switch (code) {
        case KEY_CODE_A: return 'A';
        case KEY_CODE_B: return 'B';
        case KEY_CODE_C: return 'C';
        case KEY_CODE_D: return 'D';
        case KEY_CODE_E: return 'E';
        case KEY_CODE_F: return 'F';
        case KEY_CODE_G: return 'G';
        case KEY_CODE_H: return 'H';
        case KEY_CODE_I: return 'I';
        case KEY_CODE_J: return 'J';
        case KEY_CODE_K: return 'K';
        case KEY_CODE_L: return 'L';
        case KEY_CODE_M: return 'M';
        case KEY_CODE_N: return 'N';
        case KEY_CODE_O: return 'O';
        case KEY_CODE_P: return 'P';
        case KEY_CODE_Q: return 'Q';
        case KEY_CODE_R: return 'R';
        case KEY_CODE_S: return 'S';
        case KEY_CODE_T: return 'T';
        case KEY_CODE_U: return 'U';
        case KEY_CODE_V: return 'V';
        case KEY_CODE_W: return 'W';
        case KEY_CODE_X: return 'X';
        case KEY_CODE_Y: return 'Y';
        case KEY_CODE_Z: return 'Z';

        case KEY_CODE_SPACE: return ' ';
        case KEY_CODE_ENTER: return '\n';

        case KEY_CODE_0: return '0';
        case KEY_CODE_1: return '1';
        case KEY_CODE_2: return '2';
        case KEY_CODE_3: return '3';
        case KEY_CODE_4: return '4';
        case KEY_CODE_5: return '5';
        case KEY_CODE_6: return '6';
        case KEY_CODE_7: return '7';
        case KEY_CODE_8: return '8';
        case KEY_CODE_9: return '9';

        case KEY_CODE_UP_ARROW: return '^';
        case KEY_CODE_ACUTE_ACCENT: return '`';
        case KEY_CODE_ARROW: return '<';
        case KEY_CODE_PLUS: return '+';
        case KEY_CODE_HASH: return  '#';
        case KEY_CODE_COMMA: return ',';
        case KEY_CODE_DOT: return '.';
        case KEY_CODE_DASH: return '-';
    }    
    
    return 0x00;
}
char keycode_to_ascii_ext(uint16_t code, bool shift_pressed, bool altgr_pressed) {
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
}
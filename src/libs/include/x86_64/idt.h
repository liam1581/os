#pragma once

void idt_init();
void idt_set_handler_keyboard(void (*handler)());
void idt_set_handler_timer(void (*handler)());
void idt_set_handler_mouse(void (*handler)());
void idt_load_ap();
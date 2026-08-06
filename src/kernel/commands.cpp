#include "commands.hpp"

extern "C" {
    #include "command_functions.h"

    #include "debug.h"
}

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

void Commands::add(const char* commandName, int argCount) {
    if (commandCount > MAX_COMMANDS) {
        // error handling: Too many Commands registered
        return;
    }

    commands[commandCount] = {
        commandName,
        argCount,
        nullptr,
        nullptr,
        {},
        0
    };

    commandCount++;
}

void Commands::executes(const char* commandName, int (*func)()) {
    for (int i = 0; i < commandCount; i++) {
        if (strcmp(commands[i].name, commandName) == 0) {
            commands[i].func = func;
            return;
        }
    }
}

void Commands::helpMessage(const char* commandName, const char* helpMessage) {
    for (int i = 0; i < commandCount; i++) {
        if (strcmp(commands[i].name, commandName) == 0) {
            commands[i].helpMessage = helpMessage;
            return;
        }
    }
}

void Commands::handleCommand(char keyboard_buffer[]) {
    DBG_PRINTS("Entered function \"Commands::handleCommand\" with argument: ");
    DBG_PRINTLNS(keyboard_buffer);
    for (int i = 0; i < commandCount; i++) {
        DBG_PRINTLNS("Entered for loop");
        if (strcmp(commands[i].name, keyboard_buffer) == 0) {
            if (commands[i].func != nullptr) {
                commands[i].func();
            } else {
                // error handling: No function assigned
            }
            return;
        }
    }
}

void Commands::registerCommands() {
    Commands commands;

    commands.add("help", 0);
    commands.executes("help", cmd_help);
}
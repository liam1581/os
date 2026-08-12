#pragma once

#include <stddef.h>

extern "C" {
    #include "krnl.h"
}

#define MAX_COMMANDS 256
#define MAX_ARGUMENTS 16
#define MAX_ARGUMENT_BUFFER 1024

#define NullArgument ArgumentObject(NULL, nullptr)

enum class ArgumentType {
    ARG_INT,
    ARG_FLOAT,
    ARG_BOOL,
    ARG_STRING,
};

static inline bool commands_string_equals(const char* a, const char* b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

class ArgumentObject;

class Commands {
public:
    struct Argument {
        const char* name;
        ArgumentType type;
    };

    struct Command {
        const char* name;
        int argCount;
        int (*func)(ArgumentObject) = nullptr;
        const char* helpMessage = nullptr;

        Argument arguments[MAX_ARGUMENTS] = {};
        int argumentCount = 0;
    };

    void add(const char* commandName, int argCount);
    void helpMessage(const char* commandName, const char* helpMessage);
    void executes(const char* commandName, int (*func)(ArgumentObject));

    template<typename T>
    void argument(const char* commandName, const char* argumentName) {
        for (int i = 0; i < commandCount; i++) {
            if (!commands_string_equals(commands[i].name, commandName)) {
                continue;
            }
            
            if (commands[i].argumentCount >= MAX_ARGUMENTS ||
                commands[i].argumentCount >= commands[i].argCount) {
                KERNEL_PANIC("commands.hpp", "TOO MANY ARGUMENTS DEFINED!", 1);
                return;
            }

            ArgumentType type;
            bool typeKnown = true;

            if constexpr (__is_same(T, int)) {
                type = ArgumentType::ARG_INT;
            } else if constexpr (__is_same(T, float)) {
                type = ArgumentType::ARG_FLOAT;
            } else if constexpr (__is_same(T, bool)) {
                type = ArgumentType::ARG_BOOL;
            } else if constexpr (__is_same(T, const char*)) {
                type = ArgumentType::ARG_STRING;
            } else {
                typeKnown = false;
            }

            if (!typeKnown) {
                KERNEL_PANIC("commands.hpp", "TRYING TO DEFINE UNKNOWN ARGUMENT TYPE", 1);
                return;
            }

            commands[i].arguments[commands[i].argumentCount++] = {
                argumentName,
                type
            };

            return;
        }
    }

    void handleCommand(char keyboard_buffer[1024]);

    Command* getCommands() { return commands; }
    [[nodiscard]] int getCommandsCount() const { return commandCount; }

    void registerCommands();

private:
    Command commands[MAX_COMMANDS];
    int commandCount = 0;
};

#undef bool
#undef true
#undef false

class ArgumentValue {
public:
    ArgumentValue(ArgumentType type, const char* value, size_t length);

    operator int() const;
    operator float() const;
    operator bool() const;
    operator const char*() const;

    bool isValid() const;

private:
    ArgumentType type;
    const char* value;
    size_t length;
};

class ArgumentObject {
public:
    ArgumentObject(const char* rawArguments, const Commands::Command* command);

    ArgumentValue getArgument(const char* argumentName) const;
    const char* raw() const;

private:
    const char* rawArguments;
    const Commands::Command* command;
    mutable char stringBuffers[MAX_ARGUMENTS][MAX_ARGUMENT_BUFFER];

    bool getToken(int index, char* output, size_t outputSize) const;
};
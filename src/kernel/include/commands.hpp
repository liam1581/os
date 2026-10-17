#pragma once

#include <stddef.h>

extern "C" {
    #include "log.h"
}

#define MAX_COMMANDS 256
#define MAX_ARGUMENTS 4
#define MAX_ARGUMENT_BUFFER 512

#define NullArgument ArgumentObject(NULL, nullptr)

enum class ArgumentType {
    ARG_INT,
    ARG_BOOL,
    ARG_DOUBLE,
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

        // When true, argCount is a minimum rather than an exact
        // count -- extra raw tokens beyond the declared arguments are
        // allowed through and left for the command's own function to
        // pull via ArgumentObject::getToken()/getTokenCount() (e.g.
        // "run" passing them on as a program's argv).
        bool variadic = false;
    };

    void add(const char* commandName, int argCount, bool variadic = false);
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
                KERNEL_PANIC("TOO MANY ARGUMENTS DEFINED!", 1);
                return;
            }

            ArgumentType type;
            bool typeKnown = true;

            if constexpr (__is_same(T, int)) {
                type = ArgumentType::ARG_INT;
            } else if constexpr (__is_same(T, bool)) {
                type = ArgumentType::ARG_BOOL;
            } else if constexpr (__is_same(T, double) || __is_same(T, float)) {
                type = ArgumentType::ARG_DOUBLE;
            } else if constexpr (__is_same(T, const char*)) {
                type = ArgumentType::ARG_STRING;
            } else {
                typeKnown = false;
            }

            if (!typeKnown) {
                KERNEL_PANIC("TRYING TO DEFINE UNKNOWN ARGUMENT TYPE", 1);
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

class ArgumentValue {
public:
    ArgumentValue(ArgumentType type, const char* value, size_t length);

    operator int() const;
    operator double() const;
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

    // Raw, quote-aware tokens of the full argument string -- lets a
    // command pull more arguments than it declared with argument<T>()
    // (which is capped at MAX_ARGUMENTS). "run" uses this for the
    // program's own argv: token 0 is the declared "file" argument,
    // everything from token 1 onward is passed through as argc/argv.
    int getTokenCount() const;
    bool getToken(int index, char* output, size_t outputSize) const;

private:
    const char* rawArguments;
    const Commands::Command* command;
    mutable char stringBuffers[MAX_ARGUMENTS][MAX_ARGUMENT_BUFFER];
};
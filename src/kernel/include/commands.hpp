#pragma once

#define MAX_COMMANDS 256
#define MAX_ARGUMENTS 16

enum ArgumentType {
    ARG_INT,
    ARG_FLOAT,
    ARG_BOOL,
    ARG_STRING,
};

class Commands {
public:
    struct Argument {
        const char* name;
        ArgumentType type;
    };

    struct Command {
        const char* name;
        int argCount;
        int (*func)();
        const char* helpMessage;

        Argument arguments[MAX_ARGUMENTS];
        int argumentCount;
    };

    void add(const char* commandName, int argCount);
    void helpMessage(const char* commandName, const char* helpMessage);
    void executes(const char* commandName, int (*func)());

    template<typename T>
    void argument(const char* commandName, const char* argumentName) {
        for (int i = 0; i < commandCount; i++) {
            if (commands[i].argumentCount >= MAX_ARGUMENTS) {
                // error handling: Too many arguments
                return;
            }

            ArgumentType type;

            if constexpr (__is_same(T, int)) {
                type = ARG_INT;
            } else if constexpr (__is_same(T, float)) {
                type = ARG_FLOAT;
            } else if constexpr (__is_same(T, bool)) {
                type = ARG_BOOL;
            } else if constexpr (__is_same(T, const char*)) {
                type = ARG_STRING;
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

#ifdef __cplusplus
extern "C" {
#endif

void c_handleCommand(char keyboard_buffer[1024]);

#ifdef __cplusplus
}
#endif
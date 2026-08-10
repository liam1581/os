#include "commands.hpp"

#include "command_functions.hpp"

extern "C" {
    #include "string.h"
    #include "print.h"
    #include "timer.h"

    #include "debug.h"

    #include "krnl.h"
}

#undef bool
#undef true
#undef false

static bool command_string_equals(const char* a, const char* b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}
static bool is_space(char c) {
    return c == ' ' || c == '\t';
}

static int command_line_argument_count(const char* args) {
    int count = 0;
    size_t i = 0;

    while (args[i] != '\0') {
        while (is_space(args[i])) i++;
        if (args[i] == '\0') break;

        count++;
        bool quoted = false;

        while (args[i] != '\0') {
            if (args[i] == '"') {
                quoted = !quoted;
            } else if (!quoted && is_space(args[i])) {
                break;
            }
            i++;
        }
    }

    return count;
}

static bool starts_with_command(const char* input, const char* command, const char** args) {
    size_t i = 0;

    while (command[i] != '\0') {
        if (input[i] != command[i]) return false;
        i++;
    }

    if (input[i] != '\0' && !is_space(input[i])) {
        return false;
    }

    while (is_space(input[i])) i++;
    *args = input + i;
    return true;
}

ArgumentValue::ArgumentValue(ArgumentType type, const char* value, size_t length)
    : type(type), value(value), length(length) {}

bool ArgumentValue::isValid() const {
    return value != nullptr;
}

static int parse_int(const char* text) {
    if (text == nullptr) return 0;

    bool negative = false;
    size_t i = 0;

    if (text[i] == '-') {
        negative = true;
        i++;
    } else if (text[i] == '+') {
        i++;
    }

    int result = 0;
    while (text[i] >= '0' && text[i] <= '9') {
        result = result * 10 + (text[i] - '0');
        i++;
    }

    return negative ? -result : result;
}

static float parse_float(const char* text) {
    if (text == nullptr) return 0.0f;

    bool negative = false;
    size_t i = 0;

    if (text[i] == '-') {
        negative = true;
        i++;
    } else if (text[i] == '+') {
        i++;
    }

    float result = 0.0f;
    while (text[i] >= '0' && text[i] <= '9') {
        result = result * 10.0f + (float)(text[i] - '0');
        i++;
    }

    if (text[i] == '.') {
        i++;
        float place = 0.1f;

        while (text[i] >= '0' && text[i] <= '9') {
            result += (float)(text[i] - '0') * place;
            place *= 0.1f;
            i++;
        }
    }

    return negative ? -result : result;
}

static bool text_equals(const char* a, const char* b) {
    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        char ca = a[i];
        char cb = b[i];

        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';

        if (ca != cb) return false;
        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

ArgumentValue::operator int() const {
    return parse_int(value);
}

ArgumentValue::operator float() const {
    return parse_float(value);
}

ArgumentValue::operator bool() const {
    if (value == nullptr) return false;
    return text_equals(value, "true") ||
           text_equals(value, "yes") ||
           text_equals(value, "1");
}

ArgumentValue::operator const char*() const {
    return value;
}

ArgumentObject::ArgumentObject(
    const char* rawArguments,
    const Commands::Command* command
) : rawArguments(rawArguments), command(command), stringBuffers{} {}

const char* ArgumentObject::raw() const {
    return rawArguments;
}

bool ArgumentObject::getToken(int index, char* output, size_t outputSize) const {
    if (outputSize == 0 || index < 0) return false;

    size_t i = 0;
    int current = 0;

    while (rawArguments[i] != '\0') {
        while (is_space(rawArguments[i])) i++;
        if (rawArguments[i] == '\0') break;

        bool quoted = false;
        size_t outputLength = 0;

        if (rawArguments[i] == '"') {
            quoted = true;
            i++;
        }

        while (rawArguments[i] != '\0') {
            char c = rawArguments[i];

            if (quoted) {
                if (c == '"') {
                    i++;
                    break;
                }
            } else if (is_space(c)) {
                break;
            }

            if (outputLength + 1 < outputSize) {
                output[outputLength++] = c;
            }
            i++;
        }

        output[outputLength] = '\0';

        if (current == index) {
            return true;
        }

        current++;

        while (is_space(rawArguments[i])) i++;
    }

    return false;
}

ArgumentValue ArgumentObject::getArgument(const char* argumentName) const {
    if (command == nullptr || argumentName == nullptr) {
        return ArgumentValue(ARG_STRING, nullptr, 0);
    }

    for (int i = 0; i < command->argumentCount; i++) {
        if (!command_string_equals(command->arguments[i].name, argumentName)) {
            continue;
        }

        if (i >= MAX_ARGUMENTS) {
            return ArgumentValue(command->arguments[i].type, nullptr, 0);
        }

        if (!getToken(i, stringBuffers[i], MAX_ARGUMENT_BUFFER)) {
            return ArgumentValue(command->arguments[i].type, nullptr, 0);
        }

        size_t length = 0;
        while (stringBuffers[i][length] != '\0') length++;

        return ArgumentValue(command->arguments[i].type, stringBuffers[i], length);
    }

    return ArgumentValue(ARG_STRING, nullptr, 0);
}

void Commands::add(const char* commandName, int argCount) {
    if (commandCount >= MAX_COMMANDS || argCount < 0 || argCount > MAX_ARGUMENTS) {
        KERNEL_PANIC("commands.cpp", "TOO MANY COMMANDS/ARGUMENTS REGISERED", 1);
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

void Commands::executes(const char* commandName, int (*func)(ArgumentObject)) {
    for (int i = 0; i < commandCount; i++) {
        if (command_string_equals(commands[i].name, commandName)) {
            commands[i].func = func;
            return;
        }
    }
}

void Commands::helpMessage(const char* commandName, const char* helpMessage) {
    for (int i = 0; i < commandCount; i++) {
        if (command_string_equals(commands[i].name, commandName)) {
            commands[i].helpMessage = helpMessage;
            return;
        }
    }
}

void Commands::handleCommand(char keyboard_buffer[]) {
    const char* args = nullptr;

    for (int i = 0; i < commandCount; i++) {
        if (!starts_with_command(keyboard_buffer, commands[i].name, &args)) {
            continue;
        }
        int suppliedArguments = command_line_argument_count(args);

        if (suppliedArguments != commands[i].argCount) {
            print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
            print("Invalid argument count for ");
            print(commands[i].name);
            print(": expected ");
            print_uint64_dec((uint64_t)commands[i].argCount);
            print(", got ");
            print_uint64_dec((uint64_t)suppliedArguments);
            printc('\n');
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            return;
        }

        if (commands[i].argumentCount != commands[i].argCount) {
            print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
            print("Command ");
            print(commands[i].name);
            println(" has not registered all of its arguments.");
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            return;
        }

        if (commands[i].func != nullptr) {
            ArgumentObject arguments(args, &commands[i]);
            int returnCode = commands[i].func(arguments);
            if (returnCode != 0) {
                print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
                print("ERROR: Command ");
                print(commands[i].name);
                print(" returned error code ");
                print_uint64_dec((uint64_t)returnCode);
                println("!");
                print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            }

        } else {
            clear_screen();
            print("KRNLPANIC::");
            println(commands[i].name);
            KERNEL_PANIC("commands.cpp", "COMMAND DEFINED AS KRNLPANIC::******** HAS NO FUNCTION DEFINED", 0);
        }

        return;
    }

    if (keyboard_buffer[0] != '\0') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        print("Unknown command: ");
        println(keyboard_buffer);
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    }
}

void Commands::registerCommands() {
    add("help", 0);
    executes("help", cmd_help);

    add("echo", 1);
    argument<const char*>("echo", "message");
    executes("echo", cmd_echo);

    add("serial.init", 1);
    argument<int>("serial.init", "baud");
    executes("serial.init", cmd_serial_init);

    add("serial.write", 1);
    argument<const char*>("serial.write", "message");
    executes("serial.write", cmd_serial_write);

    add("cls", 0);
    executes("cls", cmd_cls);

    add("reboot", 0);
    executes("reboot", cmd_restart);

    add("shutdown", 0);
    executes("shutdown", cmd_shutdown);

    add("cd.init", 0);
    executes("cd.init", cmd_atapi_init);

    add("ls", 0);
    executes("ls", cmd_ls);

    add("cat", 1);
    argument<const char*>("cat", "file");
    executes("cat", cmd_cat);

    add("cd", 1);
    argument<const char*>("cd", "directory");
    executes("cd", cmd_cd);

    add("run", 1);
    argument<const char*>("run", "file");
    executes("run", cmd_run);
    
    add("fat.init", 0);
    executes("fat.init", cmd_fat_init);

    add("cre.file", 1);
    argument<const char*>("cre.file", "file");
    executes("cre.file", cmd_cre_file);

    add("cre.dir", 1);
    argument<const char*>("cre.dir", "dir");
    executes("cre.dir", cmd_cre_dir);

    add("cp", 2);
    argument<const char*>("cp", "src");
    argument<const char*>("cp", "dest");
    executes("cp", cmd_cp);

    add("mv", 2);
    argument<const char*>("mv", "src");
    argument<const char*>("mv", "dest");
    executes("mv", cmd_mv);

    add("rm", 1);
    argument<const char*>("rm", "file");
    executes("rm", cmd_rm);

    add("del", 1);
    argument<const char*>("del", "file");
    executes("del", cmd_rm);
    

    add("TESTING", 0);
}
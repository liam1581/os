#include "c_commands.h"

#include "command_functions.hpp"

extern "C" void init_iso() {
    cmd_atapi_init(NullArgument);
}
#include "hello_cpp.h"

extern "C" {
    #include "print.h"
}

class Greeter {
public:
    explicit Greeter(char* name) : name_(name) {}

    void greet() const {
        print("Hello from C++, ");
        print(name_);
        println("!");
    }

private:
    char* name_;
};

extern "C" void cpp_hello() {
    Greeter greeter(const_cast<char*>("kernel"));
    greeter.greet();
}
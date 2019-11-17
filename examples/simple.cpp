
#include <string_view>
#include <iostream>
#include <clue/clue.h>

struct Args {
    std::string message = "Hello Clue!";
    int count = 1;
};

int main(int argc, char** argv) {
    clue::CommandLine<Args> cl("", "Print a message count times.");
    
    cl.Optional(&Args::count, "count", "Number of times to print the message");
    cl.Positional(&Args::message, "message", "A message to print");

    auto args = cl.ParseArgs(argc, argv);

    for (int i = 0; i < args->count; ++i) {
        std::cout << args->message << "\n";
    }
}


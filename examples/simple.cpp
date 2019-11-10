
#include <string_view>
#include <iostream>
#include <clue/clue.h>

struct Args {
    std::string message = "Hello Clue!";
    int count = 1;
};

int main(int argc, char** argv) {
    clue::CommandLine<Args> cl("", "Print a message count times.");
    cl.Add("message", &Args::message, "A message to print");
    cl.Add("count", &Args::count, "Number of times to print the message");

    auto [args, success] = cl.ParseArgs(argc, argv, clue::kExitOnError);

    for (int i = 0; i < args.count; ++i) {
        std::cout << args.message << "\n";
    }
}


#include <string_view>
#include <iostream>
#include <clue/clue.h>

struct Args {
    float a;
    float b;
};

int main(int argc, char** argv) {
    clue::CommandLine<Args> cl("Add", "Add two numbers");

    cl.AddPositional("a", &Args::a);
    cl.AddPositional("b", &Args::b);

    auto [args, success] = cl.ParseArgs(argc, argv, clue::kRequired | clue::kNoDefault);
    std::cout << args.a + args.b << std::endl;
}

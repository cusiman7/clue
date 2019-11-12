#include <string_view>
#include <iostream>
#include <clue/clue.h>

int main(int argc, char** argv) {
    clue::CommandLine cl("Add", "Add two numbers");

    int a, b;
    cl.Positional(&a);
    cl.Positional(&b);

    cl.ParseArgs(argc, argv, clue::kRequired | clue::kNoDefault);
    std::cout << a + b << std::endl;
}

# C.L.U.E. Command Line User Experience

Clue is an easy to use C++ 17 command line parser for programmers and their command line users.

## A Simple Example
```
#include <clue/clue.h>

struct Args {
    std::string message = "Hello Clue!";
    int count = 1;
};

int main(int argc, char** argv) {
    clue::CommandLine<Args> cl("", "Print a message count times.");
    
    cl.Optional(&Args::count, "count", "Number of times to print the message");
    cl.Positional(&Args::message, "message", "A message to print");

    auto [args, success] = cl.ParseArgs(argc, argv);

    for (int i = 0; i < args.count; ++i) {
        std::cout << args.message << "\n";
    }
}
``` 

## Features

* Detailed program "usage" string generation based on names, descriptions, types, and default values of arguments
* Expressive type-aware error messages for your users
* Support for the primitive types: `int`, `float`, `double` and `bool`
* Strong support for the `std::array<T>` container of any of the above primitive types (expect bool)
* Support for `std::string` and `std::string\_view` out of the box as well
* Support for structs composed of any of the above types
* Support for positional and optional arguments
* Configurable, with support for allowing no exiting on an error, support for unrecognized arguments, disabling autohelp, disabling defaults, and marking any or all arguments as required
* Header only

## What Your Users See
What your users see and feel when interacting with your command line tool is even more important than the language you used to write it in or the specific features its command line libraries supported.
Clue puts command line users first with its type aware command line argument parsing and powerful help message generation.
Arguments are annotated with types, labeled as required or optional when appropriate, and formatted to sensible line lengths for humands to be able to read.


For example, a user running `./simple -h` from the example above sees:

```
usage: ./simple [-count <int>] [message <string>]

Print a message count times.

    -count <int>: Number of times to print the message (Default: 1)

    message <string>: A message to print (Default: Hello Clue!)
```

# C.L.U.E. Command Line User Experience

[![](https://github.com/cusiman7/clue/workflows/C/C++%20CI/badge.svg)](https://github.com/cusiman7/clue/actions?query=workflow%3A%22C%2FC%2B%2B+CI%22)

Clue is an easy to use C++ 17 command line parser for programmers and their command line users.

## A Simple Example
```
#include <clue/clue.h>

struct Args {
    std::string message = "Hello Clue!";
    int count = 1;
};

int main(int argc, char** argv) {
    clue::CommandLine<Args> cl("simple", "Print a message count times.");
    
    cl.Optional(&Args::count, "count", "Number of times to print the message");
    cl.Positional(&Args::message, "message", "A message to print");

    auto args = cl.ParseArgs(argc, argv);

    for (int i = 0; i < args->count; ++i) {
        std::cout << args->message << "\n";
    }
}
``` 

## What Your Users See
What your users see and feel when interacting with your command line tool is even more important than the language you used to write it in or the specific features its command line libraries supported.
Clue puts command line users first with its type aware command line argument parsing and powerful help message generation.
Arguments are annotated with types, labeled as required or optional when appropriate, and formatted to sensible line lengths for humans to read.

For example, a user running `./simple -h` from the example above sees:

```
usage: simple [-count <int>] [message <string>]

Print a message count times.

    -count <int>: Number of times to print the message (Default: 1)

    message <string>: A message to print (Default: Hello Clue!)
```

## Technical Features

* Detailed program "usage" string generation based on names, descriptions, types, and default values of arguments
* Help output formatted to sensible line lengths for humans to read
* Expressive type-aware error messages for your users
* Support for the data types: `int`, `float`, `double`, `bool`, `string`, and `string_view`
* Strong support for the `std::array<T, N>` container of any of the above primitive types (except bool)
* Strong support for the `std::vector<T>` container of any of the above primitive types (except bool)
* Support for structs composed of any of the above types
* Support for positional and optional arguments
* Configurable 
  * Support for allowing not exiting early on an error
  * Support for unrecognized arguments
  * Disabling auto-help generation
  * Disabling defaults
  * Marking any or all arguments as required
* Header only

## Roadmap
* Enum for "choices" (from\_string helper required though?)
* Custom "user" types for the programmer
* Windows support
* Better support for aliases and short options 
* Europe friendliness 10,12456
* Large number friendliness 1,000,000.12


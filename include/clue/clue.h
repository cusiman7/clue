/* 
MIT License

Copyright (c) [2019] [Rob Cusimano]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <array>
#include <charconv>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

// TODO:
// vector<type>
// Enum for "choices" (from_string helper required though?)
// custom "user" types for the programmer
// Windows support
// Better support for aliases and short options 

#if defined(__GNUC__) || defined(__clang)
#define PRINTF_LIKE(a, b) __attribute__((format(printf, (a), (b))))
#else
#define PRINTF_LIKE(a, b)
#endif

namespace clue {

template <typename U>
struct ParseResult {
    U value;
    bool success;
};

enum ParseFlags : uint64_t {
    kNone             = 0,
    kNoExitOnError    = 1,  // If any error is encountered, don't exit. Normal behavior is to call exit(1) on error. If set, ParseResult{T, false} will be returned from ParseArgs on error. Used with ParseArgs only.
    kSkipUnrecognized = 2,  // Skip over unrecognized arguments. Normal behavior is to error out on first unrecognized argument. Used with ParseArgs only.
    kNoAutoHelp       = 4,  // Skip auto generating help args "-h", "-help", "--help", and "/?". Used with ParseArgs only.
    kNoDefault        = 8,  // Skip outputting defaults. Normal behavior is to print "(Default: <defaults here>)". Can be used for an entire ParseArgs (used with auto-help) or with Optional/Positional for individual args
    kRequired         = 16  // If arg with this flag is not provided by the user, an error will be reported. Applicable to both ParseArgs (meaning all arguments are required) and Optional/Positional (meaning only that arg is required)
};

struct StringBuilder {
    StringBuilder(int bufSize = 4096);
    ~StringBuilder();

    void NewLine(int count = 1);
    void AppendChar(char c, int count = 1);

    PRINTF_LIKE(2, 3)
    void AppendAtomic(const char* fmt, ...);
    PRINTF_LIKE(3, 4)
    void AppendAtomic(int indent, const char* fmt, ...);
    void AppendNatural(int indent, const char* str, int length);

    std::string_view GetStringView() const;
    void Clear();
private:
    void Grow(int newSize);
    PRINTF_LIKE(2, 3)
    void AppendAndGrow(const char* fmt, ...);
    void AppendAndGrow(const char* fmt, va_list vaList);
    void AppendAndGrow(int len, const char* fmt, va_list vaList);
    void AppendCharAndGrow(char c, int count = 1);

    static constexpr int maxLineLen_ = 80;

    int i_ = 0; // our index into buf
    int lineLen_ = 0; // length of the current in-progress line
    char* buf_ = nullptr;
    int bufSize_ = 0;
};

PRINTF_LIKE(1, 2)
int FormattedLength(const char* fmt, ...);

namespace detail {
template <typename T>
std::string as_string(T&& t);
} // namespace detail

template <typename T>
std::string to_string(T&& t);

template <typename>
struct JustMember;

template <typename T, class C> 
struct JustMember<T C::*> {
    using type = std::remove_cv_t<std::remove_pointer_t<T>>;
};
template <typename T>
using JustMemberT = typename JustMember<T>::type;

template <typename T=std::monostate>
struct CommandLine {
    CommandLine(std::string_view name = "", std::string_view description = "")
    : name_(name), description_(description) {
    }

    // Add a command line option to be parsed.
    // By default, help will be generated, defaults printed, and types checked.
    // name: doesn't require a dash "-" prefix, those are added automatically by Clue.
    // valuePtr: can be any of most primitive types supported by Clue, including contiguous containers and member pointers.
    //           When generating defaults to_string is called on the value, so the pointer must remain valid
    //           as long as CommandLine is in scope.
    // description: used when generating help. Line breaks and formatted are handled for you
    // flags: Some ParseFlags are used when parsing and generating help, such as for generating default strings or not
    template <typename U>
    void Optional(U&& valuePtr, std::string_view name, std::string_view description = "", uint64_t flags = kNone) {
        for (const auto& arg : args_) {
            Assert(name != arg.name, "Name \"%.*s\" already registered\n", static_cast<int>(name.size()), name.data());
        }
        args_.emplace_back(MakeArg(std::forward<U&&>(valuePtr), name, description, static_cast<ParseFlags>(flags), false));
    }
    template <size_t minArgs = 0, size_t maxArgs = std::numeric_limits<size_t>::max(), typename U>
    void Optional(std::vector<U>* valuePtr, std::string_view name, std::string_view description = "", uint64_t flags = kNone) {
        for (const auto& arg : args_) {
            Assert(name != arg.name, "Name \"%.*s\" already registered\n", static_cast<int>(name.size()), name.data());
        }
        args_.emplace_back(MakeArg<minArgs, maxArgs>(valuePtr, name, description, static_cast<ParseFlags>(flags), false));
    }
    template <size_t minArgs = 0, size_t maxArgs = std::numeric_limits<size_t>::max(), typename U>
    void Optional(std::vector<U> T::* valuePtr, std::string_view name, std::string_view description = "", uint64_t flags = kNone) {
        for (const auto& arg : args_) {
            Assert(name != arg.name, "Name \"%.*s\" already registered\n", static_cast<int>(name.size()), name.data());
        }
        args_.emplace_back(MakeArg<minArgs, maxArgs>(valuePtr, name, description, static_cast<ParseFlags>(flags), false));
    }

    // Add a positional command line argument to be parsed.
    // By default, help will be generated, defaults printed, and types checked.
    // name: doesn't require a dash "-" prefix, dashes are not expected for positional args
    // valuePtr: can be any of most primitive types supported by Clue except bool, including contiguous containers and member pointers.
    //           When generating defaults to_string is called on the value, so the pointer must remain valid
    //           as long as CommandLine is in scope.
    // description: used when generating help. Line breaks and formatted are handled for you
    // flags: Some ParseFlags are used when parsing and generating help, such as for generating default strings or not
    template <typename U>
    void Positional(U&& valuePtr, std::string_view name = "", std::string_view description = "", uint64_t flags = kNone) {
        static_assert(!std::is_same_v<std::remove_pointer_t<U>, bool>, "Positional flags don't make sense. Use Optional for bool types instead");
        static_assert(!std::is_same_v<std::remove_cv_t<U>, bool T::*>, "Positional flags don't make sense. Use Optional for bool types instead");
        positionalArgs_.emplace_back(MakeArg(std::forward<U&&>(valuePtr), name, description, static_cast<ParseFlags>(flags), true));
    }

    // Parse argv, matching args added with Optional and Positional before ParseArgs was called
    // On success returns a ParseResult{T, true} with a newly constructed T filled in with options
    // On failure returns a ParseResult{T, false} where T holds the arguments parsed successfully until the failure
    //    Note: If there are valid arguments after the failed argument that could have been parsed into T, they will have been skipped
    // Flags
    ParseResult<T> ParseArgs(const int argc, char** const argv, uint64_t flags = kNone) {
        currentFlags_ = static_cast<ParseFlags>(flags);
        size_t currentPositionalArg = 0;
        T t;

        for (int argIndex = 1; argIndex < argc; argIndex++) {
            std::string_view token(argv[argIndex]);
            auto tokenLen = static_cast<int>(token.size());
            
            if (!(flags & kNoAutoHelp) && (token == "-h" ||  token == "-help" || token == "--help" || token == "/?")) {
                PrintUsage(currentFlags_, argc, argv);
                std::exit(1);
            }

            Arg* argPtr = nullptr;
            bool positionalArg = false;

            if (token.size() >= 1) {
                for (auto& arg : args_) {
                    if (arg.name == token.substr(1)) {
                        argPtr = &arg;
                        break;
                    }
                }
            }
        
            if (!argPtr) {
                if (currentPositionalArg < positionalArgs_.size()) {
                    argPtr = &positionalArgs_[currentPositionalArg];
                    currentPositionalArg++;
                    positionalArg = true;
                }
            }
            if (!argPtr) {
                if (!(flags & kSkipUnrecognized)) {
                    ReportError("Unrecognized argument \"%.*s\"\n", tokenLen, token.data());
                    return {t, false};
                } else {
                    continue;
                }
            }

            auto& arg = *argPtr;
            auto argName= arg.name.data();
            auto argNameLen = static_cast<int>(arg.name.size());

            // If we found a token that looks like a positional arg we'll try to parse that same token as the arg type
            // Bools don't have extra arguments so we simply don't allow their creation as positional args 
            if (positionalArg) {
                argIndex--;
            }

            if (auto intVariant = std::get_if<IntVariant>(&arg.argument)) {
                auto ParseInt = [this, argc, &argv, &argIndex, &argName, argNameLen](bool reportErrors = true) -> ParseResult<int> {
                    argIndex++;
                    if (argIndex >= argc) {
                        if (reportErrors) {
                            ReportError("\"%.*s\" expected an int value\n", argNameLen, argName);
                        }
                        return {0, false};
                    }
                    auto valueToken = std::string_view(argv[argIndex]);
                    auto valueTokenData = valueToken.data();
                    auto valueTokenLen = static_cast<int>(valueToken.size());
                    int v;
                    auto result = std::from_chars(valueTokenData, valueTokenData+valueTokenLen, v);
                    if (result.ec == std::errc::invalid_argument) {
                        if (reportErrors) {
                            ReportError("\"%.*s\" expected a string representing an int but instead found \"%.*s\"\n", argNameLen, argName, valueTokenLen, valueTokenData);
                        }
                        return {v, false};
                    } else if (result.ec == std::errc::result_out_of_range) {
                        if (reportErrors) {
                            ReportError("\"%.*s\" int value \"%.*s\" out of range [%d, %d]\n",
                            argNameLen, argName, valueTokenLen, valueTokenData, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
                        }
                        return {v, false};
                    }
                    return {v, true};
                };

                if (auto intPtr = std::get_if<DataPointer<int>>(intVariant)) {
                    auto [v, success] =  ParseInt();
                    if (!success) {
                        return {t, false};
                    }
                    intPtr->Get(t) = v;
                } else if (auto arrayPtr = std::get_if<Array<int>>(intVariant)) {
                    if (!ParseArray(arrayPtr, ParseInt)) {
                        return {t, false};
                    }
                } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<int>>(intVariant)) {
                    if (!ParseArrayMemberVariant(t, arrayMemberVariant, ParseInt)) {
                        return {t, false};
                    }
                } else if (auto vector = std::get_if<Vector<int>>(intVariant)) {
                    // false means don't report errors
                    auto c = ParseVector(t, *vector, argc, &argIndex, ParseInt, false);
                    if (c < vector->minArgs) {
                        ReportError("\"%.*s\" expected at least %zu arguments but only found %zu\n", argNameLen, argName, vector->minArgs, vector->vec->size());
                        return {t, false};
                    }
                    if (c > vector->maxArgs) {
                        ReportError("\"%.*s\" expected at most %zu arguments but found %zu\n", argNameLen, argName, vector->maxArgs, vector->vec->size());
                        return {t, false};
                    }
                } else if (auto vectorMember = std::get_if<VectorMember<int>>(intVariant)) {
                    // false means don't report errors
                    auto c = ParseVector(t, *vectorMember, argc, &argIndex, ParseInt, false);
                    if (c < vectorMember->minArgs) {
                        ReportError("\"%.*s\" expected at least %zu arguments but only found %zu\n", argNameLen, argName, vectorMember->minArgs, (t.*(vectorMember->vec)).size());
                        return {t, false};
                    }
                    if (c > vectorMember->maxArgs) {
                        ReportError("\"%.*s\" expected at most %zu arguments but fount more\n", argNameLen, argName, vectorMember->maxArgs);
                        return {t, false};
                    }
                } else {
                    ReportError("Unhandled int variant\n");
                    return {t, false};
                }
                arg.wasSet = true;
            } else if (auto floatVariant = std::get_if<FloatVariant>(&arg.argument)) {
                auto ParseFloat = [this, argc, &argv, &argIndex, &argName, argNameLen]() -> ParseResult<float> {
                    argIndex++;
                    if (argIndex >= argc) {
                        ReportError("\"%.*s\" expected a float value\n", argNameLen, argName);
                        return {0.0f, false};
                    }
                    auto valueToken = std::string_view(argv[argIndex]);
                    auto valueTokenData = valueToken.data();
                    auto valueTokenLen = static_cast<int>(valueToken.size());
                    
                    auto v = strtof(valueTokenData, nullptr);
                    // TODO: 0 could be from garbage strings!
                    if (v == HUGE_VAL || v == HUGE_VALF || v == HUGE_VALL) {
                        ReportError("\"%.*s\" float value \"%.*s\" out of range\n", argNameLen, argName, valueTokenLen, valueTokenData);
                        return {v, false};
                    }
                    return {v, true};
                };
                auto ParseDouble = [this, argc, &argv, &argIndex, &argName, argNameLen]() -> ParseResult<double> {
                    argIndex++;
                    if (argIndex >= argc) {
                        ReportError("\"%.*s\" expected a double value\n", argNameLen, argName);
                        return {0.0, false};
                    }
                    auto valueToken = std::string_view(argv[argIndex]);
                    auto valueTokenData = valueToken.data();
                    auto valueTokenLen = static_cast<int>(valueToken.size());
                    
                    auto v = strtod(valueToken.data(), nullptr);
                    if (v == HUGE_VAL || v == HUGE_VALF || v == HUGE_VALL) {
                        ReportError("\"%.*s\" double value \"%.*s\" out of range\n", argNameLen, argName, valueTokenLen, valueTokenData);
                        return {v, false};
                    }
                    return {v, true};
                };
                if (auto floatPtr = std::get_if<DataPointer<float>>(floatVariant)) {
                    auto [v, success] = ParseFloat();
                    if (!success) {
                        return {t, false};
                    }
                    floatPtr->Get(t) = v;
                } else if (auto doublePtr = std::get_if<DataPointer<double>>(floatVariant)) {
                    auto [v, success] = ParseDouble();
                    if (!success) {
                        return {t, false};
                    }
                    doublePtr->Get(t) = v;
                } else if (auto arrayPtr = std::get_if<Array<float>>(floatVariant)) {
                    if (!ParseArray(arrayPtr, ParseFloat)) {
                        return {t, false};
                    }
                } else if (auto arrayPtr = std::get_if<Array<double>>(floatVariant)) {
                    if (!ParseArray(arrayPtr, ParseDouble)) {
                        return {t, false};
                    }
                } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<float>>(floatVariant)) {
                    if (!ParseArrayMemberVariant(t, arrayMemberVariant, ParseFloat)) {
                        return {t, false};
                    }
                } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<double>>(floatVariant)) {
                    if (!ParseArrayMemberVariant(t, arrayMemberVariant, ParseDouble)) {
                        return {t, false};
                    }
                } else {
                    ReportError("Unhandled float variant\n");
                    return {t, false};
                }
                arg.wasSet = true;
            } else if (auto boolVariant = std::get_if<BoolVariant>(&arg.argument)) {
                assert(!positionalArg);
                if (auto boolPtr = std::get_if<DataPointer<bool>>(boolVariant)) {
                    boolPtr->Get(t) = !boolPtr->Get(t);
                } else {
                    ReportError("Unhandled bool variant\n");
                    return {t, false};
                }
                arg.wasSet = true;
            } else if (auto stringVariant = std::get_if<StringVariant>(&arg.argument)) {
                argIndex++;
                if (argIndex >= argc) {
                    ReportError("\"%.*s\" expected a string value\n", argNameLen, argName);
                    return {t, false};
                }
                auto valueToken = std::string_view(argv[argIndex]);
                std::visit([&arg, &t, &valueToken](auto& strDataPtr) {
                    strDataPtr.Get(t) = valueToken;
                    arg.wasSet = true;
                }, *stringVariant);
                if (!arg.wasSet) {
                    ReportError("Unhandled string variant\n");
                    return {t, false};
                }
            } // end variant matching
        }

        // Check for any mising required arguments
        bool missingSomething = false;
        StringBuilder sb;
        sb.AppendAtomic("Missing required arguments:");
        sb.NewLine(2);
        for(const auto& arg : positionalArgs_) {
            if ((currentFlags_ & kRequired || arg.flags & kRequired) && !arg.wasSet) {
                missingSomething = true;
                sb.AppendChar(' ', 4);
                AppendNameAndType(arg, sb, 0, currentFlags_);
                //sb.AppendAtomic("    %.*s", static_cast<int>(arg.name.size()), arg.name.data());
                sb.NewLine();
            }
        }
        for(const auto& arg : args_) {
            if ((currentFlags_ & kRequired || arg.flags & kRequired) && !arg.wasSet) {
                missingSomething = true;
                sb.AppendChar(' ', 4);
                AppendNameAndType(arg, sb, 0, currentFlags_);
                //sb.AppendAtomic("    %.*s", static_cast<int>(arg.name.size()), arg.name.data());
                sb.NewLine();
            }
        }
        if (missingSomething) {
            PrintUsage(currentFlags_);
            auto sv = sb.GetStringView();
            ReportError("%.*s\n", static_cast<int>(sv.size()), sv.data());
        }

        return {t, true};
    }

    // Prints the full usage string to stdout
    void PrintUsage(uint64_t flags, int argc = 0, char** const argv = nullptr) const {
        StringBuilder usageBuilder; // For building the first usage lines
        StringBuilder descriptionBuilder; // For building the description line per argument

        // Default construct a T, for type info of ArrayMembers and default values
        const static T t = {};

        // build usage line
        // Looks like:
        // usage: <name> [-flag0] [-arg1 <float>] [-arg2 <string>] [-arg3 <int[3]>]
        int usageIndent = 0;
        if (!name_.empty()) {
            usageIndent = FormattedLength("usage: %.*s", static_cast<int>(name_.size()), name_.data());
            usageBuilder.AppendAtomic(0, "usage: %.*s", static_cast<int>(name_.size()), name_.data());
        } else if (argv != nullptr) { 
            usageIndent = FormattedLength("usage: %s", argv[0]);
            usageBuilder.AppendAtomic(0, "usage: %s", argv[0]);
        } else {
            usageIndent = FormattedLength("usage:");
            usageBuilder.AppendAtomic(0, "usage:");
        }

        // build descriptions 
        // Looks like:
        // Long program description here
        //     argument: argument's description
        //
        //     other_argument: other argument's description (Default: <some_default>)
        //
        descriptionBuilder.AppendNatural(0, description_.data(), static_cast<int>(description_.size()));
        descriptionBuilder.NewLine(2);

        auto DescribeArg = [flags, &usageBuilder, &usageIndent, &descriptionBuilder](const auto& arg) {
            StringBuilder defaultBuilder(64); // Used for building strings for default values
            auto ArrayDefault = [&defaultBuilder](auto arrayBegin, size_t size) {
                if (size == 0) return;
                for (size_t i = 0; i < size - 1; ++i) {
                    defaultBuilder.AppendAtomic("%s ", to_string(arrayBegin[i]).c_str());
                }
                defaultBuilder.AppendAtomic("%s", to_string(arrayBegin[size-1]).c_str());
            };
            defaultBuilder.Clear();

            int descriptionIndent = 0;
    
            auto argNameLen = static_cast<int>(arg.name.size());
            auto argNameData = arg.name.data();
            const char* usagePrefix = "";
            const char* usageSuffix = "";
            const char* namePrefix = "";
            if (!arg.isPositional) {
                namePrefix = "-";
            }            
            if (!(flags & kRequired) && !(arg.flags & kRequired)) {
                usagePrefix = "["; 
                usageSuffix = "]"; 
            } 

            if (auto intVariant = std::get_if<IntVariant>(&arg.argument)) {
                if (auto arrayPtr = std::get_if<Array<int>>(intVariant)) {
                    size_t size = arrayPtr->end - arrayPtr->begin;
                    usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s <int[%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, size, usageSuffix);

                    descriptionIndent = FormattedLength("    %s%.*s <int[%zu]>", namePrefix, argNameLen, argNameData, size);
                    descriptionBuilder.AppendAtomic(0, "    %s%.*s <int[%zu]>", namePrefix, argNameLen, argNameData, size);
                    
                    ArrayDefault(arrayPtr->begin, size);
                } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<int>>(intVariant)) {
                    size_t size = std::visit([&ArrayDefault](auto arrayMember) {
                        size_t size = (t.*arrayMember).size();
                        ArrayDefault((t.*arrayMember).begin(), size);
                        return size;
                    }, *arrayMemberVariant);
                    usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s <int[%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, size, usageSuffix);

                    descriptionIndent = FormattedLength("    %s%.*s <int[%zu]>", namePrefix, argNameLen, argNameData, size);
                    descriptionBuilder.AppendAtomic(0, "    %s%.*s <int[%zu]>", namePrefix, argNameLen, argNameData, size);
                } else if (std::holds_alternative<Vector<int>>(*intVariant) || std::holds_alternative<VectorMember<int>>(*intVariant)) {
                    size_t minArgs = 0;
                    size_t maxArgs = std::numeric_limits<size_t>::max();
                    if (auto vector = std::get_if<Vector<int>>(intVariant)) {
                        ArrayDefault(vector->vec->begin(), vector->vec->size());
                        minArgs = vector->minArgs;
                        maxArgs = vector->maxArgs; 
                    } else if (auto vector = std::get_if<VectorMember<int>>(intVariant)) {
                        ArrayDefault((t.*(vector->vec)).begin(), (t.*(vector->vec)).size());
                        minArgs = vector->minArgs;
                        maxArgs = vector->maxArgs;
                    }
                    if (minArgs != 0 && maxArgs != std::numeric_limits<size_t>::max()) {
                        usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s <int[%zu:%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, minArgs, maxArgs, usageSuffix);
                        descriptionIndent = FormattedLength("    %s%.*s <int[%zu:%zu]>", namePrefix, argNameLen, argNameData, minArgs, maxArgs);
                        descriptionBuilder.AppendAtomic(0, "    %s%.*s <int[%zu:%zu]>", namePrefix, argNameLen, argNameData, minArgs, maxArgs);
                    } else if (minArgs != 0 && maxArgs == std::numeric_limits<size_t>::max()) {
                        usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s <int[%zu:]>%s", usagePrefix, namePrefix, argNameLen, argNameData, minArgs, usageSuffix);
                        descriptionIndent = FormattedLength("    %s%.*s <int[%zu:]>", namePrefix, argNameLen, argNameData, minArgs);
                        descriptionBuilder.AppendAtomic(0, "    %s%.*s <int[%zu:]>", namePrefix, argNameLen, argNameData, minArgs);
                    } else if (minArgs == 0 && maxArgs != std::numeric_limits<size_t>::max()) {
                        usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s <int[:%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, maxArgs, usageSuffix);
                        descriptionIndent = FormattedLength("    %s%.*s <int[:%zu]>", namePrefix, argNameLen, argNameData, maxArgs);
                        descriptionBuilder.AppendAtomic(0, "    %s%.*s <int[:%zu]>", namePrefix, argNameLen, argNameData, maxArgs);
                    } else {
                        usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s <int[...]>%s", usagePrefix, namePrefix, argNameLen, argNameData, usageSuffix);
                        descriptionIndent = FormattedLength("    %s%.*s <int[...]>", namePrefix, argNameLen, argNameData);
                        descriptionBuilder.AppendAtomic(0, "    %s%.*s <int[...]>", namePrefix, argNameLen, argNameData);
                    }
                } else { // Either int or int T::*
                    usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s <int>%s", usagePrefix, namePrefix, argNameLen, argNameData, usageSuffix);

                    descriptionIndent = FormattedLength("    %s%.*s <int>", namePrefix, argNameLen, argNameData);
                    descriptionBuilder.AppendAtomic(0, "    %s%.*s <int>", namePrefix, argNameLen, argNameData);

                    if (auto intPtr = std::get_if<DataPointer<int>>(intVariant)) {
                        defaultBuilder.AppendAtomic("%s", to_string(intPtr->Get(t)).c_str());
                    }
                }
            } else if (auto floatVariant = std::get_if<FloatVariant>(&arg.argument)) {
                if (auto arrayPtr = std::get_if<Array<float>>(floatVariant)) {
                    size_t size = arrayPtr->end - arrayPtr->begin;
                    usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s <float[%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, size, usageSuffix);

                    descriptionIndent = FormattedLength("   %s%.*s <float[%zu]>", namePrefix, argNameLen, argNameData, size);
                    descriptionBuilder.AppendAtomic(0, "    %s%.*s <float[%zu]>", namePrefix, argNameLen, argNameData, size);
                    ArrayDefault(arrayPtr->begin, size);
                } else if (auto arrayPtr = std::get_if<Array<double>>(floatVariant)) {
                    size_t size = arrayPtr->end - arrayPtr->begin;
                    usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s <double[%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, size, usageSuffix);
                    
                    descriptionIndent = FormattedLength("    %s%.*s <double[%zu]>", namePrefix, argNameLen, argNameData, size);
                    descriptionBuilder.AppendAtomic(0, "    %s%.*s <double[%zu]>", namePrefix, argNameLen, argNameData, size);
                    ArrayDefault(arrayPtr->begin, size);
                } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<float>>(floatVariant)) {
                    size_t size = std::visit([&ArrayDefault](auto arrayMember) {
                        size_t size = (t.*arrayMember).size();
                        ArrayDefault((t.*arrayMember).begin(), size);
                        return size;
                    }, *arrayMemberVariant);
                    usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s <float[%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, size, usageSuffix);
                    
                    descriptionIndent = FormattedLength("    %s%.*s <float[%zu]>", namePrefix, argNameLen, argNameData, size);
                    descriptionBuilder.AppendAtomic(0, "    %s%.*s <float[%zu]>", namePrefix, argNameLen, argNameData, size);
                } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<double>>(floatVariant)) {
                    size_t size = std::visit([&ArrayDefault](auto arrayMember) {
                        size_t size = (t.*arrayMember).size();
                        ArrayDefault((t.*arrayMember).begin(), size);
                        return size;
                    }, *arrayMemberVariant);
                    usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s <double[%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, size, usageSuffix);
                    
                    descriptionIndent = FormattedLength("    %s%.*s <double[%zu]>", namePrefix, argNameLen, argNameData, size);
                    descriptionBuilder.AppendAtomic(0, "    %s%.*s <double[%zu]>", namePrefix, argNameLen, argNameData, size);
                } else if (auto floatPtr = std::get_if<DataPointer<float>>(floatVariant)) {
                    usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s <float>%s", usagePrefix, namePrefix, argNameLen, argNameData, usageSuffix);
                    
                    descriptionIndent = FormattedLength("    %s%.*s <float>", namePrefix, argNameLen, argNameData);
                    descriptionBuilder.AppendAtomic(0, "    %s%.*s <float>", namePrefix, argNameLen, argNameData);

                    defaultBuilder.AppendAtomic("%s", to_string(floatPtr->Get(t)).c_str());
                } else if (auto doublePtr = std::get_if<DataPointer<double>>(floatVariant)) {
                    usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s <double>%s", usagePrefix, namePrefix, argNameLen, argNameData, usageSuffix);
                    
                    descriptionIndent = FormattedLength("    %s%.*s <double>", namePrefix, argNameLen, argNameData);
                    descriptionBuilder.AppendAtomic(0, "    %s%.*s <double>", namePrefix, argNameLen, argNameData);
                    
                    defaultBuilder.AppendAtomic("%s", to_string(doublePtr->Get(t)).c_str());
                }
            } else if (auto stringVariantPtr = std::get_if<StringVariant>(&arg.argument)) {
                usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s <string>%s", usagePrefix, namePrefix, argNameLen, argNameData, usageSuffix);
                
                descriptionIndent = FormattedLength("    %s%.*s <string>", namePrefix, argNameLen, argNameData);
                descriptionBuilder.AppendAtomic(0, "    %s%.*s <string>", namePrefix, argNameLen, argNameData);

                if (auto stringPtr = std::get_if<DataPointer<std::string>>(stringVariantPtr)) {
                    defaultBuilder.AppendAtomic("%s", stringPtr->Get(t).c_str());
                } else if (auto stringViewPtr = std::get_if<DataPointer<std::string_view>>(stringVariantPtr)) {
                    auto sv = stringViewPtr->Get(t);
                    defaultBuilder.AppendAtomic("%.*s", static_cast<int>(sv.size()), sv.data());
                }
            } else if (auto boolVariantPtr = std::get_if<BoolVariant>(&arg.argument)) {
                usageBuilder.AppendAtomic(usageIndent, " %s%s%.*s%s", usagePrefix, namePrefix, argNameLen, argNameData, usageSuffix);
                
                descriptionIndent = FormattedLength("    %s%.*s", namePrefix, argNameLen, argNameData);
                descriptionBuilder.AppendAtomic(0, "    %s%.*s", namePrefix, argNameLen, argNameData);
                if (auto boolPtr = std::get_if<DataPointer<bool>>(boolVariantPtr)) {
                    defaultBuilder.AppendAtomic("%s", boolPtr->Get(t) ? "true" : "false");
                }
            }
                    
            // Append every argument's description
            if ((flags & kRequired) || (arg.flags & kRequired)) {
                descriptionBuilder.AppendAtomic(descriptionIndent, " (Required): ");
            } else {
                descriptionBuilder.AppendAtomic(": ");
            }
            descriptionBuilder.AppendNatural(descriptionIndent, arg.description.data(), arg.description.size());

            if (!(flags & kNoDefault) && !(arg.flags & kNoDefault)) {
                auto sv = defaultBuilder.GetStringView();
                descriptionBuilder.AppendChar(' ');
                descriptionBuilder.AppendAtomic(descriptionIndent, "(Default: %.*s)", static_cast<int>(sv.size()), sv.data());
            }
            descriptionBuilder.NewLine(2);
        };

        for (const auto& arg : args_) {
            DescribeArg(arg);
        }

        for (const auto& arg : positionalArgs_) {
            DescribeArg(arg);
        }

        auto sv = usageBuilder.GetStringView();
        printf("%.*s\n\n", static_cast<int>(sv.size()), sv.data());
        
        sv = descriptionBuilder.GetStringView();
        printf("%.*s", static_cast<int>(sv.size()), sv.data());
    }


private:
    template <typename U>
    struct DataPointer {
        DataPointer(U* v) : v_(v) {};
        DataPointer(U T::* v) : v_(v) {};
        U& Get(T& t) {
            if (auto v = std::get_if<U T::*>(&v_)) {
                return t.*(*v);
            } else {
                return *std::get<0>(v_);
            }
        }
        const U& Get(const T& t) const {
            if (auto v = std::get_if<U T::*>(&v_)) {
                return t.*(*v);
            } else {
                return *std::get<0>(v_);
            }
        }
        std::variant<U*, U T::*> v_;
    };
    template <typename U>
    struct Array {
        U* begin;
        U* end;
    };
    template <typename U>
    struct Vector {
        std::vector<U>* Get(T&) const {
            return vec;
        }
        std::vector<U>* vec;
        const size_t minArgs;
        const size_t maxArgs;
    };
    template <typename U>
    struct VectorMember{
        std::vector<U>* Get(T& t) const {
            return &(t.*vec);
        }
        std::vector<U> T::* vec;
        const size_t minArgs;
        const size_t maxArgs;
    };

    template <typename U>
    using ArrayMemberVariant = std::variant<std::array<U, 1> T::*, std::array<U, 2> T::*, std::array<U, 3> T::*, std::array<U, 4> T::*, std::array<U, 5> T::*,
         std::array<U, 6> T::*, std::array<U, 7> T::*, std::array<U, 8> T::*, std::array<U, 9> T::*, std::array<U, 10> T::*>;
    using IntVariant = std::variant<DataPointer<int>, Array<int>, ArrayMemberVariant<int>, Vector<int>, VectorMember<int>>;
    using FloatVariant = std::variant<DataPointer<float>, DataPointer<double>, Array<float>, Array<double>, ArrayMemberVariant<float>, ArrayMemberVariant<double>>;
    using BoolVariant = std::variant<DataPointer<bool>>;
    using StringVariant = std::variant<DataPointer<std::string_view>, DataPointer<std::string>>;

    using ArgumentVariant = std::variant<IntVariant, FloatVariant, BoolVariant, StringVariant>;
    
    struct Arg {
        ArgumentVariant argument;
        std::string_view name;
        std::string_view description;
        ParseFlags flags = kNone;
        bool wasSet = false;
        bool isPositional = false;
    };

    template <typename V>
    Arg MakeArg(V* value, std::string_view name, std::string_view description, ParseFlags flags, bool isPositional) {
        return Arg{value, name, description, flags, isPositional};
    }
    template <typename MemberT>
    Arg MakeArg(MemberT T::* member, std::string_view name, std::string_view description, ParseFlags flags, bool isPositional) {
        return Arg{member, name, description, flags, isPositional};
    }
    template <typename U, size_t N>
    Arg MakeArg(std::array<U, N>* array, std::string_view name, std::string_view description, ParseFlags flags, bool isPositional) {
        return Arg{Array<U>{array->begin(), array->end()}, name, description, flags, isPositional};
    }
    template <size_t MinArgs, size_t MaxArgs, typename U>
    Arg MakeArg(std::vector<U>* vector, std::string_view name, std::string_view description, ParseFlags flags, bool isPositional) {
        return Arg{Vector<U>{vector, MinArgs, MaxArgs}, name, description, flags, isPositional};
    }
    template <size_t MinArgs, size_t MaxArgs, typename U>
    Arg MakeArg(std::vector<U> T::* vector, std::string_view name, std::string_view description, ParseFlags flags, bool isPositional) {
        return Arg{VectorMember<U>{vector, MinArgs, MaxArgs}, name, description, flags, isPositional};
    }

    template <typename A, typename ParseFunc, typename ...FuncArgs>
    bool ParseArray(A&& arrayPtr, ParseFunc&& parseFunc, FuncArgs&&... args) {
        auto* curr = arrayPtr->begin;
        auto* end = arrayPtr->end;
        while (curr != end) {
            auto [v, success] = parseFunc(std::forward<FuncArgs&&>(args)...);
            if (!success) {
                return false;
            }
            *curr = v;
            curr++;
        }
        return true;
    }

    template <typename AMVP, typename ParseFunc, typename ...FuncArgs> 
    bool ParseArrayMemberVariant(T& t, AMVP&& arrayMemberVariantPtr, ParseFunc&& parseFunc, FuncArgs&&... args) {
        return std::visit([&](auto& arrayMember) -> bool {
            auto* curr = (t.*arrayMember).begin();
            auto* end = (t.*arrayMember).end();
            while (curr != end) {
                auto [v, success] = parseFunc(std::forward<FuncArgs&&>(args)...);
                if (!success) {
                    return false;
                }
                *curr = v;
                curr++;
            }
            return true;
        }, *arrayMemberVariantPtr);
    }
    
    template <typename Vec, typename ParseFunc, typename ...FuncArgs>
    size_t ParseVector(T& t, Vec&& vec, int argc, int* argIndex, ParseFunc&& parseFunc, FuncArgs&&... args) {
        // For vectors we just consume until we can no longer consume any more
        auto vecPtr = vec.Get(t);
        vecPtr->clear();
        size_t i = 0;
        for (; *argIndex <= argc; ++i) {
            auto [v, success] = parseFunc(std::forward<FuncArgs&&>(args)...);
            if (!success) {
                *argIndex -= 1;
                break;
            }
            vecPtr->push_back(v);
        }
        return i;
    }

    void AppendNameAndType(const Arg& arg, StringBuilder& stringBuilder, int indent, ParseFlags flags) {
        auto argNameLen = static_cast<int>(arg.name.size());
        auto argNameData = arg.name.data();

        const char* usagePrefix = "";
        const char* usageSuffix = "";
        const char* namePrefix = "";
        if (!arg.isPositional) {
            namePrefix = "-";
        }            
        if (!(flags & kRequired) && !(arg.flags & kRequired)) {
            usagePrefix = "["; 
            usageSuffix = "]"; 
        } 
        if (auto intVariant = std::get_if<IntVariant>(&arg.argument)) {
            if (auto arrayPtr = std::get_if<Array<int>>(intVariant)) {
                size_t size = arrayPtr->end - arrayPtr->begin;
                stringBuilder.AppendAtomic(indent, "%s%s%.*s <int[%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, size, usageSuffix);
            } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<int>>(intVariant)) {
                size_t size = std::visit([](auto arrayMember) {
                    return std::tuple_size<JustMemberT<decltype(arrayMember)>>::value;
                }, *arrayMemberVariant);
                stringBuilder.AppendAtomic(indent, "%s%s%.*s <int[%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, size, usageSuffix);
            } else if (std::holds_alternative<Vector<int>>(*intVariant) || std::holds_alternative<VectorMember<int>>(*intVariant)) {
                size_t minArgs = 0;
                size_t maxArgs = std::numeric_limits<size_t>::max();
                if (auto vector = std::get_if<Vector<int>>(intVariant)) {
                    minArgs = vector->minArgs;
                    maxArgs = vector->maxArgs; 
                } else if (auto vector = std::get_if<VectorMember<int>>(intVariant)) {
                    minArgs = vector->minArgs;
                    maxArgs = vector->maxArgs;
                }
                if (minArgs != 0 && maxArgs != std::numeric_limits<size_t>::max()) {
                    stringBuilder.AppendAtomic(indent, "%s%s%.*s <int[%zu:%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, minArgs, maxArgs, usageSuffix);
                } else if (minArgs != 0 && maxArgs == std::numeric_limits<size_t>::max()) {
                    stringBuilder.AppendAtomic(indent, "%s%s%.*s <int[%zu:]>%s", usagePrefix, namePrefix, argNameLen, argNameData, minArgs, usageSuffix);
                } else if (minArgs == 0 && maxArgs != std::numeric_limits<size_t>::max()) {
                    stringBuilder.AppendAtomic(indent, "%s%s%.*s <int[:%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, maxArgs, usageSuffix);
                } else {
                    stringBuilder.AppendAtomic(indent, "%s%s%.*s <int[...]>%s", usagePrefix, namePrefix, argNameLen, argNameData, usageSuffix);
                }
            } else { // Either int or int T::*
                stringBuilder.AppendAtomic(indent, "%s%s%.*s <int>%s", usagePrefix, namePrefix, argNameLen, argNameData, usageSuffix);
            }
        } else if (auto floatVariant = std::get_if<FloatVariant>(&arg.argument)) {
            if (auto arrayPtr = std::get_if<Array<float>>(floatVariant)) {
                size_t size = arrayPtr->end - arrayPtr->begin;
                stringBuilder.AppendAtomic(indent, "%s%s%.*s <float[%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, size, usageSuffix);
            } else if (auto arrayPtr = std::get_if<Array<double>>(floatVariant)) {
                size_t size = arrayPtr->end - arrayPtr->begin;
                stringBuilder.AppendAtomic(indent, "%s%s%.*s <double[%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, size, usageSuffix);
            } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<float>>(floatVariant)) {
                size_t size = std::visit([](auto arrayMember) {
                    return std::tuple_size<JustMemberT<decltype(arrayMember)>>::value;
                }, *arrayMemberVariant);
                stringBuilder.AppendAtomic(indent, "%s%s%.*s <float[%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, size, usageSuffix);
            } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<double>>(floatVariant)) {
                size_t size = std::visit([](auto arrayMember) {
                    return std::tuple_size<JustMemberT<decltype(arrayMember)>>::value;
                }, *arrayMemberVariant);
                stringBuilder.AppendAtomic(indent, "%s%s%.*s <double[%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, size, usageSuffix);
            } else if (std::holds_alternative<DataPointer<float>>(*floatVariant)) {
                stringBuilder.AppendAtomic(indent, "%s%s%.*s <float>%s", usagePrefix, namePrefix, argNameLen, argNameData, usageSuffix);
            } else if (std::holds_alternative<DataPointer<double>>(*floatVariant)) {
                stringBuilder.AppendAtomic(indent, "%s%s%.*s <double>%s", usagePrefix, namePrefix, argNameLen, argNameData, usageSuffix);
            }
        } else if (auto stringVariantPtr = std::get_if<StringVariant>(&arg.argument)) {
            stringBuilder.AppendAtomic(indent, "%s%s%.*s <string>%s", usagePrefix, namePrefix, argNameLen, argNameData, usageSuffix);
        } else if (auto boolVariantPtr = std::get_if<BoolVariant>(&arg.argument)) {
            stringBuilder.AppendAtomic(indent, "%s%s%.*s%s", usagePrefix, namePrefix, argNameLen, argNameData, usageSuffix);
        }
    };

    inline void ReportError(const char* fmt, va_list vaList) {
        vfprintf(stderr, fmt, vaList);
        if (!(currentFlags_ & kNoExitOnError)) {
            std::exit(1);
        }
    }

    PRINTF_LIKE(2, 3)
    inline void ReportError(const char* fmt, ...) {
        va_list vaList;
        va_start(vaList, fmt);
        ReportError(fmt, vaList);
        va_end(vaList);
    }

    PRINTF_LIKE(3, 4)
    inline void Assert(bool assertion, const char* fmt, ...) {
        if (!assertion) {
            va_list vaList;
            va_start(vaList, fmt);
            ReportError(fmt, vaList);
            va_end(vaList);
            std::exit(-1);
        }
    }

    std::string_view name_;
    std::string_view description_;
    std::vector<Arg> args_;
    std::vector<Arg> positionalArgs_;
    ParseFlags currentFlags_ = kNone;
};

StringBuilder::StringBuilder(int bufSize) {
    Grow(bufSize);
}

StringBuilder::~StringBuilder() {
    free(buf_);
}

void StringBuilder::NewLine(int count) {
    AppendCharAndGrow('\n', count);
    lineLen_ = 0;
}

void StringBuilder::AppendChar(char c, int count) {
    AppendCharAndGrow(c, count);
}

// Append an atomic unit that cannot be broken. 
PRINTF_LIKE(2, 3)
void StringBuilder::AppendAtomic(const char* fmt, ...) {
    va_list vaList;
    va_list copy;
    va_start(vaList, fmt);
    va_copy(copy, vaList);
    int len = vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    AppendAndGrow(len, fmt, vaList);
    va_end(vaList);
}

// Append an atomic unit that cannot be broken. 
// If needed, a newline + indentation will be inserted before appending.
PRINTF_LIKE(3, 4)
void StringBuilder::AppendAtomic(int indent, const char* fmt, ...) {
    va_list vaList;
    va_list copy;
    va_start(vaList, fmt);
    va_copy(copy, vaList);
    int len = vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    if (lineLen_ + len > maxLineLen_) {
        NewLine();
        AppendChar(' ', indent);
    }
    AppendAndGrow(len, fmt, vaList);
    va_end(vaList);
}

// Append a natural string. Strings will be broken at natural English positions such as whitespace, tabs and newlines
void StringBuilder::AppendNatural(int indent, const char* str, int length) {
    int currentLineStart = 0;
    int lastBreakablePos = 0;
    int cursor = 0;
    while(cursor < length && str[cursor] != '\0') {
        // If we come across whitespace, consider that a natural break position
        if (str[cursor] == ' ' || str[cursor] == '\t') {
            lastBreakablePos = cursor;
        }

        // If we encounter a newline, break here and continue
        if (str[cursor] == '\n') {
            AppendAndGrow("%.*s", cursor - currentLineStart + 1, &str[currentLineStart]); // no \n since the string has a newline
            lineLen_ = 0; // No NewLine() as the string had a newline
            AppendChar(' ', indent);
            cursor++;
            currentLineStart = cursor;
            continue;
        }

        // If we are at our maxLineLen_, break at the lastBreakablePos and continue from there + 1 
        if (cursor - currentLineStart + lineLen_ > maxLineLen_) {
            AppendAndGrow("%.*s", lastBreakablePos - currentLineStart + 1, &str[currentLineStart]);
            NewLine();
            AppendChar(' ', indent);
            cursor = lastBreakablePos + 1;
            currentLineStart = cursor;
            continue;
        }
        cursor++;
    } 
    AppendAndGrow("%.*s", cursor - currentLineStart, &str[currentLineStart]);
}

std::string_view StringBuilder::GetStringView() const {
    return {buf_, static_cast<size_t>(i_)};
}

void StringBuilder::Clear() {
    i_ = 0;
    lineLen_ = 0; 
};

void StringBuilder::Grow(int newSize) {
    bufSize_ = std::max(bufSize_*2, newSize);
    buf_ = static_cast<char*>(realloc(buf_, sizeof(char) * bufSize_));
}

PRINTF_LIKE(2, 3)
void StringBuilder::AppendAndGrow(const char* fmt, ...) {
    va_list vaList;
    va_start(vaList, fmt);
    AppendAndGrow(fmt, vaList);
    va_end(vaList);
}

void StringBuilder::AppendAndGrow(const char* fmt, va_list vaList) {
    va_list copy;
    va_copy(copy, vaList);
    int len = vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    AppendAndGrow(len, fmt, vaList);
}

void StringBuilder::AppendAndGrow(int len, const char* fmt, va_list vaList) {
    if (i_ + len + 1 >= bufSize_) {
        Grow(i_ + len +  1); // plus 1 for nullchar
    }
    assert(i_ + len < bufSize_);
    int added = vsnprintf(buf_+i_, bufSize_-i_, fmt, vaList);
    lineLen_ += added;
    i_ += added;
}

void StringBuilder::AppendCharAndGrow(char c, int count) {
    if (i_ + count + 1 >= bufSize_) {
        Grow(i_ + count +  1); // plus 1 for nullchar
    }
    assert(i_ + count < bufSize_);
    for (int i = 0; i < count; ++i) {
        int added = snprintf(buf_+i_, bufSize_-i_, "%c", c);
        lineLen_ += added;
        i_ += added;
    }
}

PRINTF_LIKE(1, 2)
int FormattedLength(const char* fmt, ...) {
    va_list vaList;
    va_start(vaList, fmt);
    int len = vsnprintf(nullptr, 0, fmt, vaList);
    va_end(vaList);
    return len;
}

// adl helper nonsense
namespace detail {
using std::to_string;
template <typename T>
std::string as_string(T&& t) {
    return to_string(std::forward<T>(t));
}
}
template <typename T>
std::string to_string(T&& t) {
    return detail::as_string(std::forward<T>(t));
}

} // namespace clue


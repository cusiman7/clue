#pragma once

#include <array>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <charconv>
#include <limits>
#include <type_traits>

#include <cstdlib>
#include <cstdio>
#include <cstdarg>

// Tenets
// 1. Great for the command line user
// 2. Great for the command line programmer
// 3. Understandable for us to program and maintain

// TODO:
// Print default values in descriptions
// vector<type>
// Enum for "choices" (from_string helper required though?)
// custom "user" types for the programmer
// Windows support

#if defined(__GNUC__) || defined(__clang)
#define PRINTF_LIKE(a, b) __attribute__((format(printf, (a), (b))))
#else
#define PRINTF_LIKE(a, b)
#endif

namespace rac {

template <typename U>
struct ParseResult {
    U value;
    bool success;
};

template <typename T>
struct Array {
    T* begin;
    T* end;
};

PRINTF_LIKE(1, 2)
int FormattedLength(const char* fmt, ...) {
    va_list vaList;
    va_start(vaList, fmt);
    int len = vsnprintf(nullptr, 0, fmt, vaList);
    va_end(vaList);
    return len;
}

struct StringBuilder {
    StringBuilder() {
        Grow(4096);
    }

    ~StringBuilder() {
        free(buf_);
    }

    void NewLine(int count = 1) {
        AppendCharAndGrow('\n', count);
        lineLen_ = 0;
    }

    void AddChar(char c, int count = 1) {
        AppendCharAndGrow(c, count);
    }

    // Append an atomic unit that cannot be broken. 
    // If needed, a newline + indentation will be inserted before appending.
    PRINTF_LIKE(3, 4)
    void AppendAtomic(int indent, const char* fmt, ...) {
        va_list vaList;
        va_list copy;
        va_start(vaList, fmt);
        va_copy(copy, vaList);
        int len = vsnprintf(nullptr, 0, fmt, copy);
        va_end(copy);
        if (lineLen_ + len > maxLineLen_) {
            NewLine();
            AddChar(' ', indent);
        }
        AppendAndGrow(len, fmt, vaList);
        va_end(vaList);
    }

    // Append a natural string. Strings will be broken at natural English positions such as whitespace, tabs and newlines
    void AppendNatural(int indent, const char* str, int length) {
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
                AddChar(' ', indent);
                cursor++;
                currentLineStart = cursor;
                continue;
            }

            // If we are at our maxLineLen_, break at the lastBreakablePos and continue from there + 1 
            if (cursor - currentLineStart + lineLen_ > maxLineLen_) {
                AppendAndGrow("%.*s", lastBreakablePos - currentLineStart + 1, &str[currentLineStart]);
                NewLine();
                AddChar(' ', indent);
                cursor = lastBreakablePos + 1;
                currentLineStart = cursor;
                continue;
            }
            cursor++;
        } 
        AppendAndGrow("%.*s", cursor - currentLineStart, &str[currentLineStart]);
    }

    std::string_view GetStringView() const {
        return {buf_, static_cast<size_t>(i_)};
    }
private:
    void Grow(int newSize) {
        bufSize_ = std::max(bufSize_*2, newSize);
        buf_ = static_cast<char*>(realloc(buf_, sizeof(char) * bufSize_));
    }

    PRINTF_LIKE(2, 3)
    void AppendAndGrow(const char* fmt, ...) {
        va_list vaList;
        va_start(vaList, fmt);
        AppendAndGrow(fmt, vaList);
        va_end(vaList);
    }

    void AppendAndGrow(const char* fmt, va_list vaList) {
        va_list copy;
        va_copy(copy, vaList);
        int len = vsnprintf(nullptr, 0, fmt, copy);
        va_end(copy);
        AppendAndGrow(len, fmt, vaList);
    }
    
    void AppendAndGrow(int len, const char* fmt, va_list vaList) {
        if (i_ + len + 1 >= bufSize_) {
            Grow(i_ + len +  1); // plus 1 for nullchar
        }
        assert(i_ + len < bufSize_);
        int added = vsnprintf(buf_+i_, bufSize_-i_, fmt, vaList);
        lineLen_ += added;
        i_ += added;
    }
    
    void AppendCharAndGrow(char c, int count = 1) {
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

    static constexpr int maxLineLen_ = 80;

    int i_ = 0; // our index into buf
    int lineLen_ = 0; // length of the current in-progress line
    char* buf_ = nullptr;
    int bufSize_ = 0;
};

enum CLParseFlags {
    kNone             = 0,
    kNoAutoHelp       = 1,
    kExitOnError      = 2,
    kSkipUnrecognized = 4
};

template <typename T=std::monostate>
struct CommandLine {
    CommandLine(std::string_view name = "", std::string_view description = "")
    : name_(name), description_(description) {
    }

    template <typename U>
    void Add(std::string_view name, U&& valuePtr, std::string_view description = "") {
        for (const auto& arg : args_) {
            Assert(name != arg.name, "Name \"%.*s\" already registered\n", static_cast<int>(name.size()), name.data());
        }
        args_.emplace_back(name, std::forward<U&&>(valuePtr), description);
    }

    // Prints the full usage string to stdout and exits the program with std::exit(code)
    void PrintUsageAndExit(int code) const {

        StringBuilder usageBuilder; // For building the first usage lines
        StringBuilder descriptionBuilder; // For building the description line per argument

        // Default construct a T, for type info of ArrayMembers and default values
        const static T t = {};

        // build usage line
        // Looks like:
        // usage: <name> [-flag0] [-arg1 <float>] [-arg2 <string>] [-arg3 <int[3]>]
        int usageIndent = FormattedLength("usage: %.*s", static_cast<int>(name_.size()), name_.data());
        usageBuilder.AppendAtomic(0, "usage: %.*s", static_cast<int>(name_.size()), name_.data());

        // build descriptions 
        // Looke like:
        // Long program description here
        //     argument: argument's description
        //
        //     other_argument: other argument's description
        //
        descriptionBuilder.AppendNatural(0, description_.data(), static_cast<int>(description_.size()));
        descriptionBuilder.NewLine(2);
        //const char* descriptionNameFmt = "    %.*s: ";

        for (const auto& arg : args_) {
            auto argNameLen = static_cast<int>(arg.name.size());
            auto argNameData = arg.name.data();

            int descriptionIndent = 0;

            if (auto intVariant = std::get_if<IntVariant>(&arg.argument)) {
                if (auto arrayPtr = std::get_if<Array<int>>(intVariant)) {
                    size_t size = arrayPtr->end - arrayPtr->begin;
                    usageBuilder.AppendAtomic(usageIndent, " [-%.*s <int[%zu]>]", argNameLen, argNameData, size);

                    descriptionIndent = FormattedLength("    -%.*s <int[%zu]>: ", argNameLen, argNameData, size);
                    descriptionBuilder.AppendAtomic(0, "    -%.*s <int[%zu]>: ", argNameLen, argNameData, size);
                } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<int>>(intVariant)) {
                    size_t size = std::visit([](auto arrayMember) {
                        return (t.*arrayMember).size();
                    }, *arrayMemberVariant);
                    usageBuilder.AppendAtomic(usageIndent, " [-%.*s <int[%zu]>]", argNameLen, argNameData, size);

                    descriptionIndent = FormattedLength("    -%.*s <int[%zu]>: ", argNameLen, argNameData, size);
                    descriptionBuilder.AppendAtomic(0, "    -%.*s <int[%zu]>: ", argNameLen, argNameData, size);
                } else { // Either int or int T::*
                    usageBuilder.AppendAtomic(usageIndent, " [-%.*s <int>]", argNameLen, argNameData);

                    descriptionIndent = FormattedLength("    -%.*s <int>: ", argNameLen, argNameData);
                    descriptionBuilder.AppendAtomic(0, "    -%.*s <int>: ", argNameLen, argNameData);
                }
            } else if (auto floatVariant = std::get_if<FloatVariant>(&arg.argument)) {
                if (auto arrayPtr = std::get_if<Array<float>>(floatVariant)) {
                    size_t size = arrayPtr->end - arrayPtr->begin;
                    usageBuilder.AppendAtomic(usageIndent, " [-%.*s <float[%zu]>]", argNameLen, argNameData, size);

                    descriptionIndent = FormattedLength("    -%.*s <float[%zu]>: ", argNameLen, argNameData, size);
                    descriptionBuilder.AppendAtomic(0, "    -%.*s <float[%zu]>: ", argNameLen, argNameData, size);
                } else if (auto arrayPtr = std::get_if<Array<double>>(floatVariant)) {
                    size_t size = arrayPtr->end - arrayPtr->begin;
                    usageBuilder.AppendAtomic(usageIndent, " [-%.*s <double[%zu]>]", argNameLen, argNameData, size);
                    
                    descriptionIndent = FormattedLength("    -%.*s <double[%zu]>: ", argNameLen, argNameData, size);
                    descriptionBuilder.AppendAtomic(0, "    -%.*s <double[%zu]>: ", argNameLen, argNameData, size);
                } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<float>>(floatVariant)) {
                    size_t size = std::visit([](auto arrayMember) {
                        return (t.*arrayMember).size();
                    }, *arrayMemberVariant);
                    usageBuilder.AppendAtomic(usageIndent, " [-%.*s <float[%zu]>]", argNameLen, argNameData, size);
                    
                    descriptionIndent = FormattedLength("    -%.*s <float[%zu]>: ", argNameLen, argNameData, size);
                    descriptionBuilder.AppendAtomic(0, "    -%.*s <float[%zu]>: ", argNameLen, argNameData, size);
                } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<double>>(floatVariant)) {
                    size_t size = std::visit([](auto arrayMember) {
                        return (t.*arrayMember).size();
                    }, *arrayMemberVariant);
                    usageBuilder.AppendAtomic(usageIndent, " [-%.*s <double[%zu]>]", argNameLen, argNameData, size);
                    
                    descriptionIndent = FormattedLength("    -%.*s <double[%zu]>: ", argNameLen, argNameData, size);
                    descriptionBuilder.AppendAtomic(0, "    -%.*s <double[%zu]>: ", argNameLen, argNameData, size);
                } else if (std::holds_alternative<float*>(*floatVariant) || std::holds_alternative<float T::*>(*floatVariant)) {
                    usageBuilder.AppendAtomic(usageIndent, " [-%.*s <float>]", argNameLen, argNameData);
                    
                    descriptionIndent = FormattedLength("    -%.*s <float>: ", argNameLen, argNameData);
                    descriptionBuilder.AppendAtomic(0, "    -%.*s <float>: ", argNameLen, argNameData);
                } else if (std::holds_alternative<double*>(*floatVariant) || std::holds_alternative<double T::*>(*floatVariant)) {
                    usageBuilder.AppendAtomic(usageIndent, " [-%.*s <double>]", argNameLen, argNameData);
                    
                    descriptionIndent = FormattedLength("    -%.*s <double>: ", argNameLen, argNameData);
                    descriptionBuilder.AppendAtomic(0, "    -%.*s <double>: ", argNameLen, argNameData);
                }
            } else if (auto stringVariantPtr = std::get_if<StringVariant>(&arg.argument)) {
                usageBuilder.AppendAtomic(usageIndent, " [-%.*s <string>]", argNameLen, argNameData);
                
                descriptionIndent = FormattedLength("    -%.*s <string>: ", argNameLen, argNameData);
                descriptionBuilder.AppendAtomic(0, "    -%.*s <string>: ", argNameLen, argNameData);
            } else if (std::holds_alternative<BoolVariant>(arg.argument)) {
                usageBuilder.AppendAtomic(usageIndent, " [-%.*s]", argNameLen, argNameData);
                
                descriptionIndent = FormattedLength("    -%.*s: ", argNameLen, argNameData);
                descriptionBuilder.AppendAtomic(0, "    -%.*s: ", argNameLen, argNameData);
            }
                    
            // Append every argument's description
            descriptionBuilder.AppendNatural(descriptionIndent, arg.description.data(), arg.description.size());
            descriptionBuilder.NewLine(2);
        }

        auto sv = usageBuilder.GetStringView();
        printf("%.*s\n\n", static_cast<int>(sv.size()), sv.data());
        
        sv = descriptionBuilder.GetStringView();
        printf("%.*s", static_cast<int>(sv.size()), sv.data());
        std::exit(code);
    }

    // Parse argv, matching args added with Add before ParseArgs was called
    // On success returns a ParseResult{T, true} with a newly constructed T filled in with options
    // On failure returns a ParseResult{T, false} where T holds the arguments parsed successfully until the failure
    //    Note: If there are valid arguments after the failed argument that could have been parsed into T, they will have been skipped
    // Flags
    ParseResult<T> ParseArgs(const int argc, char** const argv, int flags = 0) {
        currentFlags_ = flags;
        T t;

        int argIndex = 1;
        while (argIndex < argc) {
            std::string_view token(argv[argIndex]);
            auto tokenLen = static_cast<int>(token.size());
            
            if (!(flags & kNoAutoHelp) && (token == "-h" ||  token == "-help" || token == "--help" || token == "/?")) {
                PrintUsageAndExit(1);
            }

            if (token[0] != '-' && !(flags & kSkipUnrecognized)) {
                ReportError("Argument \"%.*s\" does not start with '-'\n", tokenLen, token.data());
                return {t, false};
            }

            bool matchedToken = false;
            for (auto& arg : args_) {
                if (arg.name == token.substr(1)) {
                    matchedToken = true;
                    if (auto intVariant = std::get_if<IntVariant>(&arg.argument)) {
                        auto ParseInt = [this, argc, &argv, &argIndex, &token, tokenLen]() -> ParseResult<int> {
                            if (argIndex >= argc-1) {
                                ReportError("Expected int value for argument \"%.*s\"\n", tokenLen, token.data());
                                return {0, false};
                            }
                            auto valueToken = std::string_view(argv[++argIndex]);
                            auto valueTokenLen = static_cast<int>(valueToken.size());
                            int v;
                            auto result = std::from_chars(valueToken.data(), valueToken.data()+valueToken.size(), v);
                            if (result.ec == std::errc::invalid_argument) {
                                ReportError("Expected a string representing an int but instead found \"%.*s\"\n", valueTokenLen, valueToken.data());
                                return {v, false};
                            } else if (result.ec == std::errc::result_out_of_range) {
                                ReportError("int value \"%.*s\" out of range [%d, %d]\n", valueTokenLen, valueToken.data(), std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
                                return {v, false};
                            }
                            return {v, true};
                        };

                        if (auto intPtr = std::get_if<int*>(intVariant)) {
                            auto [v, success] =  ParseInt();
                            if (!success) {
                                return {t, false};
                            }
                            **intPtr = v;
                        } else if (auto memberPtr = std::get_if<int T::*>(intVariant)) {
                            auto [v, success] =  ParseInt();
                            if (!success) {
                                return {t, false};
                            }
                            t.*(*memberPtr) = v;
                        } else if (auto arrayPtr = std::get_if<Array<int>>(intVariant)) {
                            if (!ParseArray(arrayPtr, ParseInt)) {
                                return {t, false};
                            }
                        } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<int>>(intVariant)) {
                            if (!ParseArrayMemberVariant(t, arrayMemberVariant, ParseInt)) {
                                return {t, false};
                            }
                        } else {
                            ReportError("Unhandled int variant\n");
                            return {t, false};
                        }
                    } else if (auto floatVariant = std::get_if<FloatVariant>(&arg.argument)) {
                        auto ParseFloat = [this, argc, &argv, &argIndex, &token, tokenLen]() -> ParseResult<float> {
                            if (argIndex >= argc-1) {
                                ReportError("Expected float value for argument \"%.*s\"\n", tokenLen, token.data());
                                return {0.0f, false};
                            }
                            auto valueToken = std::string_view(argv[++argIndex]);
                            auto valueTokenLen = static_cast<int>(valueToken.size());
                            
                            auto v = strtof(valueToken.data(), nullptr);
                            if (v == HUGE_VAL || v == HUGE_VALF || v == HUGE_VALL) {
                                ReportError("float value \"%.*s\" out of range\n", valueTokenLen, valueToken.data());
                                return {v, false};
                            }
                            return {v, true};
                        };
                        auto ParseDouble = [this, argc, &argv, &argIndex, &token, tokenLen]() -> ParseResult<double> {
                            if (argIndex >= argc-1) {
                                ReportError("Expected double value for argument \"%.*s\"\n", tokenLen, token.data());
                                return {0.0, false};
                            }
                            auto valueToken = std::string_view(argv[++argIndex]);
                            auto valueTokenLen = static_cast<int>(valueToken.size());
                            
                            auto v = strtod(valueToken.data(), nullptr);
                            if (v == HUGE_VAL || v == HUGE_VALF || v == HUGE_VALL) {
                                ReportError("double value \"%.*s\" out of range\n", valueTokenLen, valueToken.data());
                                return {v, false};
                            }
                            return {v, true};
                        };
                        if (auto floatPtr = std::get_if<float*>(floatVariant)) {
                            auto [v, success] = ParseFloat();
                            if (!success) {
                                return {t, false};
                            }
                            **floatPtr = v;
                        } else if (auto memberPtr = std::get_if<float T::*>(floatVariant)) {
                            auto [v, success] = ParseFloat();
                            if (!success) {
                                return {t, false};
                            }
                            t.*(*memberPtr) = v;
                        } else if (auto doublePtr = std::get_if<double*>(floatVariant)) {
                            auto [v, success] = ParseDouble();
                            if (!success) {
                                return {t, false};
                            }
                            **doublePtr = v;
                        } else if (auto memberPtr = std::get_if<double T::*>(floatVariant)) {
                            auto [v, success] = ParseDouble();
                            if (!success) {
                                return {t, false};
                            }
                            t.*(*memberPtr) = v;
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
                    } else if (auto boolVariant = std::get_if<BoolVariant>(&arg.argument)) {
                        if (auto boolPtr = std::get_if<bool*>(boolVariant)) {
                            **boolPtr = true;
                        } else if (auto memberPtr = std::get_if<bool T::*>(boolVariant)) {
                            t.*(*memberPtr) = true;
                        } else {
                            ReportError("Unhandled bool variant\n");
                            return {t, false};
                        }
                    } else if (auto stringVariant = std::get_if<StringVariant>(&arg.argument)) {
                        if (argIndex >= argc-1) {
                            ReportError("Expected string value for argument \"%.*s\"\n", tokenLen, token.data());
                            return {t, false};
                        }
                        auto valueToken = std::string_view(argv[++argIndex]);

                        if (auto stringPtr = std::get_if<std::string*>(stringVariant)) {
                            **stringPtr = valueToken;
                        } else if (auto memberPtr = std::get_if<std::string T::*>(stringVariant)) {
                            t.*(*memberPtr) = valueToken;
                        } else if (auto stringViewPtr = std::get_if<std::string_view*>(stringVariant)) {
                            **stringViewPtr = valueToken;
                        } else if (auto memberPtr = std::get_if<std::string_view T::*>(stringVariant)) {
                            t.*(*memberPtr) = valueToken;
                        } else {
                            ReportError("Unhandled string variant\n");
                            return {t, false};
                        }
                    }
                }
            }
            if (!matchedToken && !(flags & kSkipUnrecognized)) {
                ReportError("Unrecognized argument \"%.*s\"\n", tokenLen, token.data());
                return {t, false};
            }
            argIndex++;
        }

        return {t, true};
    }

private:
    template <typename U>
    using ArrayMemberVariant = std::variant<std::array<U, 1> T::*, std::array<U, 2> T::*, std::array<U, 3> T::*, std::array<U, 4> T::*, std::array<U, 5> T::*,
         std::array<U, 6> T::*, std::array<U, 7> T::*, std::array<U, 8> T::*, std::array<U, 9> T::*, std::array<U, 10> T::*>;
    using IntVariant = std::variant<int*, int T::*, Array<int>, ArrayMemberVariant<int>>;
    using FloatVariant = std::variant<float*, float T::*, double*, double T::*, Array<float>, Array<double>, ArrayMemberVariant<float>, ArrayMemberVariant<double>>;
    using BoolVariant = std::variant<bool*, bool T::*>;
    using StringVariant = std::variant<std::string_view*, std::string_view T::*, std::string*, std::string T::*>;

    using ArgumentVariant = std::variant<IntVariant, FloatVariant, BoolVariant, StringVariant>;

    struct Arg {
        template <typename V>
        Arg(std::string_view name, V* value, std::string_view description = "") 
            : name(name), argument(value), description(description) {
        }
        template <typename MemberT>
        Arg(std::string_view name, MemberT T::* member, std::string_view description = "") 
            : name(name), argument(member), description(description) {
        }
        template <typename U, size_t N>
        Arg(std::string_view name, std::array<U, N>* array, std::string_view description = "")
            : name(name), argument(Array<U>{array->begin(), array->end()}), description(description) {
        }

        std::string_view name;
        ArgumentVariant argument;
        std::string_view description;
    };

    template <typename A, typename ParseFunc>
    bool ParseArray(A&& arrayPtr, ParseFunc&& parseFunc) {
        auto* curr = arrayPtr->begin;
        auto* end = arrayPtr->end;
        while (curr != end) {
            auto [v, success] = parseFunc();
            if (!success) {
                return false;
            }
            *curr = v;
            curr++;
        }
        return true;
    }

    template <typename AMVP, typename ParseFunc> 
    bool ParseArrayMemberVariant(T& t, AMVP&& arrayMemberVariantPtr, ParseFunc&& parseFunc) {
        return std::visit([&](auto& arrayMember) -> bool {
            auto* curr = (t.*arrayMember).begin();
            auto* end = (t.*arrayMember).end();
            while (curr != end) {
                auto [v, success] = parseFunc();
                if (!success) {
                    return false;
                }
                *curr = v;
                curr++;
            }
            return true;
        }, *arrayMemberVariantPtr);
    }

    inline void ReportError(const char* fmt, va_list vaList) {
        vfprintf(stderr, fmt, vaList);
        if (currentFlags_ & kExitOnError) {
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
    int currentFlags_ = 0;
};

} // namespace rac


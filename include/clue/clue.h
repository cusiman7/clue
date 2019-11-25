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
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// TODO:
// custom "user" types for the programmer
// Enum for "choices" (from_string helper required though?)
// Better support for aliases and short options 
// Europe friendliness 10,12456
// Large number friendliness 1,000,000.12

#if defined(__GNUC__) || defined(__clang)
#define PRINTF_LIKE(a, b) __attribute__((format(printf, (a), (b))))
#else
#define PRINTF_LIKE(a, b)
#endif

namespace clue {

enum ParseFlags : uint64_t {
    kNone             = 0,
    kNoExitOnError    = 1,  // If any error is encountered, don't exit. Normal behavior is to call exit(1) on error. If set, and empty std::optional<T> will be returned from ParseArgs on error. Used with ParseArgs only.
    kSkipUnrecognized = 2,  // Skip over unrecognized arguments. Normal behavior is to error out on first unrecognized argument. Used with ParseArgs only.
    kNoAutoHelp       = 4,  // Skip auto generating help args "-h", "-help", "--help", and "/?". Used with ParseArgs only.
    kNoDefault        = 8,  // Skip outputting defaults. Normal behavior is to print "(Default: <defaults here>)". Can be used for an entire ParseArgs (used with auto-help) or with Optional/Positional for individual args
    kRequired         = 16  // If arg with this flag is not provided by the user, an error will be reported. Applicable to both ParseArgs (meaning all arguments are required) and Optional/Positional (meaning only that arg is required)
};

inline void ReportError(const char* fmt, va_list vaList) {
    vfprintf(stderr, fmt, vaList);
}

PRINTF_LIKE(1, 2)
inline void ReportError(const char* fmt, ...) {
    va_list vaList;
    va_start(vaList, fmt);
    ReportError(fmt, vaList);
    va_end(vaList);
}

PRINTF_LIKE(2, 3)
inline void Assert(bool assertion, const char* fmt, ...) {
    if (!assertion) {
        va_list vaList;
        va_start(vaList, fmt);
        ReportError(fmt, vaList);
        va_end(vaList);
        std::exit(-1);
    }
}

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
struct DataType;
template <typename T> 
struct DataType {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
};
template <typename T, class C> 
struct DataType<T C::*> {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
};
template <typename T>
using DataTypeT = typename DataType<T>::type;

template<class T> struct AlwaysFalse : std::false_type {};

template <typename> struct TypeInfo;

struct UserContainer {}; 

struct ParseState {
    int argc;
    char** argv;
    int* argIndex;
    const char* argName;
    int argNameLen;
    bool reportErrors;
};

template <typename U>
std::optional<U> Parse(ParseState state);

template <typename T=std::monostate, typename ...UserTypes>
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
    template <size_t minArgs = 0, size_t maxArgs = std::numeric_limits<size_t>::max(), typename U>
    void Positional(std::vector<U>* valuePtr, std::string_view name = "", std::string_view description = "", uint64_t flags = kNone) {
        static_assert(!std::is_same_v<std::remove_pointer_t<U>, bool>, "Positional flags don't make sense. Use Optional for bool types instead");
        static_assert(!std::is_same_v<std::remove_cv_t<U>, bool T::*>, "Positional flags don't make sense. Use Optional for bool types instead");
        positionalArgs_.emplace_back(MakeArg<minArgs, maxArgs>(valuePtr, name, description, static_cast<ParseFlags>(flags), true));
    }
    template <size_t minArgs = 0, size_t maxArgs = std::numeric_limits<size_t>::max(), typename U>
    void Positional(std::vector<U> T::* valuePtr, std::string_view name = "", std::string_view description = "", uint64_t flags = kNone) {
        static_assert(!std::is_same_v<std::remove_pointer_t<U>, bool>, "Positional flags don't make sense. Use Optional for bool types instead");
        static_assert(!std::is_same_v<std::remove_cv_t<U>, bool T::*>, "Positional flags don't make sense. Use Optional for bool types instead");
        positionalArgs_.emplace_back(MakeArg<minArgs, maxArgs>(valuePtr, name, description, static_cast<ParseFlags>(flags), true));
    }

    // Parse argv, matching args added with Optional and Positional before ParseArgs was called
    // On success returns a std::opitonal<T> with a newly constructed T filled in with options
    // On failure calls std::exit(1) unless the flag NoExitOnError is passesd where an empty std::optional is returned
    //    Note: If there are valid arguments after the failed argument that could have been parsed into T, they will have been skipped
    std::optional<T> ParseArgs(const int argc, char** const argv, uint64_t flags = kNone) {
        size_t currentPositionalArg = 0;
        T t;

        for (int argIndex = 1; argIndex < argc; argIndex++) {
            std::string_view token(argv[argIndex]);
            auto tokenLen = static_cast<int>(token.size());
            
            if (!(flags & kNoAutoHelp) && (token == "-h" ||  token == "-help" || token == "--help" || token == "/?")) {
                PrintUsage(flags, argc, argv);
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
                    if (!(flags & kNoExitOnError)) {
                        std::exit(1);
                    }
                    return {};
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

            ParseState parseState;
            parseState.argc = argc;
            parseState.argv = argv;
            parseState.argIndex = &argIndex;
            parseState.argName = argName;
            parseState.argNameLen = argNameLen;
            parseState.reportErrors = true;

            bool success = std::visit([&](auto&& a) -> bool { 
                using ValueType = typename std::remove_reference_t<decltype(a)>::value_type;
                using ContainerType = typename std::remove_reference_t<decltype(a)>::container_type;

                if constexpr (std::is_same_v<bool, ValueType>) {
                    assert(!positionalArg);
                    a.Get(t) = !a.Get(t);
                } else if constexpr (std::is_same_v<ValueType*, ContainerType>) {
                    auto v =  Parse<ValueType>(parseState);
                    if (!v) {
                        return false;
                    }
                    a.Get(t) = v.value();
                } else if constexpr (std::is_same_v<std::array<ValueType, 0>*, ContainerType>) {
                    if (!ParseArray(t, a, parseState)) {
                        return false;
                    }
                } else if constexpr (std::is_same_v<std::vector<ValueType>*, ContainerType>) {
                    auto minArgs = a.minArgs;
                    auto maxArgs = a.maxArgs;
                    size_t c = ParseVector(t, a, parseState);
                    if (c < minArgs) {
                        ReportError("\"%.*s\" expected at least %zu arguments but only found %zu\n", argNameLen, argName, minArgs, a.Get(t).size());
                        return false;
                    }
                    if (c > maxArgs) {
                        ReportError("\"%.*s\" expected at most %zu arguments but found %zu\n", argNameLen, argName, maxArgs, a.Get(t).size());
                        return false;
                    }
                } else if constexpr (std::is_same_v<UserContainer, ContainerType>) {
                    bool res = a.Parse(t, parseState);
                    if (!res) {
                        return false;
                    }
                } else {
                    static_assert(AlwaysFalse<ValueType>::value, "Unhandled Argument Type");
                }
                arg.wasSet = true; 
                return true;
            }, arg.argument);
            
            if (!success) {
                if (!(flags & kNoExitOnError)) {
                    std::exit(1);
                }
                return {};
            }
        } // end for loop

        // Check for any mising required arguments
        bool missingSomething = false;
        StringBuilder sb;
        sb.AppendAtomic("Missing required arguments:");
        sb.NewLine(2);
        for(const auto& arg : positionalArgs_) {
            if ((flags & kRequired || arg.flags & kRequired) && !arg.wasSet) {
                missingSomething = true;
                sb.AppendChar(' ', 4);
                AppendNameAndType(arg, sb, 0, flags);
                sb.NewLine();
            }
        }
        for(const auto& arg : args_) {
            if ((flags & kRequired || arg.flags & kRequired) && !arg.wasSet) {
                missingSomething = true;
                sb.AppendChar(' ', 4);
                AppendNameAndType(arg, sb, 0, flags);
                sb.NewLine();
            }
        }
        if (missingSomething) {
            PrintUsage(flags);
            auto sv = sb.GetStringView();
            ReportError("%.*s\n", static_cast<int>(sv.size()), sv.data());
            if (!(flags & kNoExitOnError)) {
                std::exit(1);
            }
        }

        return {t};
    }

    // Prints the full usage string to stdout
    void PrintUsage(uint64_t flags, int argc = 0, char** const argv = nullptr) const {
	(void)argc;
        StringBuilder usageBuilder; // For building the first usage lines
        StringBuilder descriptionBuilder; // For building the description line per argument

        // Default construct a T, for type info of ArrayPointers and default values
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
        
        if (!positionalArgs_.empty()) {
            descriptionBuilder.AppendAtomic(0, "Positional arguments:");
            descriptionBuilder.NewLine(2);
        }

        for (const auto& arg : positionalArgs_) {
            DescribeArg(arg, t, usageIndent, usageBuilder, descriptionBuilder, flags);
        }
        
        if (!args_.empty()) {
            descriptionBuilder.AppendAtomic(0, "Optional arguments:");
            descriptionBuilder.NewLine(2);
        }

        for (const auto& arg : args_) {
            DescribeArg(arg, t, usageIndent, usageBuilder, descriptionBuilder, flags);
        }

        auto sv = usageBuilder.GetStringView();
        printf("%.*s\n\n", static_cast<int>(sv.size()), sv.data());
        
        sv = descriptionBuilder.GetStringView();
        printf("%.*s", static_cast<int>(sv.size()), sv.data());
    }


private:
    template <typename U>
    struct DataPointer {
        using value_type = U;
        using container_type = U*;
        
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
    struct ArrayPointer {
        using value_type = U;
        using container_type = std::array<U, 0>*;

        template <size_t N>
        ArrayPointer(std::array<U, N>* array) : array_(array) {
            static_assert(N >= 1 && N <= 10, "Arrays from sizes 1 to 10 are supported");
        }
        template <size_t N>
        ArrayPointer(std::array<U, N> T::* array) : array_(array) {
            static_assert(N >= 1 && N <= 10, "Arrays from sizes 1 to 10 are supported");
        }
    
        U* Begin(T& t) {
            return std::visit([&t](auto array) { return &array.Get(t).front(); }, array_);
        }
        const U* Begin(const T& t) const {
            return std::visit([&t](const auto array) { return &array.Get(t).front(); }, array_);
        }
        U* End(T& t) {
            return std::visit([&t](auto array) { return &array.Get(t).back()+1; }, array_);
        }
        const U* End(const T& t) const {
            return std::visit([&t](const auto array) { return &array.Get(t).back()+1; }, array_);
        }
        constexpr size_t Size() const {
            return std::visit([](const auto array) -> size_t { return std::tuple_size<DataTypeT<decltype(array.Get(T{}))>>::value; }, array_);
        }
        
        using ArrayVariant = std::variant<DataPointer<std::array<U, 1>>, DataPointer<std::array<U, 2>>, DataPointer<std::array<U, 3>>, DataPointer<std::array<U, 4>>, DataPointer<std::array<U, 5>>,
             DataPointer<std::array<U, 6>>, DataPointer<std::array<U, 7>>, DataPointer<std::array<U, 8>>, DataPointer<std::array<U, 9>>, DataPointer<std::array<U, 10>>>;
        ArrayVariant array_;
    };

    template <typename U>
    struct VectorPointer : public DataPointer<std::vector<U>> {
        using value_type = U;
        using container_type = std::vector<U>*;

        const size_t minArgs;
        const size_t maxArgs;
    };

    template <typename>
    struct UserPointer;

    // Vec(float, float, float)
    template <typename U, typename ...Args>
    struct UserPointer<U(Args...)> : public DataPointer<U> {
        using value_type = U;
        using container_type = UserContainer;
        
        UserPointer(U* v) : DataPointer<U>(v) {};
        UserPointer(U T::* v) : DataPointer<U>(v) {};

        bool Parse(T& t, ParseState state) {
            auto ParseAllOptionals = [this, &t](auto&&... optionals) {
                bool hasValues = (optionals.has_value() && ...);
                if (!hasValues) {
                    return false;
                }
                if constexpr (std::is_aggregate_v<U>) {
                    this->Get(t) = {U{std::forward<decltype(optionals.value())>(optionals.value())...}};
                } else {
                    this->Get(t) = {U(std::forward<decltype(optionals.value())>(optionals.value())...)};
                }
                return true;
            };
            return ParseAllOptionals(std::forward<decltype(::clue::Parse<Args>(state))>(::clue::Parse<Args>(state))...);
        }

        void AppendTypeString(StringBuilder& sb) const {
            StringBuilder typesBuilder(64);
            (..., typesBuilder.AppendAtomic(0, "%s ", TypeInfo<Args>::String()));
            auto sv = typesBuilder.GetStringView();
            sb.AppendAtomic("%.*s", static_cast<int>(sv.size()-1), sv.data());
        }
    };

    using ArgumentVariant = std::variant<DataPointer<int>, ArrayPointer<int>, VectorPointer<int>,
        DataPointer<float>, ArrayPointer<float>, VectorPointer<float>, 
        DataPointer<double>, ArrayPointer<double>, VectorPointer<double>,
        DataPointer<bool>,
        DataPointer<std::string_view>, ArrayPointer<std::string_view>, VectorPointer<std::string_view>,
        DataPointer<std::string>, ArrayPointer<std::string>, VectorPointer<std::string>,
        UserPointer<UserTypes>...>;
    
    struct Arg {
        ArgumentVariant argument;
        std::string_view name;
        std::string_view description;
        ParseFlags flags = kNone;
        const bool isPositional = false;
        bool wasSet = false;
    };

    template <typename V>
    Arg MakeArg(V* value, std::string_view name, std::string_view description, ParseFlags flags, bool isPositional) {
        return Arg{value, name, description, flags, isPositional};
    }
    template <typename MemberT>
    Arg MakeArg(MemberT T::* member, std::string_view name, std::string_view description, ParseFlags flags, bool isPositional) {
        return Arg{member, name, description, flags, isPositional};
    }
    template <size_t MinArgs, size_t MaxArgs, typename U>
    Arg MakeArg(std::vector<U>* vector, std::string_view name, std::string_view description, ParseFlags flags, bool isPositional) {
        return Arg{VectorPointer<U>{vector, MinArgs, MaxArgs}, name, description, flags, isPositional};
    }
    template <size_t MinArgs, size_t MaxArgs, typename U>
    Arg MakeArg(std::vector<U> T::* vector, std::string_view name, std::string_view description, ParseFlags flags, bool isPositional) {
        return Arg{VectorPointer<U>{vector, MinArgs, MaxArgs}, name, description, flags, isPositional};
    }

    template <typename A>
    bool ParseArray(T& t, A&& arrayPtr, ParseState state) {
        auto* curr = arrayPtr.Begin(t);
        auto* end = arrayPtr.End(t);
        while (curr != end) {
            auto v = Parse<typename std::remove_reference_t<A>::value_type>(state); 
            if (!v) {
                return false;
            }
            *curr = v.value();
            curr++;
        }
        return true;
    }
    
    template <typename Vec>
    size_t ParseVector(T& t, Vec&& vecPtr, ParseState state) {
        // For vectors we just consume until we can no longer consume any more
        auto& vec = vecPtr.Get(t);
        vec.clear();
        size_t i = 0;
        for (; *state.argIndex <= state.argc; ++i) {
            int nextIndex = (*state.argIndex) + 1;
            // Stop consuming if it looks like we're about to step onto a named arg
            if (nextIndex < state.argc) {
                std::string_view token = std::string_view{state.argv[nextIndex]};
                if (token.size() >= 1) {
                    bool looksLikeAnArg = false;
                    for (const auto& arg : args_) {
                        if (arg.name == token.substr(1)) {
                            looksLikeAnArg = true;
                            break;
                        }
                    }
                    if (looksLikeAnArg) {
                        break;
                    }
                }
            }
            state.reportErrors = false;
            auto v = Parse<typename std::remove_reference_t<Vec>::value_type>(state); 
            if (!v) {
                (*state.argIndex) -= 1;
                break;
            }
            vec.push_back(v.value());
        }
        return i;
    }

    void AppendNameAndType(const Arg& arg, StringBuilder& stringBuilder, int indent, uint64_t flags) {
        std::visit([&](auto&& a) { 
            using ValueType = typename std::remove_reference_t<decltype(a)>::value_type;
            using ContainerType = typename std::remove_reference_t<decltype(a)>::container_type;

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
            if constexpr (std::is_same_v<bool, ValueType>) {
                stringBuilder.AppendAtomic(indent, "%s%s%.*s%s", usagePrefix, namePrefix, argNameLen, argNameData, usageSuffix);
            } else if constexpr (std::is_same_v<ValueType*, ContainerType>) {
                const char* typeString = TypeInfo<ValueType>::String();
                stringBuilder.AppendAtomic(indent, "%s%s%.*s <%s>%s", usagePrefix, namePrefix, argNameLen, argNameData, typeString, usageSuffix);
            } else if constexpr (std::is_same_v<std::array<ValueType, 0>*, ContainerType>) {
                auto size = a.Size();
                const char* typeString = TypeInfo<ValueType>::String();
                stringBuilder.AppendAtomic(indent, "%s%s%.*s <%s[%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, typeString, size, usageSuffix);
            } else if constexpr (std::is_same_v<std::vector<ValueType>*, ContainerType>) {
                auto minArgs = a.minArgs;
                auto maxArgs = a.maxArgs;
                const char* typeString = TypeInfo<ValueType>::String();
                if (minArgs != 0 && maxArgs != std::numeric_limits<size_t>::max()) {
                    stringBuilder.AppendAtomic(indent, "%s%s%.*s <%s[%zu:%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, typeString, minArgs, maxArgs, usageSuffix);
                } else if (minArgs != 0 && maxArgs == std::numeric_limits<size_t>::max()) {
                    stringBuilder.AppendAtomic(indent, "%s%s%.*s <%s[%zu:]>%s", usagePrefix, namePrefix, argNameLen, argNameData, typeString, minArgs, usageSuffix);
                } else if (minArgs == 0 && maxArgs != std::numeric_limits<size_t>::max()) {
                    stringBuilder.AppendAtomic(indent, "%s%s%.*s <%s[:%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, typeString, maxArgs, usageSuffix);
                } else {
                    stringBuilder.AppendAtomic(indent, "%s%s%.*s <%s[...]>%s", usagePrefix, namePrefix, argNameLen, argNameData, typeString, usageSuffix);
                }
            } else if constexpr (std::is_same_v<UserContainer, ContainerType>) {
                StringBuilder userBuilder(64);
                a.AppendTypeString(userBuilder);
                auto sv = userBuilder.GetStringView();
                stringBuilder.AppendAtomic(indent, "%s%s%.*s <%s>%s", usagePrefix, namePrefix, argNameLen, argNameData, sv.data(), usageSuffix);
            } else {
                static_assert(AlwaysFalse<ValueType>::value, "Unhandled Argument Type");
            }
        }, arg.argument);
    }

    void DescribeArg(const Arg& arg, const T& t, int usageIndent, StringBuilder& usageBuilder, StringBuilder& descriptionBuilder, uint64_t flags) const {
        int descriptionIndent = 0;
        StringBuilder defaultBuilder(64); // used for building strings for default values

        std::visit([&](auto&& a) {
            using ValueType = typename std::remove_reference_t<decltype(a)>::value_type;
            using ContainerType = typename std::remove_reference_t<decltype(a)>::container_type;

            auto ArrayDefault = [&defaultBuilder](auto arrayBegin, size_t size) {
                if (size == 0) return;
                if constexpr (std::is_same_v<std::string, ValueType>) {
                    for (size_t i = 0; i < size - 1; ++i) {
                        defaultBuilder.AppendAtomic("%s ", arrayBegin[i].c_str());
                    }
                    defaultBuilder.AppendAtomic("%s", arrayBegin[size-1].c_str());
                } else if constexpr (std::is_same_v<std::string_view, ValueType>) {
                    for (size_t i = 0; i < size - 1; ++i) {
                        defaultBuilder.AppendAtomic("%.*s ", static_cast<int>(arrayBegin[i].size()), arrayBegin[i].data());
                    }
                    defaultBuilder.AppendAtomic("%.*s ", static_cast<int>(arrayBegin[size-1].size()), arrayBegin[size-1].data());
                } else {
                    for (size_t i = 0; i < size - 1; ++i) {
                        defaultBuilder.AppendAtomic("%s ", to_string(arrayBegin[i]).c_str());
                    }
                    defaultBuilder.AppendAtomic("%s", to_string(arrayBegin[size-1]).c_str());
                }
            };

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

            usageBuilder.AppendChar(' ');
            descriptionBuilder.AppendChar(' ', 4);
            if constexpr (std::is_same_v<bool, ValueType>) {
                (void)ArrayDefault;
                usageBuilder.AppendAtomic(usageIndent, "%s%s%.*s%s", usagePrefix, namePrefix, argNameLen, argNameData, usageSuffix);

                descriptionIndent = FormattedLength("    %s%.*s", namePrefix, argNameLen, argNameData);
                descriptionBuilder.AppendAtomic(0, "%s%.*s", namePrefix, argNameLen, argNameData);

                defaultBuilder.AppendAtomic("%s", a.Get(t) ? "true" : "false");
            } else if constexpr (std::is_same_v<ValueType*, ContainerType>) {
                (void)ArrayDefault;
                const char* typeString = TypeInfo<ValueType>::String();
                usageBuilder.AppendAtomic(usageIndent, "%s%s%.*s <%s>%s", usagePrefix, namePrefix, argNameLen, argNameData, typeString, usageSuffix);

                descriptionIndent = FormattedLength("    %s%.*s <%s>", namePrefix, argNameLen, argNameData, typeString);
                descriptionBuilder.AppendAtomic(0, "%s%.*s <%s>", namePrefix, argNameLen, argNameData, typeString);

                if constexpr (std::is_same_v<std::string_view, ValueType>) {
                    defaultBuilder.AppendAtomic("%.*s", static_cast<int>(a.Get(t).size()), a.Get(t).data());
                } else if constexpr(std::is_same_v<std::string, ValueType>) {
                    defaultBuilder.AppendAtomic("%s", a.Get(t).c_str());
                } else {
                    defaultBuilder.AppendAtomic("%s", to_string(a.Get(t)).c_str());
                }
            } else if constexpr (std::is_same_v<std::array<ValueType, 0>*, ContainerType>) {
                const char* typeString = TypeInfo<ValueType>::String();
                auto size = a.Size();
                usageBuilder.AppendAtomic(usageIndent, "%s%s%.*s <%s[%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, typeString, size, usageSuffix);

                descriptionIndent = FormattedLength("    %s%.*s <%s[%zu]>", namePrefix, argNameLen, argNameData, typeString, size);
                descriptionBuilder.AppendAtomic(0, "%s%.*s <%s[%zu]>", namePrefix, argNameLen, argNameData, typeString, size);
                
                ArrayDefault(a.Begin(t), size);
            } else if constexpr (std::is_same_v<std::vector<ValueType>*, ContainerType>) {
                const char* typeString = TypeInfo<ValueType>::String();
                auto minArgs = a.minArgs;
                auto maxArgs = a.maxArgs;
                ArrayDefault(a.Get(t).begin(), a.Get(t).size());

                if (minArgs != 0 && maxArgs != std::numeric_limits<size_t>::max()) {
                    usageBuilder.AppendAtomic(usageIndent, "%s%s%.*s <%s[%zu:%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, typeString, minArgs, maxArgs, usageSuffix);
                    descriptionIndent = FormattedLength("    %s%.*s <%s[%zu:%zu]>", namePrefix, argNameLen, argNameData, typeString, minArgs, maxArgs);
                    descriptionBuilder.AppendAtomic(0, "%s%.*s <%s[%zu:%zu]>", namePrefix, argNameLen, argNameData, typeString, minArgs, maxArgs);
                } else if (minArgs != 0 && maxArgs == std::numeric_limits<size_t>::max()) {
                    usageBuilder.AppendAtomic(usageIndent, "%s%s%.*s <%s[%zu:]>%s", usagePrefix, namePrefix, argNameLen, argNameData, typeString, minArgs, usageSuffix);
                    descriptionIndent = FormattedLength("    %s%.*s <%s[%zu:]>", namePrefix, argNameLen, argNameData, typeString, minArgs);
                    descriptionBuilder.AppendAtomic(0, "%s%.*s <%s[%zu:]>", namePrefix, argNameLen, argNameData, typeString, minArgs);
                } else if (minArgs == 0 && maxArgs != std::numeric_limits<size_t>::max()) {
                    usageBuilder.AppendAtomic(usageIndent, "%s%s%.*s <%s[:%zu]>%s", usagePrefix, namePrefix, argNameLen, argNameData, typeString, maxArgs, usageSuffix);
                    descriptionIndent = FormattedLength("    %s%.*s <%s[:%zu]>", namePrefix, argNameLen, argNameData, typeString, maxArgs);
                    descriptionBuilder.AppendAtomic(0, "%s%.*s <%s[:%zu]>", namePrefix, argNameLen, argNameData, typeString, maxArgs);
                } else {
                    usageBuilder.AppendAtomic(usageIndent, "%s%s%.*s <%s[...]>%s", usagePrefix, namePrefix, argNameLen, argNameData, typeString, usageSuffix);
                    descriptionIndent = FormattedLength("    %s%.*s <%s[...]>", namePrefix, argNameLen, argNameData, typeString);
                    descriptionBuilder.AppendAtomic(0, "%s%.*s <%s[...]>", namePrefix, argNameLen, argNameData, typeString);
                }
            } else if constexpr (std::is_same_v<UserContainer, ContainerType>) {
                (void)ArrayDefault;
                StringBuilder userBuilder(64);
                a.AppendTypeString(userBuilder);
                auto sv = userBuilder.GetStringView();
                usageBuilder.AppendAtomic(usageIndent, "%s%s%.*s <%s>%s", usagePrefix, namePrefix, argNameLen, argNameData, sv.data(), usageSuffix);

                descriptionIndent = FormattedLength("    %s%.*s <%s>", namePrefix, argNameLen, argNameData, sv.data());
                descriptionBuilder.AppendAtomic(0, "%s%.*s <%s>", namePrefix, argNameLen, argNameData, sv.data());

                flags |= kNoDefault;
            } else {
                static_assert(AlwaysFalse<ValueType>::value, "Unhandled Argument Type");
            }
        }, arg.argument);
                    
        // Append every argument's description
        if ((flags & kRequired) || (arg.flags & kRequired)) {
            descriptionBuilder.AppendAtomic(descriptionIndent, " (Required): ");
        } else {
            descriptionBuilder.AppendAtomic(": ");
        }
        descriptionBuilder.AppendNatural(descriptionIndent, arg.description.data(), static_cast<int>(arg.description.size()));

        if (!(flags & kNoDefault) && !(arg.flags & kNoDefault)) {
            auto sv = defaultBuilder.GetStringView();
            descriptionBuilder.AppendChar(' ');
            descriptionBuilder.AppendAtomic(descriptionIndent, "(Default: %.*s)", static_cast<int>(sv.size()), sv.data());
        }
        descriptionBuilder.NewLine(2);
    }

    std::string_view name_;
    std::string_view description_;
    std::vector<Arg> args_;
    std::vector<Arg> positionalArgs_;
};
    
template <>
std::optional<int> Parse<int>(ParseState state) {
    (*state.argIndex)++;
    if (*state.argIndex >= state.argc) {
        if (state.reportErrors) {
            ReportError("\"%.*s\" expected an int value\n", state.argNameLen, state.argName);
        }
        return {};
    }
    auto valueToken = std::string_view(state.argv[*state.argIndex]);
    auto valueTokenData = valueToken.data();
    auto valueTokenLen = static_cast<int>(valueToken.size());
    char* end = nullptr;
    int64_t v = strtol(valueTokenData, &end, 10);
    if (v == 0 && end == valueTokenData) {
        if (state.reportErrors) {
            ReportError("\"%.*s\" expected a string representing an int but instead found \"%.*s\"\n", state.argNameLen, state.argName, valueTokenLen, valueTokenData);
        }
        return {};
    } else if (v < static_cast<int64_t>(std::numeric_limits<int>::min()) || v > static_cast<int64_t>(std::numeric_limits<int>::max())) {
        if (state.reportErrors) {
            ReportError("\"%.*s\" int value \"%.*s\" out of range [%d, %d]\n",
            state.argNameLen, state.argName, valueTokenLen, valueTokenData, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        }
        return {};
    }
    return {static_cast<int>(v)};
}

template <>
std::optional<float> Parse<float>(ParseState state) {
    (*state.argIndex)++;
    if (*state.argIndex >= state.argc) {
        if (state.reportErrors) {
            ReportError("\"%.*s\" expected a float value\n", state.argNameLen, state.argName);
        }
        return {};
    }
    auto valueToken = std::string_view(state.argv[*state.argIndex]);
    auto valueTokenData = valueToken.data();
    auto valueTokenLen = static_cast<int>(valueToken.size());
    
    float v = strtof(valueTokenData, nullptr);
    // TODO: 0 could be from garbage strings!
    if (v == HUGE_VAL || v == HUGE_VALF || v == HUGE_VALL) {
        if (state.reportErrors) {
            ReportError("\"%.*s\" float value \"%.*s\" out of range\n", state.argNameLen, state.argName, valueTokenLen, valueTokenData);
        }
        return {};
    }
    return {v};
}

template <>
std::optional<double> Parse<double>(ParseState state) {
    (*state.argIndex)++;
    if (*state.argIndex >= state.argc) {
        if (state.reportErrors) {
            ReportError("\"%.*s\" expected a double value\n", state.argNameLen, state.argName);
        }
        return {};
    }
    auto valueToken = std::string_view(state.argv[*state.argIndex]);
    auto valueTokenData = valueToken.data();
    auto valueTokenLen = static_cast<int>(valueToken.size());
    
    double v = strtod(valueToken.data(), nullptr);
    if (v == HUGE_VAL || v == HUGE_VALF || v == HUGE_VALL) {
        if (state.reportErrors) {
            ReportError("\"%.*s\" double value \"%.*s\" out of range\n", state.argNameLen, state.argName, valueTokenLen, valueTokenData);
        }
        return {};
    }
    return {v};
}

template <>
std::optional<std::string> Parse<std::string>(ParseState state) {
    (*state.argIndex)++;
    if (*state.argIndex >= state.argc) {
        if (state.reportErrors) {
            ReportError("\"%.*s\" expected a string value\n", state.argNameLen, state.argName);
        }
        return {};
    }
    return {state.argv[*state.argIndex]};
}

template <>
std::optional<std::string_view> Parse<std::string_view>(ParseState state) {
    (*state.argIndex)++;
    if (*state.argIndex >= state.argc) {
        if (state.reportErrors) {
            ReportError("\"%.*s\" expected a string value\n", state.argNameLen, state.argName);
        }
        return {};
    }
    return {state.argv[*state.argIndex]};
}

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
}

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

template <> struct TypeInfo<int> {
    static const char* String() { return "int"; }
};
template <> struct TypeInfo<float> {
    static const char* String() { return "float"; }
};
template <> struct TypeInfo<double> {
    static const char* String() { return "double"; }
};
template <> struct TypeInfo<bool> {
    static const char* String() { return ""; } // purposefully blank as bools are flags
};
template <> struct TypeInfo<std::string> {
    static const char* String() { return "string"; }
};
template <> struct TypeInfo<std::string_view> {
    static const char* String() { return "string"; }
};
template <typename T> struct TypeInfo {
    static const char* String() { return ""; }
};

} // namespace clue


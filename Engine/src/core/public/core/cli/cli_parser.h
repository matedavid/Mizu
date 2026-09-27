#pragma once

#include <algorithm>
#include <charconv>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace Mizu
{
namespace cli
{

template <size_t N>
struct fixed_string
{
    char data[N];

    constexpr fixed_string(const char (&s)[N]) { std::copy_n(s, N, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

struct UnclaimedOption
{
    std::string_view name;
    std::string_view value;
};

template <fixed_string Key, fixed_string LongArg, fixed_string ShortArg, typename T>
struct CliArgument
{
    using ValueType = T;

    static constexpr std::string_view key = Key.view();
    static constexpr std::string_view long_arg = LongArg.view();
    static constexpr std::string_view short_arg = ShortArg.view();

    bool is_argument = false;
    bool is_flag = false;
    std::string_view description{};
    std::optional<T> value{};
};

template <typename... Args>
class CliParser;

template <typename... Args>
class CliParserResult
{
  public:
    explicit CliParserResult(std::tuple<Args...> values) : m_values(std::move(values)) {}

    template <fixed_string Key>
    constexpr const auto& get() const
    {
        return get_impl<Key>(m_values);
    }

    template <fixed_string Key, typename T>
    constexpr auto get_or(T&& default_value) const
    {
        const auto& opt_value = get_impl<Key>(m_values);
        return opt_value.value_or(std::forward<T>(default_value));
    }

    const std::vector<UnclaimedOption>& get_unclaimed_options() const { return m_unclaimed; }

    std::string_view program_name() const { return m_program_name; }

    bool help_requested() const { return m_help_requested; }
    bool parse_failed() const { return m_parse_failed; }

    std::string help_text() const
    {
        std::ostringstream usage;
        std::ostringstream arguments;
        std::ostringstream options;

        usage << "Usage: " << m_program_name << " [options]";

        std::apply([&](const Args&... opt) { (append_help_entry(opt, usage, arguments, options), ...); }, m_values);

        options << "  --help, -h    Show this help message\n";

        std::ostringstream out;
        out << usage.str() << "\n";

        const std::string args_str = arguments.str();
        if (!args_str.empty())
            out << "\nArguments:\n" << args_str;

        out << "\nOptions:\n" << options.str();

        return out.str();
    }

    void print_help() const { std::cout << help_text(); }

  private:
    std::tuple<Args...> m_values{};
    std::vector<UnclaimedOption> m_unclaimed{};

    std::string m_program_name{};
    bool m_help_requested = false;
    bool m_parse_failed = false;

    template <fixed_string Key, size_t I = 0>
    static constexpr const auto& get_impl(const std::tuple<Args...>& values)
    {
        static_assert(I < sizeof...(Args), "Cli key not registered");

        using Current = std::tuple_element_t<I, std::tuple<Args...>>;

        if constexpr (Current::key == Key.view())
        {
            return std::get<I>(values).value;
        }
        else
        {
            return get_impl<Key, I + 1>(values);
        }
    }

    template <typename Option>
    static void append_help_entry(
        const Option& opt,
        std::ostringstream& usage,
        std::ostringstream& arguments,
        std::ostringstream& options)
    {
        if (opt.is_argument)
        {
            usage << " <" << opt.key << ">";
            arguments << "  " << opt.key;
            arguments << "    " << opt.description;
            arguments << "\n";
        }
        else
        {
            options << "  ";
            if (!opt.long_arg.empty())
                options << opt.long_arg;
            if (!opt.long_arg.empty() && !opt.short_arg.empty())
                options << ", ";
            if (!opt.short_arg.empty())
                options << opt.short_arg;
            if (!opt.is_flag)
                options << " <value>";
            options << "    " << opt.description;
            options << "\n";
        }
    }

    template <typename...>
    friend class CliParser;
};

template <typename... Args>
class CliParser
{
  public:
    constexpr explicit CliParser() = default;
    constexpr explicit CliParser(std::tuple<Args...> values) : m_values(std::move(values)) {}

    template <fixed_string Key, typename T>
    constexpr auto add_argument(std::string_view description)
    {
        return add_impl<Key, "", "", T>(true, false, description, std::nullopt);
    }

    template <fixed_string Key, fixed_string LongArg, fixed_string ShortArg, typename T>
    constexpr auto add_option(std::string_view description)
    {
        return add_impl<Key, LongArg, ShortArg, T>(false, false, description, std::nullopt);
    }

    template <fixed_string Key, fixed_string LongArg, fixed_string ShortArg>
    constexpr auto add_flag(std::string_view description)
    {
        return add_impl<Key, LongArg, ShortArg, bool>(false, true, description, false);
    }

    CliParserResult<Args...> parse(int32_t argc, const char* argv[]) const
    {
        CliParserResult<Args...> result{m_values};
        result.m_program_name = extract_program_name(argc > 0 ? argv[0] : "");

        size_t target_positional = 0;

        int32_t i = 1;
        while (i < argc)
        {
            std::string_view token{argv[i]};

            if (token == "--help" || token == "-h")
            {
                result.m_help_requested = true;
                ++i;
                continue;
            }

            if (is_option_token(token))
            {
                bool matched = false;
                bool consumed_value = false;

                std::apply(
                    [&](Args&... opt) { (try_match_option(opt, token, i, argc, argv, matched, consumed_value), ...); },
                    result.m_values);

                if (!matched)
                {
                    std::string_view value{};
                    const bool has_value = (i + 1 < argc) && !is_option_token(std::string_view{argv[i + 1]});
                    if (has_value)
                        value = std::string_view{argv[i + 1]};

                    result.m_unclaimed.push_back(UnclaimedOption{token.substr(2), value});
                    i += has_value ? 2 : 1;
                }
                else
                {
                    i += consumed_value ? 2 : 1;
                }
            }
            else
            {
                size_t current_positional = 0;
                bool positional_matched = false;
                std::apply(
                    [&](Args&... opt) {
                        (try_assign_positional(opt, token, target_positional, current_positional, positional_matched),
                         ...);
                    },
                    result.m_values);
                ++i;
            }
        }

        const bool all_arguments_set = std::apply(
            [](const Args&... opt) {
                bool all_set = true;
                ((all_set = all_set && (!opt.is_argument || opt.value.has_value())), ...);
                return all_set;
            },
            result.m_values);

        result.m_parse_failed = !all_arguments_set;

        return result;
    }

  private:
    std::tuple<Args...> m_values{};

    template <fixed_string Key, fixed_string LongArg, fixed_string ShortArg, typename T>
    constexpr auto add_impl(bool is_argument, bool is_flag, std::string_view description, std::optional<T> value)
    {
        using NewArgument = CliArgument<Key, LongArg, ShortArg, T>;

        validate_arg_prefixes<LongArg, ShortArg>();
        (validate_long_short_arg<LongArg, ShortArg, Args>(), ...);

        return CliParser<Args..., NewArgument>{std::tuple_cat(
            m_values, std::make_tuple<NewArgument>(NewArgument{is_argument, is_flag, description, value}))};
    }

    template <fixed_string LongArg, fixed_string ShortArg, typename Option>
    constexpr void validate_long_short_arg()
    {
        static_assert(LongArg.view().empty() || Option::long_arg != LongArg.view(), "Duplicated long argument");
        static_assert(ShortArg.view().empty() || Option::short_arg != ShortArg.view(), "Duplicated short argument");
    }

    template <fixed_string LongArg, fixed_string ShortArg>
    constexpr void validate_arg_prefixes()
    {
        static_assert(
            LongArg.view().empty() || LongArg.view().starts_with("--"), "Long argument must start with \"--\"");

        static_assert(
            ShortArg.view().empty() || (ShortArg.view().starts_with("-") && !ShortArg.view().starts_with("--")),
            "Short argument must start with \"-\" and not \"--\"");
    }

    static constexpr bool is_option_token(std::string_view token) { return token.size() > 1 && token[0] == '-'; }

    static std::string extract_program_name(std::string_view full_path)
    {
        const size_t slash = full_path.find_last_of("/\\");
        return std::string{slash == std::string_view::npos ? full_path : full_path.substr(slash + 1)};
    }

    template <typename Option>
    static constexpr void try_match_option(
        Option& opt,
        std::string_view token,
        int32_t i,
        int32_t argc,
        const char* argv[],
        bool& matched,
        bool& consumed_value)
    {
        if (matched || opt.is_argument)
            return;

        if ((!opt.long_arg.empty() && token == opt.long_arg) || (!opt.short_arg.empty() && token == opt.short_arg))
        {
            matched = true;

            if constexpr (std::is_same_v<typename Option::ValueType, bool>)
            {
                if (opt.is_flag)
                {
                    opt.value = true;
                    consumed_value = false;
                    return;
                }
            }

            if (i + 1 < argc)
            {
                opt.value = parse_value<typename Option::ValueType>(std::string_view{argv[i + 1]});
                consumed_value = true;
            }
        }
    }

    template <typename Option>
    static constexpr void try_assign_positional(
        Option& opt,
        std::string_view token,
        size_t& target_index,
        size_t& current_index,
        bool& matched)
    {
        if (!opt.is_argument || matched)
            return;

        if (current_index == target_index)
        {
            opt.value = parse_value<typename Option::ValueType>(token);
            ++target_index;
            matched = true;
        }

        ++current_index;
    }

    template <typename T>
    static constexpr T parse_value(std::string_view text)
    {
        if constexpr (std::is_same_v<T, std::string_view>)
        {
            return text;
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            return std::string{text};
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            return text == "true" || text == "1" || text == "yes";
        }
        else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>)
        {
            T result{};
            std::from_chars(text.data(), text.data() + text.size(), result);
            return result;
        }
        else
        {
            static_assert(!sizeof(T*), "Unsupported Cli value type");
        }
    }
};

} // namespace cli
} // namespace Mizu
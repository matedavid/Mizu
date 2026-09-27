#include <catch2/catch_all.hpp>

#include <string>

#include "core/cli/cli_parser.h"

using namespace Mizu;
using namespace Mizu::cli;

namespace
{

auto make_argument_parser()
{
    return CliParser<>{}.add_argument<"input", std::string>("Input file path");
}

auto make_multi_argument_parser()
{
    return CliParser<>{}
        .add_argument<"source", std::string>("Source file")
        .add_argument<"destination", std::string>("Destination file");
}

auto make_option_parser()
{
    return CliParser<>{}.add_option<"output", "--output", "-o", std::string>("Output file path");
}

auto make_flag_parser()
{
    return CliParser<>{}.add_flag<"verbose", "--verbose", "-v">("Enable verbose output");
}

auto make_numeric_parser()
{
    return CliParser<>{}
        .add_option<"count", "--count", "-c", int32_t>("Number of iterations")
        .add_option<"ratio", "--ratio", "-r", double>("Some ratio value");
}

auto make_full_parser()
{
    return CliParser<>{}
        .add_argument<"input", std::string>("Input file path")
        .add_option<"output", "--output", "-o", std::string>("Output file path")
        .add_flag<"verbose", "--verbose", "-v">("Enable verbose output");
}

} // namespace

TEST_CASE("CliParser parses a required positional argument", "[Base][Cli]")
{
    auto parser = make_argument_parser();

    const char* argv[] = {"myapp", "input.txt"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    REQUIRE(!result.parse_failed());
    REQUIRE(result.get<"input">().has_value());
    REQUIRE(result.get<"input">().value() == "input.txt");
}

TEST_CASE("CliParser marks parsing as failed when a required argument is missing", "[Base][Cli]")
{
    auto parser = make_argument_parser();

    const char* argv[] = {"myapp"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    REQUIRE(result.parse_failed());
    REQUIRE(!result.get<"input">().has_value());
}

TEST_CASE("CliParser assigns multiple positional arguments in declaration order", "[Base][Cli]")
{
    auto parser = make_multi_argument_parser();

    const char* argv[] = {"myapp", "a.txt", "b.txt"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    REQUIRE(!result.parse_failed());
    REQUIRE(result.get<"source">().value() == "a.txt");
    REQUIRE(result.get<"destination">().value() == "b.txt");
}

TEST_CASE("CliParser fails when only some positional arguments are provided", "[Base][Cli]")
{
    auto parser = make_multi_argument_parser();

    const char* argv[] = {"myapp", "a.txt"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    REQUIRE(result.parse_failed());
    REQUIRE(result.get<"source">().value() == "a.txt");
    REQUIRE(!result.get<"destination">().has_value());
}

TEST_CASE("CliParser parses an option given in its long form", "[Base][Cli]")
{
    auto parser = make_option_parser();

    const char* argv[] = {"myapp", "--output", "result.txt"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    REQUIRE(result.get<"output">().value() == "result.txt");
}

TEST_CASE("CliParser parses an option given in its short form", "[Base][Cli]")
{
    auto parser = make_option_parser();

    const char* argv[] = {"myapp", "-o", "result.txt"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    REQUIRE(result.get<"output">().value() == "result.txt");
}

TEST_CASE("CliParser get_or returns the default value when an option is not provided", "[Base][Cli]")
{
    auto parser = make_option_parser();

    const char* argv[] = {"myapp"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    REQUIRE(!result.get<"output">().has_value());
    REQUIRE(result.get_or<"output">(std::string{"default.txt"}) == "default.txt");
}

TEST_CASE("CliParser get_or returns the parsed value when an option is provided", "[Base][Cli]")
{
    auto parser = make_option_parser();

    const char* argv[] = {"myapp", "--output", "result.txt"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    REQUIRE(result.get_or<"output">(std::string{"default.txt"}) == "result.txt");
}

TEST_CASE("CliParser flag defaults to false when not present", "[Base][Cli]")
{
    auto parser = make_flag_parser();

    const char* argv[] = {"myapp"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    REQUIRE(result.get<"verbose">().has_value());
    REQUIRE(result.get<"verbose">().value() == false);
}

TEST_CASE("CliParser parses a flag as true when given in its long form", "[Base][Cli]")
{
    auto parser = make_flag_parser();

    const char* argv[] = {"myapp", "--verbose"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    REQUIRE(result.get<"verbose">().value() == true);
}

TEST_CASE("CliParser parses a flag as true when given in its short form", "[Base][Cli]")
{
    auto parser = make_flag_parser();

    const char* argv[] = {"myapp", "-v"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    REQUIRE(result.get<"verbose">().value() == true);
}

TEST_CASE("CliParser parses integer and floating point option values", "[Base][Cli]")
{
    auto parser = make_numeric_parser();

    const char* argv[] = {"myapp", "--count", "42", "--ratio", "3.5"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    REQUIRE(result.get<"count">().value() == 42);
    REQUIRE(result.get<"ratio">().value() == Catch::Approx(3.5));
}

TEST_CASE("CliParser stores an unrecognized option with a following value as unclaimed", "[Base][Cli]")
{
    auto parser = make_option_parser();

    const char* argv[] = {"myapp", "--unknown", "value"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    const auto& unclaimed = result.get_unclaimed_options();
    REQUIRE(unclaimed.size() == 1);
    REQUIRE(unclaimed[0].name == "unknown");
    REQUIRE(unclaimed[0].value == "value");
}

TEST_CASE("CliParser stores an unrecognized option without a following value as unclaimed", "[Base][Cli]")
{
    auto parser = make_option_parser();

    const char* argv[] = {"myapp", "--unknown", "--output", "result.txt"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    const auto& unclaimed = result.get_unclaimed_options();
    REQUIRE(unclaimed.size() == 1);
    REQUIRE(unclaimed[0].name == "unknown");
    REQUIRE(unclaimed[0].value.empty());

    REQUIRE(result.get<"output">().value() == "result.txt");
}

TEST_CASE("CliParser recognizes --help and -h", "[Base][Cli]")
{
    auto parser = make_argument_parser();

    const char* argv_long[] = {"myapp", "--help"};
    const auto result_long = parser.parse(static_cast<int32_t>(std::size(argv_long)), argv_long);
    REQUIRE(result_long.help_requested());

    const char* argv_short[] = {"myapp", "-h"};
    const auto result_short = parser.parse(static_cast<int32_t>(std::size(argv_short)), argv_short);
    REQUIRE(result_short.help_requested());
}

TEST_CASE("CliParser extracts the program name from the executable path", "[Base][Cli]")
{
    auto parser = make_argument_parser();

    const char* argv[] = {"/usr/local/bin/myapp", "input.txt"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    REQUIRE(result.program_name() == "myapp");
}

TEST_CASE("CliParser help_text includes usage, argument and option descriptions", "[Base][Cli]")
{
    auto parser = make_full_parser();

    const char* argv[] = {"myapp", "in.txt"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    const std::string help = result.help_text();

    REQUIRE(help.find("Usage: myapp") != std::string::npos);
    REQUIRE(help.find("<input>") != std::string::npos);
    REQUIRE(help.find("Input file path") != std::string::npos);
    REQUIRE(help.find("--output, -o") != std::string::npos);
    REQUIRE(help.find("--verbose, -v") != std::string::npos);
    REQUIRE(help.find("--help, -h") != std::string::npos);
}

TEST_CASE("CliParser handles arguments, options, flags and unclaimed options together", "[Base][Cli]")
{
    auto parser = make_full_parser();

    const char* argv[] = {"myapp", "--verbose", "input.txt", "-o", "out.txt", "--extra", "thing"};
    const auto result = parser.parse(static_cast<int32_t>(std::size(argv)), argv);

    REQUIRE(!result.parse_failed());
    REQUIRE(result.get<"input">().value() == "input.txt");
    REQUIRE(result.get<"output">().value() == "out.txt");
    REQUIRE(result.get<"verbose">().value() == true);

    const auto& unclaimed = result.get_unclaimed_options();
    REQUIRE(unclaimed.size() == 1);
    REQUIRE(unclaimed[0].name == "extra");
    REQUIRE(unclaimed[0].value == "thing");
}
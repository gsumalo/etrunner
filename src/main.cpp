#include <gtest/gtest.h>
#include <boost/program_options.hpp>
#include "environment_dt.hpp"

namespace std {

static inline std::istream & operator>>(std::istream & is, std::pair<std::string, std::string> & into)
{
    char ch;

    while (is >> ch && ch!='=') {
        into.first += ch;
    }

    return is >> into.second;
}

}   // namespace std


static bool parse_params(int argc, char *argv[], boost::program_options::variables_map & vm)
{
    bool rv(true);

    boost::program_options::options_description desc("Allowed options", 160);
    try {
        desc.add_options()
            ("help", "Show Google Test help")
            ("test_spec",           boost::program_options::value<std::string>(),                             "Path to test specification (mandatory)")
            ("client",              boost::program_options::value<std::string>(),                             "FastDB client binary (mandatory)")
            ("maximum_concurrency", boost::program_options::value<uint64_t>()->default_value(0),              "Maximum level of concurrency (0 means no limit)")
            ("property,D",          boost::program_options::value<std::vector<std::pair<std::string,std::string>>>()->multitoken(), "Definition of property=value")
        ;

        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).run(), vm);
        boost::program_options::notify(vm);

        if (vm.count("help") || !vm.count("test_spec") || !vm.count("client")) {
            rv = false;
        }
    } catch (const std::exception & e) {
        std::cerr << e.what() << std::endl;
        rv = false;
    } catch(...) {
        rv = false;
    }

    if (!rv) {
        std::cerr << "Wrong syntax\n\n" << desc << std::endl;
    }

    return rv;
}

int main(int argc, char *argv[])
{
    int rv(EXIT_FAILURE);

    ::testing::InitGoogleTest(&argc, argv);

    boost::program_options::variables_map vm;
    auto & environment(dt::EnvironmentDT::instance());

    if (parse_params(argc, argv, vm) && environment.init(vm)) {
        try {
            const auto & tests(environment.test_spec());
            const auto & maximum_concurrency(environment.maximum_concurrency());
            const auto & client(environment.client());
            const auto & properties(environment.properties());

            for (auto & suite: tests.get_suites()) {
                suite->set_properties(properties);
            }

            for (const auto & test: tests.get_cases()) {
                dt::register_test(test, maximum_concurrency, client);
            }

            rv = RUN_ALL_TESTS();
        } catch (const std::exception & e) {
            std::cerr << e.what() << std::endl;
        } catch (...) {
        }
    }

    return rv;
}

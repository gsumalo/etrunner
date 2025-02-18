#include "environment_dt.hpp"
#include <format>
#include <iostream>
#include <boost/filesystem/operations.hpp>
#include "convenience.hpp"

namespace bpdx = boost::property_tree::detail::rapidxml;

namespace dt {

bool EnvironmentDT::init(const boost::program_options::variables_map & opt)
{
    bool rv(true);

    try {
        const boost::filesystem::path test_spec(opt["test_spec"].as<std::string>());
        const boost::filesystem::path client(opt["client"].as<std::string>());
        m_maximum_concurrency_ = opt["maximum_concurrency"].as<uint64_t>();

        if (!opt["property"].empty()) {
            auto definitions(opt["property"].as<std::vector<std::pair<std::string, std::string>>>());
            m_definitions_.insert(definitions.begin(), definitions.end());
        }

        if (!boost::filesystem::exists(test_spec)) {
            throw std::runtime_error(std::format("'{}' does not exist", test_spec.string()));
        } else if(!boost::filesystem::is_regular_file(test_spec)) {
            throw std::runtime_error(std::format("'{}' is not a file", test_spec.string()));
        } else {
            load_test_spec_(test_spec);
        }

        if (!boost::filesystem::exists(client)) {
            throw std::runtime_error(std::format("'{}' does not exist", client.string()));
        } else if(!boost::filesystem::is_regular_file(client)) {
            throw std::runtime_error(std::format("'{}' is not a file", client.string()));
        } else {
            m_client_ = client;
        }
    } catch (const std::exception & e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        rv = false;
    }

    return rv;
}

EnvironmentDT & EnvironmentDT::instance()
{
    static EnvironmentDT singleton;

    return singleton;
}

void EnvironmentDT::load_test_spec_(const boost::filesystem::path & test_spec_path)
{
    static constexpr std::string_view TESTS_LABEL("tests");
    static constexpr std::string_view SUITE_LABEL("suite");
    static constexpr std::string_view CASE_LABEL("case");
    static constexpr std::string_view SETUP_LABEL("setup");
    static constexpr std::string_view TEARDOWN_LABEL("teardown");
    static constexpr std::string_view NAME_LABEL("name");
    static constexpr std::string_view ENABLED_LABEL("enabled");
    static constexpr std::string_view PATH_LABEL("path");
    static constexpr std::string_view YES_LABEL("yes");
    static constexpr std::string_view BASETIME_LABEL("basetime");

    auto file_content(convenience::read_file(test_spec_path));
    file_content.push_back('\0');

    try {
        auto doc(std::make_unique<bpdx::xml_document<char>>());

        doc->parse<bpdx::parse_declaration_node | bpdx::parse_no_data_nodes
                | bpdx::parse_validate_closing_tags >(file_content.data());

        auto tests_node(convenience::first_node<char>(*doc, TESTS_LABEL));

        if (tests_node == nullptr) {
            throw std::runtime_error(std::format("Missing '{}' node", TESTS_LABEL));
        }

        for (auto suite_node(convenience::first_node<char>(*tests_node, SUITE_LABEL)); suite_node != nullptr;
                suite_node = convenience::next_sibling<char>(*suite_node, SUITE_LABEL)) {
            auto suite_name_node(convenience::first_attribute<char>(*suite_node, NAME_LABEL));
            auto suite_enabled_node(convenience::first_attribute<char>(*suite_node, ENABLED_LABEL));

            if ((suite_name_node != nullptr) && (suite_enabled_node != nullptr)) {
                const std::string suite_name(suite_name_node->value());
                const std::string suite_enabled(suite_enabled_node->value());

                auto current_suite(std::make_shared<DynamicTestSuite>(suite_name, suite_enabled == YES_LABEL));
                m_test_spec_.add_suite(current_suite);

                if (auto setup_node(convenience::first_node(*suite_node, SETUP_LABEL)); setup_node != nullptr) {
                    for (auto path_node(convenience::first_node<char>(*setup_node, PATH_LABEL)); path_node != nullptr;
                            path_node = convenience::next_sibling<char>(*path_node, PATH_LABEL)) {
                        const std::string path_value(path_node->value());
                        boost::filesystem::path temptative_path(path_value);
                        boost::filesystem::path final_path(temptative_path.is_absolute() ? temptative_path
                                : test_spec_path.parent_path() / temptative_path);

                        current_suite->add_setup(final_path);
                    }
                }

                if (auto teardown_node(convenience::first_node(*suite_node, TEARDOWN_LABEL)); teardown_node != nullptr) {
                    for (auto path_node(convenience::first_node<char>(*teardown_node, PATH_LABEL)); path_node != nullptr;
                            path_node = convenience::next_sibling<char>(*path_node, PATH_LABEL)) {
                        const std::string path_value(path_node->value());
                        boost::filesystem::path temptative_path(path_value);
                        boost::filesystem::path final_path(temptative_path.is_absolute() ? temptative_path
                                : test_spec_path.parent_path() / temptative_path);

                        current_suite->add_teardown(final_path);
                    }
                }

                for (auto case_node(convenience::first_node<char>(*suite_node, CASE_LABEL)); case_node != nullptr;
                        case_node = convenience::next_sibling<char>(*case_node, CASE_LABEL)) {
                    auto case_name_node(convenience::first_attribute<char>(*case_node, NAME_LABEL));
                    auto case_enabled_node(convenience::first_attribute<char>(*case_node, ENABLED_LABEL));
                    auto case_path_node(convenience::first_node<char>(*case_node, PATH_LABEL));
                    auto basetime_node(convenience::first_attribute<char>(*case_node, BASETIME_LABEL));

                    if ((case_name_node != nullptr) && (case_path_node != nullptr) && (case_enabled_node != nullptr)
                            && (basetime_node != nullptr)) {
                        const std::string case_name(case_name_node->value());
                        const std::string case_enabled(case_enabled_node->value());

                        auto current_test(std::make_shared<DynamicTestCase>(current_suite, case_name,
                                case_enabled == YES_LABEL));
                        m_test_spec_.add_case(current_test);

                        for (; case_path_node != nullptr;
                                case_path_node = convenience::next_sibling<char>(*case_path_node, PATH_LABEL)) {
                            const std::string path_value(case_path_node->value());
                            boost::filesystem::path temptative_path(path_value);
                            boost::filesystem::path final_path(temptative_path.is_absolute() ? temptative_path
                                    : test_spec_path.parent_path() / temptative_path);

                            current_test->add_step_to_plan(final_path);
                        }

                        if (auto setup_node(convenience::first_node(*case_node, SETUP_LABEL)); setup_node != nullptr) {
                            for (auto path_node(convenience::first_node<char>(*setup_node, PATH_LABEL)); path_node != nullptr;
                                    path_node = convenience::next_sibling<char>(*path_node, PATH_LABEL)) {
                                const std::string path_value(path_node->value());
                                boost::filesystem::path temptative_path(path_value);
                                boost::filesystem::path final_path(temptative_path.is_absolute() ? temptative_path
                                        : test_spec_path.parent_path() / temptative_path);

                                current_test->add_setup(final_path);
                            }
                        }

                        if (auto teardown_node(convenience::first_node(*case_node, TEARDOWN_LABEL)); teardown_node != nullptr) {
                            for (auto path_node(convenience::first_node<char>(*teardown_node, PATH_LABEL)); path_node != nullptr;
                                    path_node = convenience::next_sibling<char>(*path_node, PATH_LABEL)) {
                                const std::string path_value(path_node->value());
                                boost::filesystem::path temptative_path(path_value);
                                boost::filesystem::path final_path(temptative_path.is_absolute() ? temptative_path
                                        : test_spec_path.parent_path() / temptative_path);

                                current_test->add_teardown(final_path);
                            }
                        }
                    }
                }
            }
        }
    } catch (const std::exception & e) {
        throw e;
    } catch (...) {
        throw std::runtime_error("Unexpected error parsing test specification");
    }
}

}   // namespace dt

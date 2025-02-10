#ifndef DEPLOYMENT_TESTS_DYNAMIC_TEST_HPP_
#define DEPLOYMENT_TESTS_DYNAMIC_TEST_HPP_

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <boost/filesystem/path.hpp>
#include "test_body.hpp"

namespace dt {

class DynamicTestSuite
{
public:
    DynamicTestSuite(
            std::string_view name,
            bool enabled)
        : m_name_(name)
        , m_enabled_(enabled)
    {
    }

    void add_setup(
            const boost::filesystem::path & setup_file)
    {
        m_setup_files_.push_back(setup_file);
    }

    void add_teardown(
            const boost::filesystem::path & teardown_file)
    {
        m_teardown_files_.push_back(teardown_file);
    }

    std::string_view get_name() const
    {
        return m_name_;
    }

    const auto & get_properties() const
    {
        return m_properties_;
    }

    const auto & get_setup() const
    {
        return m_setup_files_;
    }

    const auto & get_teardown() const
    {
        return m_teardown_files_;
    }

    bool is_enabled() const
    {
        return m_enabled_;
    }

    void set_properties(
            const placeholders_t & properties)
    {
        m_properties_ = std::make_shared<placeholders_t>(properties);
    }

private:
    std::string m_name_;
    bool m_enabled_;
    plan_t m_setup_files_;
    plan_t m_teardown_files_;
    std::shared_ptr<placeholders_t> m_properties_;
};

class DynamicTestCase
{
public:
    DynamicTestCase(
            std::shared_ptr<DynamicTestSuite> suite,
            std::string_view name,
            bool enabled)
        : m_suite_(suite)
        , m_name_(name)
        , m_enabled_(enabled)
    {
    }

    void add_setup(
            const boost::filesystem::path & setup_file)
    {
        m_setup_files_.push_back(setup_file);
    }

    void add_step_to_plan(
            const boost::filesystem::path & step_file)
    {
        m_plan_file_.emplace_back(step_file);
    }

    void add_teardown(
            const boost::filesystem::path & teardown_file)
    {
        m_teardown_files_.push_back(teardown_file);
    }

    std::string_view get_name() const
    {
        return m_name_;
    }

    const auto & get_plan() const
    {
        return m_plan_file_;
    }

    const auto & get_setup() const
    {
        return m_setup_files_;
    }

    const auto & get_suite() const
    {
        return *m_suite_;
    }

    const auto & get_teardown() const
    {
        return m_teardown_files_;
    }

    bool is_enabled() const
    {
        return m_enabled_;
    }

private:
    std::shared_ptr<DynamicTestSuite> m_suite_;
    std::string m_name_;
    bool m_enabled_;
    plan_t m_plan_file_;
    plan_t m_setup_files_;
    plan_t m_teardown_files_;
};

class DynamicSpec
{
public:
    void add_case(
            std::shared_ptr<DynamicTestCase> item)
    {
        m_cases_.emplace_back(std::move(item));
    }

    void add_suite(
            std::shared_ptr<DynamicTestSuite> item)
    {
        m_suites_.emplace_back(std::move(item));
    }

    const auto & get_cases() const
    {
        return m_cases_;
    }

    const auto & get_suites() const
    {
        return m_suites_;
    }

private:
    std::vector<std::shared_ptr<DynamicTestSuite>> m_suites_;
    std::vector<std::shared_ptr<DynamicTestCase>> m_cases_;
};

void register_test(
        std::shared_ptr<DynamicTestCase> spec,
        uint64_t maximum_concurrency,
        const boost::filesystem::path & executable);

}   // namespace dt

#endif // DEPLOYMENT_TESTS_DYNAMIC_TEST_HPP_

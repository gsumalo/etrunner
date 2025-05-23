#include "dynamic_test.hpp"
#include <gtest/gtest.h>

namespace dt {

template <typename Factory>
::testing::TestInfo* register_gtest(
        const char* test_suite_name,
        const char* test_name,
        const char* file,
        int line,
        Factory factory,
        ::testing::internal::SetUpTestSuiteFunc setup,
        ::testing::internal::TearDownTestSuiteFunc teardown)
{
    using TestT = typename std::remove_pointer<decltype(factory())>::type;

    class FactoryImpl: public ::testing::internal::TestFactoryBase
    {
    public:
        explicit FactoryImpl(Factory f)
            : factory_(std::move(f))
        {
        }

        ::testing::Test* CreateTest() override 
        {
            return factory_();
        }

    private:
        Factory factory_;
    };

    return ::testing::internal::MakeAndRegisterTestInfo(test_suite_name, test_name, nullptr, nullptr,
            ::testing::internal::CodeLocation(file, line), ::testing::internal::GetTypeId<TestT>(), setup, teardown,
            new FactoryImpl{std::move(factory)});
}

class CaseWrapper: public ::testing::Test
{
public:
    explicit CaseWrapper(
            std::function<void()> body,
            std::function<void()> setup,
            std::function<void()> teardown)
        : m_body_(body)
        , m_setup_(setup)
        , m_teardown_(teardown)
    {
    }

    void TestBody() override
    {
        if (m_body_) {
            ASSERT_NO_FATAL_FAILURE(m_body_());
        } else {
            GTEST_FAIL() << "Test body could not be evaluated";
        }
    }

protected:
    void SetUp() override
    {
        if (m_setup_) {
            ASSERT_NO_FATAL_FAILURE(m_setup_());
        }
    }

    void TearDown() override
    {
        if (m_teardown_) {
            ASSERT_NO_FATAL_FAILURE(m_teardown_());
        }
    }

private:
    std::function<void()> m_body_;
    std::function<void()> m_setup_;
    std::function<void()> m_teardown_;
};

void register_test(
        std::shared_ptr<DynamicTestCase> spec,
        uint64_t maximum_concurrency,
        const boost::filesystem::path & executable)
{
    static constexpr std::string_view DISABLED_PREFIX("DISABLED_");

    const auto & suite(spec->get_suite());
    auto properties(suite.get_properties());
    std::string suite_name(suite.is_enabled() ? "" : DISABLED_PREFIX);
    suite_name.append(suite.get_name());
    std::string case_name(spec->is_enabled() ? "" : DISABLED_PREFIX);
    case_name.append(spec->get_name());

    auto case_body([=]() -> ::testing::Test* {
            auto case_properties(std::make_shared<placeholders_t>(*properties));

            return new CaseWrapper(
                    std::bind(test_body, spec->get_plan(), maximum_concurrency, executable, case_properties),
                    std::bind(setup_body, spec->get_setup(), maximum_concurrency, executable, case_properties),
                    std::bind(teardown_body, spec->get_teardown(), maximum_concurrency, executable, case_properties));
        });
    ::testing::internal::SetUpTestSuiteFunc setup([=]() {
            ASSERT_NO_FATAL_FAILURE(setup_body(spec->get_suite().get_setup(), maximum_concurrency, executable,
                    properties));
        });
    ::testing::internal::TearDownTestSuiteFunc teardown([=]() {
            EXPECT_NO_FATAL_FAILURE(teardown_body(spec->get_suite().get_teardown(), maximum_concurrency, executable,
                    properties));
        });

    register_gtest(suite_name.c_str(), case_name.c_str(), __FILE__, __LINE__, case_body, setup, teardown);
}

}   // namespace dt

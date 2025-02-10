#ifndef ENVIRONMENT_DT
#define ENVIRONMENT_DT

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <boost/program_options.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include "dynamic_test.hpp"

namespace dt {

class EnvironmentDT
{
public:
    EnvironmentDT(const EnvironmentDT &) = delete;
    EnvironmentDT(EnvironmentDT &&) = default;
    ~EnvironmentDT() = default;

    EnvironmentDT & operator=(const EnvironmentDT &) = delete;
    EnvironmentDT & operator=(EnvironmentDT &&) = default;

    bool init(const boost::program_options::variables_map & opt);

    static EnvironmentDT & instance();

    const auto & test_spec() const
    {
        return m_test_spec_;
    }

    const auto & client() const
    {
        return m_client_;
    }

    const auto & properties() const
    {
        return m_definitions_;
    }

    const auto & maximum_concurrency() const
    {
        return m_maximum_concurrency_;
    }

private:
    EnvironmentDT()
        : m_maximum_concurrency_(0)
    {
    }

    void load_test_spec_(const boost::filesystem::path & test_spec_path);

    DynamicSpec m_test_spec_;
    boost::filesystem::path m_client_;
    uint64_t m_maximum_concurrency_;
    std::map<std::string,std::string> m_definitions_;
};

} // namespace dt

#endif //ENVIRONMENT_DT

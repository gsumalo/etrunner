#ifndef DEPLOYMENT_TESTS_TEST_BODY_HPP_
#define DEPLOYMENT_TESTS_TEST_BODY_HPP_

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <boost/filesystem/path.hpp>

namespace dt {

typedef std::map<std::string,std::string> placeholders_t;
typedef std::vector<boost::filesystem::path> plan_t;

void setup_body(
        const plan_t & plan,
        uint64_t maximum_concurrency,
        const boost::filesystem::path & executable, 
        std::shared_ptr<placeholders_t> properties);

void teardown_body(
        const plan_t & plan,
        uint64_t maximum_concurrency,
        const boost::filesystem::path & executable, 
        std::shared_ptr<placeholders_t> properties);

void test_body(
        const plan_t & plan,
        uint64_t maximum_concurrency,
        const boost::filesystem::path & executable, 
        std::shared_ptr<placeholders_t> properties);

}   // namespace dt;

#endif // DEPLOYMENT_TESTS_TEST_BODY_HPP_

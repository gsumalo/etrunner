#ifndef DEPLOYMENT_TESTS_CONVENIENCE_HPP_
#define DEPLOYMENT_TESTS_CONVENIENCE_HPP_

#include <string>
#include <string_view>
#include <vector>
#include <boost/filesystem/path.hpp>
#include <boost/property_tree/detail/rapidxml.hpp>

namespace convenience {

std::string read_file(
        const boost::filesystem::path & path);

template <typename Ch>
boost::property_tree::detail::rapidxml::xml_attribute<Ch> *first_attribute(
        const boost::property_tree::detail::rapidxml::xml_node<Ch> & node,
        std::basic_string_view<Ch> name,
        bool case_sensitive = true)
{
    return node.first_attribute(name.data(), name.size(), case_sensitive);
}

template <typename Ch>
boost::property_tree::detail::rapidxml::xml_node<Ch> *first_node(
        const boost::property_tree::detail::rapidxml::xml_document<Ch> & doc,
        std::basic_string_view<Ch> name,
        bool case_sensitive = true)
{
    return doc.first_node(name.data(), name.size(), case_sensitive);
}

template <typename Ch>
boost::property_tree::detail::rapidxml::xml_node<Ch> *first_node(
        const boost::property_tree::detail::rapidxml::xml_node<Ch> & node,
        std::basic_string_view<Ch> name,
        bool case_sensitive = true)
{
    return node.first_node(name.data(), name.size(), case_sensitive);
}

template <typename Ch>
boost::property_tree::detail::rapidxml::xml_node<Ch> *next_sibling(
        const boost::property_tree::detail::rapidxml::xml_node<Ch> & node,
        std::basic_string_view<Ch> name,
        bool case_sensitive = true)
{
    return node.next_sibling(name.data(), name.size(), case_sensitive);
}

int run_process(
        const boost::filesystem::path & executable,
        std::vector<std::string> args,
        std::string_view std_in,
        std::string & std_out,
        std::string & std_err);

}   // namespace convenience

#endif // DEPLOYMENT_TESTS_CONVENIENCE_HPP_

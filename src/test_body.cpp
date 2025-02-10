#include "test_body.hpp"
#include <format>
#include <future>
#include <vector>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/graphml.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <gtest/gtest.h>
#include <pugixml.hpp>
#include <tbb/flow_graph.h>
#include "convenience.hpp"

namespace dt {

struct GraphData
{
    std::string label;
    std::string args;
    std::string extra_args;
};

typedef boost::adjacency_list<boost::setS, boost::vecS, boost::bidirectionalS, GraphData> TestGraph;

std::string apply_placeholders(
        std::string_view message,
        const placeholders_t & placeholders)
{
    std::string rv(message);

    for (const auto & it: placeholders) {
        boost::algorithm::replace_all(rv, it.first, it.second);
    }

    return rv;
}


struct TestNode
{
public:
    boost::filesystem::path m_request_file;
    boost::filesystem::path m_expected_response_file;
    std::vector<std::string> m_args;
    placeholders_t m_placeholders;

    TestNode() = default;
    explicit TestNode(
            const std::vector<std::string> & args)
        : m_args(args)
    {
    }

    placeholders_t get_placeholder_values(
            std::string_view response) const;
    std::vector<pugi::xpath_query> get_suppresion_list() const;
    bool is_empty_request() const;
    std::string run(
            const boost::filesystem::path & executable,
            std::string_view request) const;
    void set_files(
            const boost::filesystem::path & request_file,
            const boost::filesystem::path & expected_response_file)
    {
        m_request_file = request_file;
        m_expected_response_file = expected_response_file;
    }
};

class TestCase: public testing::Test
{
public:
    TestCase(
            const plan_t & plan,
            const boost::filesystem::path & executable);

    void TestBody() override;

    void add_as_placeholders(const placeholders_t & properties);
    std::string get_as_placeholder(const std::string & key) const;
    placeholders_t get_new_properties() const;
    placeholders_t get_placeholders() const;
    void set_concurrency(uint64_t maximum_concurrency);

private:
    static void parse_test_graph_(
            const boost::filesystem::path & graph_file,
            const placeholders_t & placeholders,
            TestGraph & rv);
    void run_(const TestNode & test);

    const plan_t m_plan_;
    const boost::filesystem::path m_executable_;
    mutable std::mutex m_placeholders_guard_;
    placeholders_t m_placeholders_;
    int m_concurrency_;
    std::atomic<bool> m_no_fatal_error_;
    placeholders_t m_new_properties_;
};

TestCase::TestCase(
        const plan_t & plan,
        const boost::filesystem::path & executable)
    : m_plan_(plan)
    , m_executable_(executable)
    , m_concurrency_(tbb::task_arena::automatic)
    , m_no_fatal_error_(true)
{
}

void TestCase::add_as_placeholders(
        const placeholders_t & properties)
{
    std::lock_guard safe(m_placeholders_guard_);

    for (const auto & pair: properties) {
        m_placeholders_[get_as_placeholder(pair.first)] = pair.second;
    }
}

std::string TestCase::get_as_placeholder(const std::string & key) const
{
    auto rv("${" + key + "}");
    return rv;
}

placeholders_t TestCase::get_new_properties() const
{
    return m_new_properties_;
}

placeholders_t TestCase::get_placeholders() const
{
    std::lock_guard safe(m_placeholders_guard_);

    auto rv(m_placeholders_);
    return rv;
}

void TestCase::parse_test_graph_(
        const boost::filesystem::path & graph_file,
        const placeholders_t & placeholders,
        TestGraph & rv)
{
    ASSERT_TRUE(!graph_file.empty() && boost::filesystem::exists(graph_file)
            && boost::filesystem::is_regular_file(graph_file)) << std::format(" with file '{}'", graph_file.string());

    const auto graph_plan(apply_placeholders(convenience::read_file(graph_file), placeholders));
    ASSERT_FALSE(graph_plan.empty());

    boost::dynamic_properties graph_properties;
    graph_properties.property("label", boost::get(&GraphData::label, rv));
    graph_properties.property("args", boost::get(&GraphData::args, rv));
    graph_properties.property("extra_args", boost::get(&GraphData::extra_args, rv));

    std::istringstream graph_accessor(graph_plan);
    ASSERT_NO_THROW(boost::read_graphml(graph_accessor, rv, graph_properties));
}

void TestCase::TestBody()
{
    for (const auto & step_file: m_plan_) {
        TestGraph step_graph;
        parse_test_graph_(step_file, get_placeholders(), step_graph);

        // Get execution order
        std::vector<decltype(step_graph)::vertex_descriptor> nodes;
        boost::topological_sort(step_graph, std::back_inserter(nodes));
        std::reverse(nodes.begin(), nodes.end());

        // Insertion of a fictitious common origin node ancestor of all the real nodes
        tbb::flow::graph executor;
        typedef tbb::flow::continue_node<tbb::flow::continue_msg> task_node_t;
        task_node_t origin(executor, [](const tbb::flow::continue_msg &) { });
        std::map<decltype(nodes)::value_type, std::shared_ptr<task_node_t>> task_nodes;

        const boost::filesystem::path plan_dir(step_file.parent_path() / std::string_view(step_file.stem().string()));
        const auto requests_dir(plan_dir / std::string_view("requests"));
        const auto responses_dir(plan_dir / std::string_view("responses"));

        for (const auto & node: nodes) {
            std::string_view node_name(step_graph[node].label);
            const auto & args(step_graph[node].args);
            const auto & extra_args(step_graph[node].extra_args);

            std::vector<std::string> final_args;
            boost::char_separator<char> delimiter(",");
            boost::tokenizer<boost::char_separator<char>> tok(args, delimiter);
            for (auto it(tok.begin()); it != tok.end(); ++it) {
                final_args.emplace_back(*it);
            }
            tok = boost::tokenizer<boost::char_separator<char>>(extra_args, delimiter);
            for (auto it(tok.begin()); it != tok.end(); ++it) {
                final_args.emplace_back(*it);
            }

            auto test_node(std::make_shared<TestNode>(final_args));

            if (!node_name.empty()) {
                const auto request_file(requests_dir / node_name);
                ASSERT_TRUE(boost::filesystem::exists(request_file)) << " with file " << request_file.string();

                const auto response_file(responses_dir / node_name);
                ASSERT_TRUE(boost::filesystem::exists(response_file)) << " with file " << response_file.string();

                test_node->set_files(request_file, response_file);
            }

            // Addition of nodes and edges to the task graph
            auto task_node(std::make_shared<task_node_t>(executor, [&, test_node](const tbb::flow::continue_msg &) {
                    if (m_no_fatal_error_) {
                        try {
                            test_node->m_placeholders = get_placeholders();
                            run_(*test_node);

                            if (HasFatalFailure()) {
                                m_no_fatal_error_ = false;
                            }
                        } catch (...) {
                            m_no_fatal_error_ = false;
                            FAIL();
                        }
                    }
                }));

            auto dependencies(boost::in_edges(node, step_graph));

            if (dependencies.first == dependencies.second) {    // No dependencies -> real origin node
                tbb::flow::make_edge(origin, *task_node);
            } else {
                for (auto it(dependencies.first); it != dependencies.second; ++it) {
                    auto ancestor(task_nodes.find(boost::source(*it, step_graph)));
                    tbb::flow::make_edge(*(ancestor->second), *task_node);
                }
            }

            task_nodes[node] = task_node;
        }

        // Execute the tests
        tbb::task_arena arena(boost::numeric_cast<int>(m_concurrency_));
        arena.execute([&]() { executor.reset(); });
        origin.try_put(tbb::flow::continue_msg());
        executor.wait_for_all();
    }
}

void TestCase::run_(const TestNode & test)
{
    if (test.is_empty_request()) {
        std::string bulk_response;
        ASSERT_NO_THROW(bulk_response = test.run(m_executable_, ""))
                << std::format(" with executable '{}' and empty request\n", m_executable_.string());
    } else {
        const auto request(apply_placeholders(convenience::read_file(test.m_request_file), test.m_placeholders));
        ASSERT_FALSE(request.empty());
        const auto expected_response(apply_placeholders(convenience::read_file(test.m_expected_response_file),
                test.m_placeholders));
        ASSERT_FALSE(expected_response.empty()) << std::format(" with request '{}'\n", request);

        std::string bulk_response;
        ASSERT_NO_THROW(bulk_response = test.run(m_executable_, request))
                << std::format(" with executable '{}' and request '{}'\n", m_executable_.string(), request);

        const auto response(apply_placeholders(bulk_response, test.m_placeholders));
        ASSERT_FALSE(response.empty()) << std::format(" with request file '{}'\n", test.m_request_file.string());

        std::vector<pugi::xml_document> response_docs(2);
        ASSERT_TRUE(response_docs[0].load_buffer(expected_response.data(), expected_response.size()));
        ASSERT_TRUE(response_docs[1].load_buffer(response.data(), response.size()));

        for (const auto & suppression: test.get_suppresion_list()) {
            for (auto & doc: response_docs) {
                auto nodes(doc.select_nodes(suppression));

                for (const auto & node: nodes) {
                    node.parent().remove_child(node.node());
                }
            }
        }

        std::vector<std::string> final_responses;
        final_responses.reserve(response_docs.size());
        for (const auto & doc: response_docs) {
            std::ostringstream flattened_doc;
            doc.save(flattened_doc, " ");
            final_responses.push_back(flattened_doc.str());
        }

        const auto & expected(final_responses[0]);
        const auto & result(final_responses[1]);
        ASSERT_EQ(expected, result) << std::format(" with request file '{}'\n", test.m_request_file.string());

        if (auto new_properties(test.get_placeholder_values(response)); !new_properties.empty()) {
            add_as_placeholders(new_properties);

            for (const auto & new_property: new_properties) {
                m_new_properties_[new_property.first] = new_property.second;
            }
        }
    }
}

void TestCase::set_concurrency(uint64_t maximum_concurrency)
{
    if (maximum_concurrency == 0) {
        m_concurrency_ = tbb::task_arena::automatic;
    } else {
        m_concurrency_ = boost::numeric_cast<decltype(m_concurrency_)>(maximum_concurrency);
    }
}

placeholders_t TestNode::get_placeholder_values(std::string_view response) const
{
    placeholders_t rv;

    boost::filesystem::path control_file(m_request_file);
    control_file.replace_extension(".ctl");

    if (boost::filesystem::exists(control_file)) {
        auto control_contents(convenience::read_file(control_file));

        pugi::xml_document control_doc;
        if (!control_doc.load_buffer(control_contents.data(), control_contents.size())) {
            throw std::runtime_error("Invalid XML found at " + control_file.string());
        }

        placeholders_t placeholder_mapper;

        auto nodes(control_doc.select_nodes("/control/placeholder"));
        for (const auto & node: nodes) {
            std::string name(node.node().child_value("name"));
            std::string metavalue(node.node().child_value("metavalue"));

            if (!name.empty() && !metavalue.empty()) {
                placeholder_mapper[name] = metavalue;
            }
        }

        pugi::xml_document response_doc;
        if (!response_doc.load_buffer(response.data(), response.size())) {
            throw std::runtime_error("Invalid XML found at response");
        }

        for (const auto & mapping: placeholder_mapper) {
            const auto node(response_doc.select_node(mapping.second.c_str()));

            if (node != nullptr) {
                rv[mapping.first] = node.node().child_value();
            } else {
                throw std::runtime_error("Missing node from control specification");
            }
        }
    }

    return rv;
}

std::vector<pugi::xpath_query> TestNode::get_suppresion_list() const
{
    std::vector<pugi::xpath_query> rv;

    boost::filesystem::path ignore_file(m_request_file);
    ignore_file.replace_extension(".ign");

    if (boost::filesystem::exists(ignore_file)) {
        auto ignore_contents(convenience::read_file(ignore_file));

        boost::char_separator<char> eol("\n");
        boost::tokenizer<boost::char_separator<char>> tok(ignore_contents, eol);
        for (auto it(tok.begin()); it != tok.end(); ++it) {
            std::string line(*it);
            rv.emplace_back(line.c_str());
        }
    }

    return rv;
}

bool TestNode::is_empty_request() const
{
    bool rv(m_request_file.empty());
    return rv;
}

std::string TestNode::run(
        const boost::filesystem::path & executable,
        std::string_view request) const
{
    std::string response, error_text;

    EXPECT_EQ(EXIT_SUCCESS, convenience::run_process(executable, m_args, request, response, error_text)) << error_text
            << (response.empty() ? "\n" : "\nwith response:\n" + response + "\n");

    return response;
}

void setup_body(
        const plan_t & plan,
        uint64_t maximum_concurrency,
        const boost::filesystem::path & executable,
        std::shared_ptr<placeholders_t> properties)
{
    TestCase test_case(plan, executable);
    test_case.add_as_placeholders(*properties);
    test_case.set_concurrency(maximum_concurrency);
    test_case.TestBody();

    auto & old_properties(*properties);
    const auto new_properties(test_case.get_new_properties());

    for (const auto & property: new_properties) {
        old_properties[property.first] = property.second;
    }
}

void test_body(
        const plan_t & plan,
        uint64_t maximum_concurrency,
        const boost::filesystem::path & executable,
        std::shared_ptr<placeholders_t> properties)
{
    TestCase test_case(plan, executable);
    test_case.add_as_placeholders(*properties);
    test_case.set_concurrency(maximum_concurrency);
    test_case.TestBody();
}

}   // namespace dt

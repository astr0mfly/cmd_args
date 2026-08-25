#include "catch2/catch.hpp"

#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"
#include "cmd_args/option/option_graph.h"

using namespace cmd_args;

/* ==================== OptionGraph：有向图 ==================== */

TEST_CASE("graph: addEdge 后节点自动创建", "[graph]")
{
    OptionGraph graph;
    graph.addEdge("a", "b", OptionGraph::EdgeKind_E::Conditional, "c");

    REQUIRE(graph.nodes().count("a") == 1);
    REQUIRE(graph.nodes().count("b") == 1);
    REQUIRE(graph.nodes().count("c") == 1);
    REQUIRE_FALSE(graph.empty());
}

TEST_CASE("graph: 拓扑排序——无环图返回有效序", "[graph]")
{
    OptionGraph graph;
    graph.addEdge("a", "b", OptionGraph::EdgeKind_E::Dependency);
    graph.addEdge("b", "c", OptionGraph::EdgeKind_E::Dependency);

    auto order = graph.topologicalSort();
    REQUIRE(order.size() == 3);
    // a 在 b 之前，b 在 c 之前
    auto posA = std::find(order.begin(), order.end(), "a") - order.begin();
    auto posB = std::find(order.begin(), order.end(), "b") - order.begin();
    auto posC = std::find(order.begin(), order.end(), "c") - order.begin();
    REQUIRE(posA < posB);
    REQUIRE(posB < posC);
}

TEST_CASE("graph: 条件求值——条件为真激活 then 分支", "[graph]")
{
    OptionGraph graph;
    graph.addEdge("verbose", "debug", OptionGraph::EdgeKind_E::Conditional, "quiet");

    graph.setNodeState("verbose", true);
    auto activated = graph.evaluate();

    REQUIRE(activated.size() == 1);
    REQUIRE(activated[0] == "debug");
    REQUIRE(graph.isNodeEnabled("debug"));
    REQUIRE_FALSE(graph.isNodeEnabled("quiet"));
}

TEST_CASE("graph: 条件求值——条件为假激活 else 分支", "[graph]")
{
    OptionGraph graph;
    graph.addEdge("verbose", "debug", OptionGraph::EdgeKind_E::Conditional, "quiet");

    graph.setNodeState("verbose", false);
    auto activated = graph.evaluate();

    REQUIRE(activated.size() == 1);
    REQUIRE(activated[0] == "quiet");
    REQUIRE(graph.isNodeEnabled("quiet"));
    REQUIRE_FALSE(graph.isNodeEnabled("debug"));
}

TEST_CASE("graph: 条件求值——else 为 _ 时不激活任何选项", "[graph]")
{
    OptionGraph graph;
    graph.addEdge("verbose", "debug", OptionGraph::EdgeKind_E::Conditional, "_");

    graph.setNodeState("verbose", false);
    auto activated = graph.evaluate();

    REQUIRE(activated.empty());
    REQUIRE_FALSE(graph.isNodeEnabled("debug"));
}

TEST_CASE("graph: 依赖边不参与条件求值（仅影响拓扑序）", "[graph]")
{
    OptionGraph graph;
    graph.addEdge("compile", "link", OptionGraph::EdgeKind_E::Dependency);

    graph.setNodeState("compile", true);
    auto activated = graph.evaluate();

    // Dependency 边不在 evaluate 中激活目标——目标由上层（Executor）按拓扑序显式触发
    REQUIRE(activated.empty());
}

TEST_CASE("graph: clear 后图为空", "[graph]")
{
    OptionGraph graph;
    graph.addEdge("a", "b", OptionGraph::EdgeKind_E::Conditional, "c");
    REQUIRE_FALSE(graph.empty());

    graph.clear();
    REQUIRE(graph.empty());
    REQUIRE(graph.nodes().empty());
    REQUIRE(graph.edges().empty());
}

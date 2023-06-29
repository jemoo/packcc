#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <iostream>
#include <unordered_map>

template <typename Annotation> struct AstBase : public Annotation {
    AstBase(size_t line, size_t column, const char* name, const char* view, int token,
        size_t position = 0, size_t length = 0)
        : line(line), column(column), name(name),
        position(position), length(length), original_name(name),
        view(view), token(token) {}

    AstBase(const AstBase& ast, const char* original_name, size_t position = 0, size_t length = 0)
        : line(ast.line), column(ast.column), name(ast.name),
        position(position), length(length), original_name(original_name),
        view(ast.view), token(ast.token), nodes(ast.nodes), parent(ast.parent) {}

    const size_t line;
    const size_t column;
    const size_t position;
    const size_t length;
    const std::string_view name;
    const char* view;
    const int token;
    const std::string_view original_name;
    std::vector<std::shared_ptr<AstBase<Annotation>>> nodes;
    std::weak_ptr<AstBase<Annotation>> parent;

    std::vector<AstBase<Annotation>*> cached_nodes;
    std::unordered_map< std::string_view, std::vector<AstBase<Annotation>*> > cached_map;
};

inline std::string as_string(const std::string_view& v) {
    return { v.data(), v.size() };
}

template <typename T>
void ast_to_s_core(const std::shared_ptr<T>& ptr, std::string& s, int level,
    std::function<std::string(const T& ast, int level)> fn) {
    const auto& ast = *ptr;
    for (auto i = 0; i < level; i++) {
        s += "  ";
    }
    auto name = as_string(ast.name);
    if (ast.name != ast.original_name) {
        name += ": " + as_string(ast.original_name);
    }
    if (ast.token) {
        s += "- " + name + " (" + std::string(ast.view, ast.length) + ")\n";
    }
    else {
        s += "+ " + name + "\n";
    }
    if (fn) { s += fn(ast, level + 1); }
    for (auto node : ast.nodes) {
        ast_to_s_core(node, s, level + 1, fn);
    }
}

template <typename T>
std::string
ast_to_s(const std::shared_ptr<T>& ptr,
    std::function<std::string(const T& ast, int level)> fn = nullptr) {
    std::string s;
    ast_to_s_core(ptr, s, 0, fn);
    return s;
}

struct AstOptimizer {
    AstOptimizer(bool mode, const std::vector<std::string>& rules = {})
        : mode_(mode), rules_(rules) {}

    template <typename T>
    std::shared_ptr<T> optimize(std::shared_ptr<T> original,
        std::shared_ptr<T> parent = nullptr) {
        auto found =
            std::find(rules_.begin(), rules_.end(), original->name) != rules_.end();
        auto opt = mode_ ? !found : found;

        if (opt && original->nodes.size() == 1) {
            auto child = optimize(original->nodes[0], parent);
            auto ast = std::make_shared<T>(*child, original->name.data(),
                original->position, original->length);
            for (auto node : ast->nodes) {
                node->parent = ast;
            }
            return ast;
        }

        auto ast = std::make_shared<T>(*original);
        ast->parent = parent;
        ast->nodes.clear();
        for (auto node : original->nodes) {
            auto child = optimize(node, ast);
            ast->nodes.push_back(child);
        }
        return ast;
    }

private:
    const bool mode_;
    const std::vector<std::string> rules_;
};

struct EmptyType {};
using AstNode = AstBase<EmptyType>;

typedef std::shared_ptr<AstNode> AstPtr;

void ast_flush(AstPtr node) {
    if (!node->nodes.empty()) {
        // update length
        auto last_node = node->nodes.back();
        size_t last_node_end = last_node->position + last_node->length;
        const_cast<size_t&>(node->length) = last_node_end - node->position;
        // make node cache
        for (auto sub : node->nodes) {
            node->cached_nodes.push_back(sub.get());
            auto it = node->cached_map.find(sub->name);
            if (it == node->cached_map.end()) {
                node->cached_map.insert({ sub->name, {sub.get()} });
            }
            else {
                it->second.push_back(sub.get());
            }
            ast_flush(sub);
        }
    }
}

extern "C" const char** vsharp_fragments();

std::vector<std::string> get_ast_opt_rules() {
    const char** fragments = vsharp_fragments();
    std::vector<std::string> rules;
    for (int i = 0;; i++) {
        const char* name = fragments[i];
        if (!name) {
            break;
        }
        rules.push_back(name);
    }
    return rules;
}

#include "vsparser.h"
#include "vsharp.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <iostream>

#include "ast.h"

using namespace std;

bool IsEmpty(const vsp_pos_t& pos) {
    return pos.start >= pos.end;
}

vsp_pos_t operator | (const vsp_pos_t& a, const vsp_pos_t& b) {
    if (IsEmpty(a)) {
        return b;
    }
    if (IsEmpty(b)) {
        return a;
    }
    return { a.start, b.end, a.line, a.col };
}

struct VspType {
    vsp_pos_t pos;
    vsp_pos_t mod_pos;          // `module`.type
    vsp_pos_t name_pos;         // module.`type`
    int type_pointers;          // type`*`
    vsp_pos_t reference_pos;    // type`&`
    vsp_pos_t nullable_pos;     // type`?`
    int is_tuple;
    int is_generic;
    int is_function;
    int return_type;
    vector<int> type_params;    // is_tuple: `(int, bool)`
                                // is_generic: module.type`<t>`
                                // is_function: function `(int, bool)`
};

struct VspFunc {
    vsp_pos_t pos;
    vsp_pos_t const_pos;
    vsp_pos_t func_pos;
    vsp_pos_t name_pos;
    vector<vsp_pos_t> generic_names;
    vector<vsp_param_t> params;
    int type;
};

struct VsParser {
    vsparser_t base;

    const char* src;
    size_t len;
    size_t rp;
    vector<size_t> lines;

    std::vector<AstPtr> ast_nodes;
    AstPtr root_node;
    AstPtr cur_node;

    int indent;

    struct {
        vsp_pos_t import_pos;
        vsp_pos_t mod_pos;
        vsp_pos_t as_pos;
        vsp_pos_t alias_pos;
        string mod_name;
        vector<vsp_pos_t> syms;
    } import_decl;

    struct {
        vsp_pos_t kind_pos;
        vsp_pos_t name_pos;
        vector<vsp_pos_t> generic_names;
        vector<int> supers;
        vector<vsp_field_t> fields;
        vector<int> methods;
    } struct_decl;

    vector<vsp_pos_t> generic_names;        // class Map`<TKey, TValue>`
    vector<vsp_param_t> param_list;       // function Foo`(A: int, B: int)`

    vector<int> type_stack;
    vector<VspType> type_specs;
    int type_spec;

    vsp_pos_t field_kind_pos;

    vector<VspFunc> func_specs;
    int func_spec;

    bool IsInsideStruct() const {
        return !IsEmpty(struct_decl.kind_pos);
    }

    vector<vsp_pos_t> TakeGenericNames() {
        auto tmp = generic_names;
        generic_names = {};
        return tmp;
    }

    vector<vsp_param_t> TakeFunctionParameters() {
        auto tmp = param_list;
        param_list = {};
        return tmp;
    }

    string str(const vsp_pos_t& pos) const {
        if (IsEmpty(pos))
            return string();
        return string(src + pos.start, pos.end - pos.start);
    }

    VspType& AddTypeSpec(const VspType& in_type_spec) {
        type_spec = (int)type_specs.size();
        type_specs.push_back(in_type_spec);
        return type_specs.back();
    }

    VspType& CurTypeSpec() {
        return type_specs[type_spec];
    }

    void PushAstNode(const char* name, int term, size_t start, size_t end);
    void PopAstNode(const char* name, bool succeeded);
};

vsp_strv_t vsp_strv(const string& str) {
    return vsp_strv_t{ str.c_str(), (int)str.size() };
}

vsp_strv_t vsp_strv(VsParser* p, const vsp_pos_t& pos) {
    return vsp_strv_t{ p->src+pos.start, pos.end-pos.start };
}

void vsp_newline(VsParser* p, size_t pos) {
    p->lines.push_back(pos);
}

void vsp_compute_line_and_column(VsParser* p, size_t pos, size_t& line, size_t& col) {
    auto n = p->lines.size();
    for (auto i = n; i >= 1; i--) {
        auto line_start = p->lines.at(i - 1);
        if (pos >= line_start) {
            line = i - 1;
            col = pos - line_start;
            return;
        }
    }
    line = 0;
    col = 0;
}

vsp_pos_t vsp_pos(vsparser_t* p, size_t start, size_t end) {
    vsp_pos_t pos;
    if (!p || start >= end) {
        pos.start = 0;
        pos.end = 0;
        pos.line = 0;
        pos.col = 0;
    }
    else {
        pos.start = (int)start;
        pos.end = (int)end;
        size_t line, col;
        vsp_compute_line_and_column((VsParser*)p, start, line, col);
        pos.line = (int)line;
        pos.col = (int)col;
    }
    return pos;
}

void vsp_clear_type_specs(VsParser* p) {
    p->type_stack.clear();
    p->type_specs.clear();
    p->type_spec = 0;
}

#define STR(_POS)       _POS.end-_POS.start, p->src+_POS.start
#define POS(_POS)       _POS.start, _POS.end
#define PSTR(_POS)      POS(_POS), STR(_POS)
#define INDENT          p->indent*2, ""

#ifdef VSP_LIB
#define VSP_LOG(...)
#else
#define VSP_LOG(...)\
    printf(__VA_ARGS__)
#endif

#define VSP_INVOKE(callback, ...)\
    if (p->base.el && p->base.el->callback) {\
        p->base.el->callback(p->base.el->udata, ##__VA_ARGS__);\
    }

void on_module_clause(VsParser* p, vsp_pos_t module_pos, vsp_pos_t name_pos) {
    VSP_LOG("%*smodule[%d, %d], name[%d, %d]:%.*s\r\n", INDENT, POS(module_pos), PSTR(name_pos));
}

#pragma region import_declaration

void enter_import_declaration(VsParser* p, vsp_pos_t import_pos, vsp_pos_t mod_pos) {
    VSP_LOG("\r\n");
    VSP_LOG("%*simport[%d, %d], module[%d, %d]:%.*s {\r\n", INDENT, POS(import_pos), PSTR(mod_pos));
    p->indent++;

    p->import_decl.import_pos = import_pos;
    p->import_decl.mod_pos = mod_pos;
}

void exit_import_declaration(VsParser* p) {
    auto& import_decl = p->import_decl;
    if (import_decl.mod_name.size()) {
        // case 2
        if (import_decl.syms.size()) {
            VSP_INVOKE(on_import_declaration, import_decl.import_pos,
                import_decl.mod_pos, vsp_strv(import_decl.mod_name),
                vsp_pos(nullptr, 0, 0), import_decl.alias_pos, 
                import_decl.syms.data(), (int)import_decl.syms.size());
        }
        // case 1
        else {
            VSP_INVOKE(on_import_declaration, import_decl.import_pos,
                import_decl.mod_pos, vsp_strv(import_decl.mod_name),
                vsp_pos(nullptr, 0, 0), import_decl.alias_pos,
                nullptr, 0);
        }
        import_decl = {};
    }

    p->indent--;
    VSP_LOG("%*s} //import\r\n", INDENT);
}

void on_import_module_name(VsParser* p, vsp_pos_t name_pos, int dot) {
    p->import_decl.alias_pos = name_pos;
    if (dot) {
        p->import_decl.mod_name += '.';
        p->import_decl.mod_name += p->str(name_pos);
    }
    else {
        p->import_decl.mod_name = p->str(name_pos);
    }
}

void on_import_alias(VsParser* p, vsp_pos_t as_pos, vsp_pos_t alias_pos) {
    VSP_LOG("%*sas[%d, %d], alias[%d, %d]:%.*s\r\n", INDENT, POS(as_pos), PSTR(alias_pos));

    auto& import_decl = p->import_decl;
    import_decl.as_pos = as_pos;
    import_decl.alias_pos = alias_pos;

    VSP_INVOKE(on_import_declaration, import_decl.import_pos, 
        import_decl.mod_pos, vsp_strv(import_decl.mod_name), 
        import_decl.as_pos, import_decl.alias_pos, 
        nullptr, 0);

    import_decl = {};
}

void on_import_symbol(VsParser* p, vsp_pos_t symbol_pos) {
    VSP_LOG("%*ssymbol[%d, %d]:%.*s\r\n", INDENT, PSTR(symbol_pos));

    auto& import_decl = p->import_decl;
    import_decl.syms.push_back(symbol_pos);
}

#pragma endregion

#pragma region struct_declaration

void enter_struct_declaration(VsParser* p, vsp_pos_t kind_pos, vsp_pos_t name_pos) {
    VSP_LOG("\r\n");
    VSP_LOG("%*sstruct[%d, %d]:%.*s, name[%d, %d]:%.*s {\r\n", INDENT, PSTR(kind_pos), PSTR(name_pos));
    p->indent++;

    auto& struct_decl = p->struct_decl;
    struct_decl.kind_pos = kind_pos;
    struct_decl.name_pos = name_pos;
}

void exit_struct_declaration(VsParser* p, vsp_pos_t pos) {
    auto& struct_decl = p->struct_decl;
    VSP_INVOKE(on_struct_declaration, 
        pos, struct_decl.kind_pos, struct_decl.name_pos,
        struct_decl.generic_names.data(), (int)struct_decl.generic_names.size(),
        struct_decl.supers.data(), (int)struct_decl.supers.size(),
        struct_decl.fields.data(), (int)struct_decl.fields.size(),
        struct_decl.methods.data(), (int)struct_decl.methods.size());
    p->struct_decl = {};
    vsp_clear_type_specs(p);

    p->indent--;
    VSP_LOG("%*s} //struct\r\n", INDENT);
}

void on_generic_name(VsParser* p, vsp_pos_t type_param_pos) {
    VSP_LOG("%*stype_param[%d, %d]:%.*s\r\n", INDENT, PSTR(type_param_pos));

    p->generic_names.push_back(type_param_pos);
}

void on_struct_generic_names(VsParser* p, vsp_pos_t generic_names_pos) {
    VSP_LOG("%*sgeneric_names[%d, %d]:%.*s\r\n", INDENT, PSTR(generic_names_pos));

    p->struct_decl.generic_names = p->TakeGenericNames();
}

void on_struct_super_type(VsParser* p, int comma) {
    if (!comma) {
        p->struct_decl.supers = {};
    }
    p->struct_decl.supers.push_back(p->type_spec);
}

#pragma endregion

#pragma region type

void on_named_type(VsParser* p, vsp_pos_t mod_pos, vsp_pos_t type_pos) {

    if (IsEmpty(mod_pos)) {
        VSP_LOG("%*stype_spec[%d, %d]:%.*s\r\n", INDENT, PSTR(type_pos));
    }
    else {
        VSP_LOG("%*stype_spec[%d, %d]:%.*s.%.*s\r\n", INDENT, POS(type_pos), STR(mod_pos), STR(type_pos));
    }

    p->AddTypeSpec({ mod_pos | type_pos, mod_pos, type_pos });
}

void on_type_pointer(VsParser* p, vsp_pos_t pos, vsp_pos_t sym_pos) {
    if (p->type_spec >= 0) {
        auto& type_sepc = p->CurTypeSpec();
        type_sepc.type_pointers++;
        type_sepc.pos = pos;
    }
}

void on_type_reference(VsParser* p, vsp_pos_t pos, vsp_pos_t sym_pos) {
    if (p->type_spec >= 0) {
        auto& type_sepc = p->CurTypeSpec();
        type_sepc.pos = pos;
        type_sepc.reference_pos = sym_pos;
        if (!IsEmpty(sym_pos))
            type_sepc.type_pointers++;
    }
}

void on_type_nullable(VsParser* p, vsp_pos_t pos, vsp_pos_t sym_pos) {
    if (p->type_spec >= 0) {
        auto& type_sepc = p->CurTypeSpec();
        type_sepc.pos = pos;
        type_sepc.nullable_pos = sym_pos;
    }
}

void enter_type_list(VsParser* p) {
    p->type_stack.push_back(p->type_spec);
}

void on_type_list_item(VsParser* p, int comma) {
    int list = p->type_stack.back();
    int item = p->type_spec;
    p->type_specs[list].type_params.push_back(item);
}

void exit_type_list(VsParser* p) {
    p->type_spec = p->type_stack.back();
    p->type_stack.pop_back();
}

void enter_generic_type(VsParser* p) {
    auto& type_sepc = p->CurTypeSpec();
    type_sepc.is_generic = true;
    enter_type_list(p);
}

void exit_generic_type(VsParser* p) {
    exit_type_list(p);
}

void enter_tuple_type(VsParser* p) {
    p->AddTypeSpec({});
    enter_type_list(p);
}

void exit_tuple_type(VsParser* p, vsp_pos_t pos) {
    exit_type_list(p);
    auto& type_sepc = p->CurTypeSpec();
    type_sepc.is_tuple = true;
    type_sepc.pos = pos;
}

void enter_function_type(VsParser* p) {
    VspType type_spec = {};
    type_spec.is_function = true;
    type_spec.return_type = -1;
    p->AddTypeSpec(type_spec);
    enter_type_list(p);
}

void on_function_type_parameters(VsParser* p) {
    exit_type_list(p);
}

void on_function_type_return_type(VsParser* p, vsp_pos_t return_type_pos) {
    if (!IsEmpty(return_type_pos)) {
        auto& type_sepc = p->CurTypeSpec();
        type_sepc.return_type = p->type_spec;
    }
}

void exit_function_type(VsParser* p, vsp_pos_t pos) {
    auto& type_sepc = p->CurTypeSpec();
    type_sepc.pos = pos;
}

#pragma endregion

#pragma region field_declaration

void enter_field_declaration(VsParser* p, vsp_pos_t kind_pos) {
    VSP_LOG("\r\n");
    VSP_LOG("%*sfield[%d, %d]:%.*s {\r\n", INDENT, PSTR(kind_pos));
    p->indent++;

    p->field_kind_pos = kind_pos;
}

void exit_field_declaration(VsParser* p) {
    p->indent--;
    VSP_LOG("%*s} //field\r\n", INDENT);
}

void on_field_specifier(VsParser* p, vsp_pos_t pos, vsp_pos_t name_pos, vsp_pos_t type_pos, vsp_pos_t init_pos) {
    VSP_LOG("%*sname[%d, %d]:%.*s, type[%d, %d]:%.*s, init[%d, %d]:%.*s\r\n", INDENT, PSTR(name_pos), PSTR(type_pos), PSTR(init_pos));

    auto field_decl = vsp_field_t{
        pos,
        p->field_kind_pos,
        name_pos,
        p->type_spec
    };

    p->struct_decl.fields.push_back(field_decl);
}

#pragma endregion

#pragma region function_declaration

void enter_function_declaration(VsParser* p, vsp_pos_t const_pos, vsp_pos_t func_pos, vsp_pos_t name_pos, vsp_pos_t generic_names_pos) {
    VSP_LOG("\r\n");
    VSP_LOG("%*sfunction[%d, %d], name[%d, %d]:%.*s {\r\n", INDENT, POS(func_pos), PSTR(name_pos));
    p->indent++;

    p->func_spec = (int)p->func_specs.size();
    p->func_specs.push_back({ func_pos | name_pos, const_pos, func_pos, name_pos, p->TakeGenericNames(), {}, -1 });
}

void exit_function_declaration(VsParser* p, vsp_pos_t pos) {
    p->indent--;
    VSP_LOG("%*s} //function\r\n", INDENT);

    auto& func_spec = p->func_specs[p->func_spec];
    func_spec.pos = pos;

    if (p->IsInsideStruct()) {
        // struct method
        p->struct_decl.methods.push_back(p->func_spec);
    }
    else {
        // global function
    }
}

void on_function_parameter(VsParser* p, vsp_pos_t pos, vsp_pos_t const_pos, vsp_pos_t name_pos, vsp_pos_t type_pos) {
    VSP_LOG("%*sparam[%d, %d]:%.*s\r\n", INDENT, PSTR(pos));
    p->param_list.push_back({ pos, const_pos, name_pos, p->type_spec });
}

void on_function_signature(VsParser* p, vsp_pos_t pos, vsp_pos_t type_pos) {
    VSP_LOG("%*sfunc_sign[%d, %d]:%.*s\r\n", INDENT, PSTR(pos));

    auto& func_spec = p->func_specs[p->func_spec];
    func_spec.params = p->TakeFunctionParameters();
    func_spec.type = !IsEmpty(type_pos) ? p->type_spec : -1;
}

#pragma endregion

void enter_type_declaration(VsParser* p, vsp_pos_t type_pos, vsp_pos_t name_pos) {
    VSP_LOG("\r\n");
    VSP_LOG("%*stype[%d, %d], name[%d, %d]:%.*s {\r\n", INDENT, POS(type_pos), PSTR(name_pos));
    p->indent++;
}

void exit_type_declaration(VsParser* p, vsp_pos_t values_pos) {
    VSP_LOG("%*svalues[%d, %d]:%.*s\r\n", INDENT, PSTR(values_pos));
    p->indent--;
    VSP_LOG("%*s} //type\r\n", INDENT);
}

void vsp_init_listener(vsp_internal_listener_t* l) {
#define LISTENER(callback) *((void**)&(l->callback)) = (void*)(&callback)

    LISTENER(on_module_clause);

    LISTENER(enter_import_declaration);
    LISTENER(on_import_module_name);
    LISTENER(on_import_alias);
    LISTENER(on_import_symbol);
    LISTENER(exit_import_declaration);

    LISTENER(enter_type_declaration);
    LISTENER(exit_type_declaration);

    LISTENER(on_generic_name);
    LISTENER(on_named_type);
    LISTENER(on_type_pointer);
    LISTENER(on_type_reference);
    LISTENER(on_type_nullable);
    LISTENER(on_type_list_item);
    LISTENER(enter_generic_type);
    LISTENER(exit_generic_type);
    LISTENER(enter_tuple_type);
    LISTENER(exit_tuple_type);
    LISTENER(enter_function_type);
    LISTENER(on_function_type_return_type);
    LISTENER(exit_function_type);

    LISTENER(enter_struct_declaration);
    LISTENER(on_struct_generic_names);
    LISTENER(on_struct_super_type);
    LISTENER(exit_struct_declaration);

    LISTENER(enter_field_declaration);
    LISTENER(on_field_specifier);
    LISTENER(exit_field_declaration);

    LISTENER(enter_function_declaration);
    LISTENER(on_function_parameter);
    LISTENER(on_function_signature);
    LISTENER(exit_function_declaration);
#undef LISTENER
}

/*-----------------------------------------------------------------------------
 *  Ast
 *---------------------------------------------------------------------------*/

//void vsp_print_ast_node(AstPtr node) {
//    if (!node) {
//        printf("null");
//    }
//    auto parent = node->parent.lock();
//    if (parent) {
//        vsp_print_ast_node(parent);
//        printf(".%s", node->name.data());
//    }
//    else {
//        printf("%s", node->name.data());
//    }
//}

void vsp_ast_push(vsparser_t* p_, const char* name, int term, size_t start, size_t end) {
    auto p = (VsParser*)p_;
    p->PushAstNode(name, term, start, end);
}

void vsp_ast_pop(vsparser_t* p_, const char* name, bool succeeded) {
    auto p = (VsParser*)p_;
    p->PopAstNode(name, succeeded);
}

void VsParser::PushAstNode(const char* name, int term, size_t start, size_t end) {
    assert(name);
    size_t line, col;
    size_t len_ = end - start;
    vsp_compute_line_and_column(this, start, line, col);
    auto token = term ? std::string_view(src + start, len_) : std::string_view();
    auto node = make_shared<AstNode>(line, col, name, src+start, term, start, len_);
    ast_nodes.push_back(node);
    if (cur_node) {
        node->parent = cur_node;
        cur_node->nodes.push_back(node);
    }
    else {
        root_node = node;
    }
    cur_node = node;
    //printf(">> ");
    //vsp_print_ast_node(node);
    //printf("\r\n");
}

void VsParser::PopAstNode(const char* name, bool succeeded) {
    //assert(name);
    //if (cur_node->name != name) {
    //    printf("missmatch: %s %s", name, cur_node->name.c_str());
    //}
    ast_nodes.pop_back();
    if (ast_nodes.empty()) {
        cur_node = nullptr;
    }
    else {
        cur_node = ast_nodes.back();
        if (!succeeded) {
            cur_node->nodes.pop_back();
        }
    }
    //printf("<< ");
    //vsp_print_ast_node(cur_node);
    //printf("\r\n");
}

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
 *  Console
 *---------------------------------------------------------------------------*/

#ifndef VSP_LIB

int vsp_readfile(VsParser* p, const char* FileName) {
    FILE* textfile;
    size_t length;
    char* text;

    textfile = fopen(FileName, "r");
    if (textfile == NULL)
        return 1;

    fseek(textfile, 0L, SEEK_END);
    length = ftell(textfile);
    fseek(textfile, 0L, SEEK_SET);

    text = (char*)calloc(length, sizeof(char));
    if (text == NULL)
        return 1;

    fread(text, sizeof(char), length, textfile);
    fclose(textfile);

    p->src = text;
    p->len = length;
    p->rp = 0;
    p->indent = 0;
    p->lines.push_back(0);

    return 0;
}

int main() {
    VsParser p = {};
    if (vsp_readfile(&p, "./tests/vsharp.vs") != 0)
        return 1;

    p.base.el = nullptr;
    vsp_init_listener(&p.base.il);
    vsp_clear_type_specs(&p);

    vsharp_context_t* ctx = vsharp_create(&p.base);
    vsharp_parse(ctx, NULL);
    vsharp_destroy(ctx);

    if (p.root_node)
    {
        p.root_node = AstOptimizer(false, get_ast_opt_rules()).optimize(p.root_node);
        cout << "--------------------------------------------------" << endl;
        cout << ast_to_s(p.root_node);
    }

    return 0;
}

#endif

/*-----------------------------------------------------------------------------
 *  Library
 *---------------------------------------------------------------------------*/

#ifdef VSP_LIB

void vsp_parse(const char* file_path, const char* src, int len, vsp_listener_t* listener) {

    vector<size_t> lines;

    VsParser p = {};
    p.src = src;
    p.len = len;
    p.rp = 0;
    p.indent = 0;
    p.lines.push_back(0);

    listener->ctx = &p;
    listener->num_lines = 0;
    p.base.el = listener;
    vsp_init_listener(&p.base.il);
    vsp_clear_type_specs(&p);

    printf("parse: %s\r\n", file_path);
    vsharp_context_t* ctx = vsharp_create(&p.base);
    vsharp_parse(ctx, NULL);
    vsharp_destroy(ctx);

    listener->num_lines = (int)p.lines.size();

    if (p.root_node && listener->on_ast_file) {
        p.root_node = AstOptimizer(false, get_ast_opt_rules()).optimize(p.root_node);
        //cout << "--------------------------------------------------" << endl;
        //cout << ast_to_s(p.root_node);

        ast_flush(p.root_node);
        listener->on_ast_file(listener->udata, p.root_node.get());
    }
}

void* vsp_named_nodes(void* parent_, const char* name, size_t len, size_t* num) {
    auto parent = (AstNode*)parent_;
    if (parent) {
        auto it = parent->cached_map.find(std::string_view(name, len));
        if (it != parent->cached_map.end()) {
            if (!it->second.empty()) {
                if (num)
                    *num = it->second.size();
                return it->second.data();
            }
        }
    }
    return nullptr;
}

void* vsp_sub_nodes(void* parent_, size_t* num) {
    auto parent = (AstNode*)parent_;
    if (parent) {
        if (num)
            *num = parent->cached_nodes.size();
        return parent->cached_nodes.data();
    }
    return nullptr;
}

//void* vsp_sub_node(void* parent_, size_t index) {
//    auto parent = (AstNode*)parent_;
//    if (parent && index < parent->cached_nodes.size()) {
//        return parent->cached_nodes.at(index);
//    }
//    return nullptr;
//}

void vsp_type_desc(void* ctx, int id, vsp_type_desc_t* type_desc) {
    VsParser* p = (VsParser*)ctx;
    if (id < p->type_specs.size()) {
        const auto& type_spec = p->type_specs.at(id);
        type_desc->pos = type_spec.pos;
        type_desc->mod_pos = type_spec.mod_pos;
        type_desc->name_pos = type_spec.name_pos;
        type_desc->p_type_params = type_spec.type_params.data();
        type_desc->n_type_params = (int)type_spec.type_params.size();
        type_desc->is_generic = type_spec.is_generic;
        type_desc->is_tuple = type_spec.is_tuple;
        type_desc->is_function = type_spec.is_function;
        type_desc->return_type = type_spec.return_type;
        type_desc->type_pointers = type_spec.type_pointers;
        type_desc->reference_pos = type_spec.reference_pos;
        type_desc->nullable_pos = type_spec.nullable_pos;
    }
}

void vsp_func_desc(void* ctx, int id, vsp_func_desc_t* func_desc) {
    VsParser* p = (VsParser*)ctx;
    if (id < p->func_specs.size()) {
        const auto& func_spec = p->func_specs.at(id);
        func_desc->pos = func_spec.pos;
        func_desc->const_pos = func_spec.const_pos;
        func_desc->func_pos = func_spec.func_pos;
        func_desc->name_pos = func_spec.name_pos;
        func_desc->p_generic_names = func_spec.generic_names.data();
        func_desc->n_generic_names = (int)func_spec.generic_names.size();
        func_desc->p_params = func_spec.params.data();
        func_desc->n_params = (int)func_spec.params.size();
        func_desc->type = func_spec.type;
    }
}

#endif

/*-----------------------------------------------------------------------------
 *  VSharp
 *---------------------------------------------------------------------------*/

int vsp_getchar(VsParser* p) {
    if (!p->src || p->rp >= p->len)
        return -1;
    int ch = p->src[p->rp++];
    if (ch == '\r') {
        if (p->rp < p->len) {
            if (p->src[p->rp] != '\n') {
                vsp_newline(p, p->rp);
            }
        }
        else {
            vsp_newline(p, p->rp);
        }
    }
    else if (ch == '\n') {
        vsp_newline(p, p->rp);
    }
    return ch ? ch : -1;
}

void vsp_debug(VsParser* p, int event, const char* rule, int level, size_t pos, const char* buffer, int length) {
#ifndef VSP_LIB
    //static const char* dbg_str[] = { "Evaluating rule", "Matched rule", "Abandoning rule" };
    //fprintf(stderr, "%*s%s %s @%zu [%.*s]\n", level * 2, "", dbg_str[event], rule, pos, length, buffer);
#endif
}

void vsp_error(VsParser* p) {
    fprintf(stderr, "Syntax error\n");
}

#ifdef __cplusplus
}
#endif

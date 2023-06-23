#include "vsparser.h"
#include "vsharp.h"

#include <stdio.h>
#include <stdlib.h>

#include <vector>
#include <string>

using namespace std;

typedef struct vsparserex_t {
    vsparser_t base;
    const char* src;
    size_t len;
    size_t rp;
    int indent;
    vector<size_t> lines;

    struct {
        vsp_pos_t import_pos;
        vsp_pos_t mod_pos;
        vsp_pos_t as_pos;
        vsp_pos_t alias_pos;
        string mod_name;
        vector<vsp_pos_t> syms;
    } import_decl;
}
vsparserex_t;

string vsp_str(vsparserex_t* p, const vsp_pos_t& pos) {
    if (pos.start < 0)
        return string();
    return string(p->src + pos.start, pos.end - pos.start);
}

vsp_strv_t vsp_strv(const string& str) {
    return vsp_strv_t{ str.c_str(), (int)str.size() };
}

vsp_strv_t vsp_strv(vsparserex_t* p, const vsp_pos_t& pos) {
    return vsp_strv_t{ p->src+pos.start, pos.end-pos.start };
}

void vsp_newline(vsparserex_t* p, size_t pos) {
    p->lines.push_back(pos);
}

void vsp_compute_line_and_column(vsparserex_t* p, size_t pos, int& line, int& col) {
    auto n = p->lines.size();
    for (auto i = n; i >= 1; i--) {
        auto line_start = p->lines.at(i - 1);
        if (pos >= line_start) {
            line = (int)(i - 1);
            col = (int)(pos - line_start);
            return;
        }
    }
    line = -1;
    col = -1;
}

#ifdef __cplusplus
extern "C" {
#endif

int vsp_getchar(vsparserex_t* p) {
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

vsp_pos_t vsp_pos(vsparser_t* p, size_t start, size_t end) {
    vsp_pos_t pos;
    if (!p || start >= end) {
        pos.start = -1;
        pos.end = -1;
        pos.line = -1;
        pos.col = -1;
    }
    else {
        pos.start = (int)start;
        pos.end = (int)end;
        vsp_compute_line_and_column((vsparserex_t*)p, start, pos.line, pos.col);
    }
    return pos;
}

void vsp_debug(vsparserex_t* p, int event, const char* rule, int level, size_t pos, const char* buffer, int length) {
#ifndef VSP_LIB
    //static const char* dbg_str[] = { "Evaluating rule", "Matched rule", "Abandoning rule" };
    //fprintf(stderr, "%*s%s %s @%zu [%.*s]\n", level * 2, "", dbg_str[event], rule, pos, length, buffer);
#endif
}

void vsp_error(vsparserex_t* p) {
    fprintf(stderr, "Syntax error\n");
}

#define STR(_POS)       _POS.end-_POS.start, p->src+_POS.start
#define POS(_POS)       _POS.start, _POS.end
#define PSTR(_POS)      POS(_POS), STR(_POS)
#define INDENT          p->indent*2, ""

#ifdef VSP_LIB
#define L_LOG(...)
#else
#define L_LOG(...)\
    printf(__VA_ARGS__)
#endif

#define L_INVOKE(callback, ...)\
    if (p->base.el && p->base.el->callback) {\
        p->base.el->callback(p->base.el->udata, ##__VA_ARGS__);\
    }

void on_module_clause(vsparserex_t* p, vsp_pos_t module_pos, vsp_pos_t name_pos) {
    L_LOG("%*smodule[%d, %d], name[%d, %d]:%.*s\r\n", INDENT, POS(module_pos), PSTR(name_pos));
}

void enter_import_declaration(vsparserex_t* p, vsp_pos_t import_pos, vsp_pos_t mod_pos) {
    L_LOG("\r\n");
    L_LOG("%*simport[%d, %d], module[%d, %d]:%.*s {\r\n", INDENT, POS(import_pos), PSTR(mod_pos));
    p->indent++;

    p->import_decl.import_pos = import_pos;
    p->import_decl.mod_pos = mod_pos;
}

void exit_import_declaration(vsparserex_t* p) {
    auto& import_decl = p->import_decl;
    if (import_decl.mod_name.size()) {
        // case 2
        if (import_decl.syms.size()) {
            L_INVOKE(on_import_declaration, import_decl.import_pos,
                import_decl.mod_pos, vsp_strv(import_decl.mod_name),
                vsp_pos(nullptr, 0, 0), import_decl.alias_pos, 
                import_decl.syms.data(), (int)import_decl.syms.size());
        }
        // case 1
        else {
            L_INVOKE(on_import_declaration, import_decl.import_pos,
                import_decl.mod_pos, vsp_strv(import_decl.mod_name),
                vsp_pos(nullptr, 0, 0), import_decl.alias_pos,
                nullptr, 0);
        }
        import_decl = {};
    }

    p->indent--;
    L_LOG("%*s} //import\r\n", INDENT);
}

void on_import_module_name(vsparserex_t* p, vsp_pos_t name_pos, int dot) {
    p->import_decl.alias_pos = name_pos;
    if (dot) {
        p->import_decl.mod_name += '.';
        p->import_decl.mod_name += vsp_str(p, name_pos);
    }
    else {
        p->import_decl.mod_name = vsp_str(p, name_pos);
    }
}

void on_import_alias(vsparserex_t* p, vsp_pos_t as_pos, vsp_pos_t alias_pos) {
    L_LOG("%*sas[%d, %d], alias[%d, %d]:%.*s\r\n", INDENT, POS(as_pos), PSTR(alias_pos));

    auto& import_decl = p->import_decl;
    import_decl.as_pos = as_pos;
    import_decl.alias_pos = alias_pos;

    L_INVOKE(on_import_declaration, import_decl.import_pos, 
        import_decl.mod_pos, vsp_strv(import_decl.mod_name), 
        import_decl.as_pos, import_decl.alias_pos, 
        nullptr, 0);

    import_decl = {};
}

void on_import_symbol(vsparserex_t* p, vsp_pos_t symbol_pos) {
    L_LOG("%*ssymbol[%d, %d]:%.*s\r\n", INDENT, PSTR(symbol_pos));

    auto& import_decl = p->import_decl;
    import_decl.syms.push_back(symbol_pos);
}

void enter_struct_declaration(vsparserex_t* p, vsp_pos_t struct_pos, vsp_pos_t name_pos) {
    L_LOG("\r\n");
    L_LOG("%*sstruct[%d, %d]:%.*s, name[%d, %d]:%.*s {\r\n", INDENT, PSTR(struct_pos), PSTR(name_pos));
    p->indent++;
}

void exit_struct_declaration(vsparserex_t* p) {
    p->indent--;
    L_LOG("%*s} //struct\r\n", INDENT);
}

void on_type_parameter(vsparserex_t* p, vsp_pos_t type_param_pos) {
    L_LOG("%*stype_param[%d, %d]:%.*s\r\n", INDENT, PSTR(type_param_pos));
}

void on_type_parameters(vsparserex_t* p, vsp_pos_t type_params_pos) {
    L_LOG("%*stype_params[%d, %d]:%.*s\r\n", INDENT, PSTR(type_params_pos));
}

void on_super_type_list(vsparserex_t* p, vsp_pos_t type_list_pos) {
    L_LOG("%*ssuper_types[%d, %d]:%.*s\r\n", INDENT, PSTR(type_list_pos));
}

void enter_field_declaration(vsparserex_t* p, vsp_pos_t field_pos) {
    L_LOG("\r\n");
    L_LOG("%*sfield[%d, %d]:%.*s {\r\n", INDENT, PSTR(field_pos));
    p->indent++;
}

void exit_field_declaration(vsparserex_t* p) {
    p->indent--;
    L_LOG("%*s} //field\r\n", INDENT);
}

void on_field_specifier(vsparserex_t* p, vsp_pos_t name_pos, vsp_pos_t type_pos, vsp_pos_t init_pos) {
    L_LOG("%*sname[%d, %d]:%.*s, type[%d, %d]:%.*s, init[%d, %d]:%.*s\r\n", INDENT, PSTR(name_pos), PSTR(type_pos), PSTR(init_pos));
}

void enter_function_declaration(vsparserex_t* p, vsp_pos_t func_pos, vsp_pos_t name_pos) {
    L_LOG("\r\n");
    L_LOG("%*sfunction[%d, %d], name[%d, %d]:%.*s {\r\n", INDENT, POS(func_pos), PSTR(name_pos));
    p->indent++;
}

void exit_function_declaration(vsparserex_t* p) {
    p->indent--;
    L_LOG("%*s} //function\r\n", INDENT);
}

void on_function_signature(vsparserex_t* p, vsp_pos_t func_sign_pos) {
    L_LOG("%*sfunc_sign[%d, %d]:%.*s\r\n", INDENT, PSTR(func_sign_pos));
}

void enter_type_declaration(vsparserex_t* p, vsp_pos_t type_pos, vsp_pos_t name_pos) {
    L_LOG("\r\n");
    L_LOG("%*stype[%d, %d], name[%d, %d]:%.*s {\r\n", INDENT, POS(type_pos), PSTR(name_pos));
    p->indent++;
}

void exit_type_declaration(vsparserex_t* p, vsp_pos_t values_pos) {
    L_LOG("%*svalues[%d, %d]:%.*s\r\n", INDENT, PSTR(values_pos));
    p->indent--;
    L_LOG("%*s} //type\r\n", INDENT);
}

void vsp_init_listener(vsp_listener_t* l) {
#define LISTENER(callback) *((void**)&(l->callback)) = (void*)(&callback)

    LISTENER(on_module_clause);

    LISTENER(enter_import_declaration);
    LISTENER(on_import_module_name);
    LISTENER(on_import_alias);
    LISTENER(on_import_symbol);
    LISTENER(exit_import_declaration);

    LISTENER(enter_type_declaration);
    LISTENER(exit_type_declaration);

    LISTENER(enter_struct_declaration);
    LISTENER(on_type_parameter);
    LISTENER(on_type_parameters);
    LISTENER(on_super_type_list);
    LISTENER(exit_struct_declaration);

    LISTENER(enter_field_declaration);
    LISTENER(on_field_specifier);
    LISTENER(exit_field_declaration);

    LISTENER(enter_function_declaration);
    LISTENER(on_function_signature);
    LISTENER(exit_function_declaration);
#undef LISTENER
}

#ifdef VSP_LIB

int vsp_parse(const char* file_path, const char* src, int len, vsp_listener_t* listener) {

    vector<size_t> lines;

    vsparserex_t p;
    p.src = src;
    p.len = len;
    p.rp = 0;
    p.indent = 0;
    p.lines.push_back(0);

    p.base.el = listener;
    p.base.il.udata = nullptr;
    vsp_init_listener(&p.base.il);

    printf("parse: %s\r\n", file_path);
    vsharp_context_t* ctx = vsharp_create(&p.base);
    vsharp_parse(ctx, NULL);
    vsharp_destroy(ctx);

    return 0;
}

#else

int vsp_readfile(vsparserex_t* p, const char* FileName) {
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
    vsparserex_t p = {0};
    if (vsp_readfile(&p, "./tests/vsharp.vs") != 0)
        return 1;

    p.base.el = nullptr;
    p.base.il.udata = nullptr;
    vsp_init_listener(&p.base.il);

    vsharp_context_t* ctx = vsharp_create(&p.base);
    vsharp_parse(ctx, NULL);
    vsharp_destroy(ctx);

    return 0;
}

#endif

#ifdef __cplusplus
}
#endif

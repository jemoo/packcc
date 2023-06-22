#include "vsparser.h"
#include "vsharp.h"

#include <stdio.h>
#include <stdlib.h>

int vsp_getchar(vsparser_t* p) {
    if (!p->src || p->rp >= p->len)
        return -1;
    int ch = p->src[p->rp++];
    return ch ? ch : -1;
}

void vsp_debug(vsparser_t* p, int event, const char* rule, int level, size_t pos, const char* buffer, int length) {
#ifndef VSP_LIB
    //static const char* dbg_str[] = { "Evaluating rule", "Matched rule", "Abandoning rule" };
    //fprintf(stderr, "%*s%s %s @%zu [%.*s]\n", level * 2, "", dbg_str[event], rule, pos, length, buffer);
#endif
}

void vsp_error(vsparser_t* p) {
    fprintf(stderr, "Syntax error\n");
}

vsp_pos_t vsp_pos(size_t start, size_t end) {
    vsp_pos_t pos;
    pos.start = (int)start;
    pos.end = (int)end;
    return pos;
}

#ifdef VSP_LIB

int vsp_parse(const char* file_path, const char* src, int len, vsp_listener_t* listener) {
    vsparser_t p;
    p.src = src;
    p.len = len;
    p.rp = 0;
    p.listener = listener;

    printf("parse: %s\r\n", file_path);
    vsharp_context_t* ctx = vsharp_create(&p);
    vsharp_parse(ctx, NULL);
    vsharp_destroy(ctx);

    return 0;
}

#else

#define STR(_POS)       _POS.end-_POS.start, p->src+_POS.start
#define POS(_POS)       _POS.start, _POS.end
#define PSTR(_POS)      POS(_POS), STR(_POS)
#define INDENT          p->indent*2, ""

void on_module_clause(vsparser_t* p, vsp_pos_t module_pos, vsp_pos_t name_pos) {
    printf("%*smodule[%d, %d], name[%d, %d]:%.*s\r\n", INDENT, POS(module_pos), PSTR(name_pos));
}

void enter_import_declaration(vsparser_t* p, vsp_pos_t import_pos, vsp_pos_t module_pos) {
    printf("\r\n");
    printf("%*simport[%d, %d], module[%d, %d]:%.*s {\r\n", INDENT, POS(import_pos), PSTR(module_pos));
    p->indent++;
}

void exit_import_declaration(vsparser_t* p) {
    p->indent--;
    printf("%*s} //import\r\n", INDENT);
}

void on_import_alias(vsparser_t* p, vsp_pos_t as_pos, vsp_pos_t alias_pos) {
    printf("%*sas[%d, %d], alias[%d, %d]:%.*s\r\n", INDENT, POS(as_pos), PSTR(alias_pos));
}

void on_import_symbol(vsparser_t* p, vsp_pos_t symbol_pos) {
    printf("%*ssymbol[%d, %d]:%.*s\r\n", INDENT, PSTR(symbol_pos));
}

void enter_struct_declaration(vsparser_t* p, vsp_pos_t struct_pos, vsp_pos_t name_pos) {
    printf("\r\n");
    printf("%*sstruct[%d, %d]:%.*s, name[%d, %d]:%.*s {\r\n", INDENT, PSTR(struct_pos), PSTR(name_pos));
    p->indent++;
}

void exit_struct_declaration(vsparser_t* p) {
    p->indent--;
    printf("%*s} //struct\r\n", INDENT);
}

void on_type_parameter(vsparser_t* p, vsp_pos_t type_param_pos) {
    printf("%*stype_param[%d, %d]:%.*s\r\n", INDENT, PSTR(type_param_pos));
}

void on_type_parameters(vsparser_t* p, vsp_pos_t type_params_pos) {
    printf("%*stype_params[%d, %d]:%.*s\r\n", INDENT, PSTR(type_params_pos));
}

void on_super_type_list(vsparser_t* p, vsp_pos_t type_list_pos) {
    printf("%*ssuper_types[%d, %d]:%.*s\r\n", INDENT, PSTR(type_list_pos));
}

void enter_field_declaration(vsparser_t* p, vsp_pos_t field_pos) {
    printf("\r\n");
    printf("%*sfield[%d, %d]:%.*s {\r\n", INDENT, PSTR(field_pos));
    p->indent++;
}

void exit_field_declaration(vsparser_t* p) {
    p->indent--;
    printf("%*s} //field\r\n", INDENT);
}

void on_field_specifier(vsparser_t* p, vsp_pos_t name_pos, vsp_pos_t type_pos, vsp_pos_t init_pos) {
    printf("%*sname[%d, %d]:%.*s, type[%d, %d]:%.*s, init[%d, %d]:%.*s\r\n", INDENT, PSTR(name_pos), PSTR(type_pos), PSTR(init_pos));
}

void enter_function_declaration(vsparser_t* p, vsp_pos_t func_pos, vsp_pos_t name_pos) {
    printf("\r\n");
    printf("%*sfunction[%d, %d], name[%d, %d]:%.*s {\r\n", INDENT, POS(func_pos), PSTR(name_pos));
    p->indent++;
}

void exit_function_declaration(vsparser_t* p) {
    p->indent--;
    printf("%*s} //function\r\n", INDENT);
}

void on_function_signature(vsparser_t* p, vsp_pos_t func_sign_pos) {
    printf("%*sfunc_sign[%d, %d]:%.*s\r\n", INDENT, PSTR(func_sign_pos));
}

void enter_type_declaration(vsparser_t* p, vsp_pos_t type_pos, vsp_pos_t name_pos) {
    printf("\r\n");
    printf("%*stype[%d, %d], name[%d, %d]:%.*s {\r\n", INDENT, POS(type_pos), PSTR(name_pos));
    p->indent++;
}

void exit_type_declaration(vsparser_t* p, vsp_pos_t values_pos) {
    printf("%*svalues[%d, %d]:%.*s\r\n", INDENT, PSTR(values_pos));
    p->indent--;
    printf("%*s} //type\r\n", INDENT);
}

void vsp_init_listener(vsp_listener_t* l) {
    l->on_module_clause = &on_module_clause;

    l->enter_import_declaration = &enter_import_declaration;
    l->on_import_alias = &on_import_alias;
    l->on_import_symbol = &on_import_symbol;
    l->exit_import_declaration = &exit_import_declaration;

    l->enter_type_declaration = &enter_type_declaration;
    l->exit_type_declaration = &exit_type_declaration;

    l->enter_struct_declaration = &enter_struct_declaration;
    l->on_type_parameter = &on_type_parameter;
    l->on_type_parameters = &on_type_parameters;
    l->on_super_type_list = &on_super_type_list;
    l->exit_struct_declaration = &exit_struct_declaration;

    l->enter_field_declaration = &enter_field_declaration;
    l->on_field_specifier = &on_field_specifier;
    l->exit_field_declaration = &exit_field_declaration;

    l->enter_function_declaration = &enter_function_declaration;
    l->on_function_signature = &on_function_signature;
    l->exit_function_declaration = &exit_function_declaration;
}

int vsp_readfile(vsparser_t* p, const char* FileName) {
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

    return 0;
}

int main() {
    vsparser_t p;
    if (vsp_readfile(&p, "./tests/vsharp.vs") != 0)
        return 1;

    vsp_listener_t listener = {0};
    p.listener = &listener;
    p.listener->udata = &p;
    p.indent = 0;
    vsp_init_listener(p.listener);

    vsharp_context_t* ctx = vsharp_create(&p);
    vsharp_parse(ctx, NULL);
    vsharp_destroy(ctx);

    return 0;
}

#endif

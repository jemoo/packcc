#ifndef VSPARSER_H
#define VSPARSER_H

#include "./include/vsp.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct vsp_internal_listener_t {
        void (*on_module_clause)(void* udata, vsp_pos_t module_pos, vsp_pos_t name_pos);

        void (*enter_import_declaration)(void* udata, vsp_pos_t import_pos, vsp_pos_t mod_pos);
        void (*on_import_module_name)(void* udata, vsp_pos_t name_pos, int dot);
        void (*on_import_alias)(void* udata, vsp_pos_t as_pos, vsp_pos_t alias_pos);
        void (*on_import_symbol)(void* udata, vsp_pos_t symbol_pos);
        void (*exit_import_declaration)(void* udata);

        void (*enter_type_declaration)(void* udata, vsp_pos_t type_pos, vsp_pos_t name_pos);
        void (*on_type_decl_generic_names)(void* udata, vsp_pos_t generic_names_pos);
        void (*exit_type_declaration)(void* udata, vsp_pos_t values_pos);

        void (*on_generic_name)(void* udata, vsp_pos_t type_param_pos);
        void (*on_named_type)(void* udata, vsp_pos_t mod_pos, vsp_pos_t name_pos);
        void (*on_type_pointer)(void* udata, vsp_pos_t pos, vsp_pos_t sym_pos);
        void (*on_type_reference)(void* udata, vsp_pos_t pos, vsp_pos_t sym_pos);
        void (*on_type_nullable)(void* udata, vsp_pos_t pos, vsp_pos_t sym_pos);
        void (*on_type_list_item)(void* udata, int comma);

        void (*enter_generic_type)(void* udata);
        void (*exit_generic_type)(void* udata);

        void (*enter_tuple_type)(void* udata);
        void (*exit_tuple_type)(void* udata, vsp_pos_t pos);

        void (*enter_function_type)(void* udata);
        void (*on_function_type_parameters)(void* udata);
        void (*on_function_type_return_type)(void* udata, vsp_pos_t return_type_pos);
        void (*exit_function_type)(void* udata, vsp_pos_t pos);

        void (*enter_struct_declaration)(void* udata, vsp_pos_t kind_pos, vsp_pos_t name_pos);
        void (*on_struct_generic_names)(void* udata, vsp_pos_t generic_names_pos);
        void (*on_struct_super_type)(void* udata, int comma);
        void (*exit_struct_declaration)(void* udata, vsp_pos_t pos);

        void (*enter_field_declaration)(void* udata, vsp_pos_t kind_pos);
        void (*on_field_specifier)(void* udata, vsp_pos_t pos, vsp_pos_t name_pos, vsp_pos_t type_pos, vsp_pos_t init_pos);
        void (*exit_field_declaration)(void* udata);

        void (*enter_function_declaration)(void* udata, vsp_pos_t const_pos, vsp_pos_t func_pos, vsp_pos_t name_pos, vsp_pos_t generic_names_pos);
        void (*on_function_parameter)(void* udata, vsp_pos_t pos, vsp_pos_t const_pos, vsp_pos_t name_pos, vsp_pos_t type_pos);
        void (*on_function_signature)(void* udata, vsp_pos_t pos, vsp_pos_t type_pos);
        void (*exit_function_declaration)(void* udata, vsp_pos_t pos);
    }
    vsp_internal_listener_t;

    typedef struct vsparser_t {
        vsp_internal_listener_t il;
        vsp_listener_t* el;
    }
    vsparser_t;

    vsp_pos_t vsp_pos(vsparser_t* p, size_t start, size_t end);

#ifdef __cplusplus
}
#endif

#endif //VSP_PARSER_H

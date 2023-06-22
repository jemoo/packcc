#ifndef VSP_H
#define VSP_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct vsp_pos_t {
        int start;
        int end;
    }
    vsp_pos_t;

    typedef struct vsp_listener_t {
        void* udata;

        void (*on_module_clause)(void* udata, vsp_pos_t module_pos, vsp_pos_t name_pos);

        void (*enter_import_declaration)(void* udata, vsp_pos_t import_pos, vsp_pos_t module_pos);
        void (*on_import_alias)(void* udata, vsp_pos_t as_pos, vsp_pos_t alias_pos);
        void (*on_import_symbol)(void* udata, vsp_pos_t symbol_pos);
        void (*exit_import_declaration)(void* udata);

        void (*enter_type_declaration)(void* udata, vsp_pos_t type_pos, vsp_pos_t name_pos);
        void (*exit_type_declaration)(void* udata, vsp_pos_t values_pos);

        void (*enter_struct_declaration)(void* udata, vsp_pos_t struct_pos, vsp_pos_t name_pos);
        void (*on_type_parameter)(void* udata, vsp_pos_t type_param_pos);
        void (*on_type_parameters)(void* udata, vsp_pos_t type_params_pos);
        void (*on_super_type_list)(void* udata, vsp_pos_t type_list_pos);
        void (*exit_struct_declaration)(void* udata);

        void (*enter_field_declaration)(void* udata, vsp_pos_t field_pos);
        void (*on_field_specifier)(void* udata, vsp_pos_t name_pos, vsp_pos_t type_pos, vsp_pos_t init_pos);
        void (*exit_field_declaration)(void* udata);

        void (*enter_function_declaration)(void* udata, vsp_pos_t func_pos, vsp_pos_t name_pos);
        void (*on_function_signature)(void* udata, vsp_pos_t func_sign_pos);
        void (*exit_function_declaration)(void* udata);
    }
    vsp_listener_t;

    int vsp_parse(const char* file_path, const char* src, int len, vsp_listener_t* listener);

#ifdef __cplusplus
}
#endif

#endif //VSP_H

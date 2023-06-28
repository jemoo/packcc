#ifndef VSP_H
#define VSP_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct vsp_pos_t {
        int start;
        int end;
        int line;
        int col;
    } vsp_pos_t;

    typedef struct vsp_strv_t {
        const char* buf;
        int len;
    } vsp_strv_t;

    typedef struct vsp_field_t {
        vsp_pos_t pos;
        vsp_pos_t kind_pos;
        vsp_pos_t name_pos;
        int type;
    } vsp_field_t;

    typedef struct vsp_param_t {
        vsp_pos_t pos;
        vsp_pos_t const_pos;
        vsp_pos_t name_pos;
        int type;
    } vsp_param_t;

    typedef struct vsp_listener_t {
        void* udata;
        void* ctx;
        int num_lines;
        
        void (*on_module_clause)(void* udata, vsp_pos_t module_pos, vsp_pos_t name_pos);
        
        void (*on_import_declaration)(void* udata, vsp_pos_t import_pos, vsp_pos_t mod_pos, vsp_strv_t mod_name, 
            vsp_pos_t as_pos, vsp_pos_t alias_pos, const vsp_pos_t* p_syms, int n_syms);

        void (*on_struct_declaration)(void* ctx, vsp_pos_t pos, vsp_pos_t kind_pos, vsp_pos_t name_pos,
            const vsp_pos_t* p_generic_names, int n_generic_names, const int* p_supers, int n_supers,
            const vsp_field_t* p_fields, int n_fields, const int* p_methods, int n_methods);

        void (*on_ast_file)(void* udata, void* ast);

    } vsp_listener_t;

    void vsp_parse(const char* file_path, const char* src, int len, vsp_listener_t* listener);
    void* vsp_named_nodes(void* ast_node, const char* name, size_t len, size_t* num);
    void* vsp_sub_nodes(void* ast_node, size_t* num);

    typedef struct vsp_type_desc_t {
        vsp_pos_t pos;
        vsp_pos_t mod_pos;
        vsp_pos_t name_pos;
        int type_pointers;
        vsp_pos_t reference_pos;
        vsp_pos_t nullable_pos;
        int is_tuple;
        int is_generic;
        int is_function;
        int return_type;
        const int* p_type_params;
        int n_type_params;
    } vsp_type_desc_t;

    void vsp_type_desc(void* ctx, int id, vsp_type_desc_t* type_desc);

    typedef struct vsp_func_desc_t {
        vsp_pos_t pos;
        vsp_pos_t const_pos;
        vsp_pos_t func_pos;
        vsp_pos_t name_pos;
        const vsp_pos_t* p_generic_names;
        int n_generic_names;
        const vsp_param_t* p_params;
        int n_params;
        int type;
    } vsp_func_desc_t;

    void vsp_func_desc(void* ctx, int id, vsp_func_desc_t* func_desc);

#ifdef __cplusplus
}
#endif

#endif //VSP_H

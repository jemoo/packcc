#ifndef VSP_HPP
#define VSP_HPP

#ifdef __cplusplus

#include "ast.h"

extern "C" {

    void vsp_parse2(const char* file_path, const char* src, int len, void (*callback)(void*, AstNode*, size_t), void* user_data);

}
#endif

#endif //VSP_HPP

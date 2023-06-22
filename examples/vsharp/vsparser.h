#ifndef VSPARSER_H
#define VSPARSER_H

#include "./include/vsp.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct vsparser_t {
        const char* src;
        size_t len;
        size_t rp;
        vsp_listener_t* listener;
        int indent;
    }
    vsparser_t;

    vsp_pos_t vsp_pos(size_t start, size_t end);

#ifdef __cplusplus
}
#endif

#endif //VSP_PARSER_H

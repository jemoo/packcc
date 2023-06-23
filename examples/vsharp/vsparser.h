#ifndef VSPARSER_H
#define VSPARSER_H

#include "./include/vsp.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct vsparser_t {
        vsp_listener_t il;
        vsp_listener_t* el;
    }
    vsparser_t;

    vsp_pos_t vsp_pos(vsparser_t* p, size_t start, size_t end);

#ifdef __cplusplus
}
#endif

#endif //VSP_PARSER_H

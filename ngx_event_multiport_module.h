/*
 * Copyright (C) AlexWoo(Wu Jie) wj19840501@gmail.com
 */


#ifndef _NGX_EVENT_MULTIPORT_MODULE_H_INCLUDED_
#define _NGX_EVENT_MULTIPORT_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


ngx_int_t ngx_event_multiport_get_multiport(struct sockaddr *sa,
        ngx_str_t *multiport, ngx_int_t pslot);

#endif


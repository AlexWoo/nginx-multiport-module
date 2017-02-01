/*
 * Copyright (C) AlexWoo(Wu Jie) wj19840501@gmail.com
 */


#ifndef _NGX_EVENT_MULTIPORT_MODULE_H_INCLUDED_
#define _NGX_EVENT_MULTIPORT_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * sa: return sockaddr of multiport, sa must alloc memory for store sockaddr
 * multiport: base multiport configure in multi_listen of event block
 *  multiport format support bellow:
 *      port only: port
 *      IPv4: host:port     host must be ipaddr of IPv4 or *
 *      IPv6: [host]:port   host must be ipaddr of IPv6
 *      Unix: unix:/path
 * pslot: process_slot
 * return value: socklen of sa, 0 for failed
 */
socklen_t ngx_event_multiport_get_multiport(struct sockaddr *sa,
        ngx_str_t *multiport, ngx_int_t pslot);

#endif


/*
 * Copyright (C) AlexWoo(Wu Jie) wj19840501@gmail.com
 */


#ifndef _NGX_MULTIPORT_H_INCLUDED_
#define _NGX_MULTIPORT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * return value:
 *      NGX_OK for success, NGX_ERROR for failed
 * paras:
 *      pool: pool for port memory alloc
 *      port: process real listen port while process_slot is pslot
 *      multiport: port configure for processes, format as below:
 *          port only: port
 *          IPv4: host:port     host must be ipaddr of IPv4 or *
 *          IPv6: [host]:port   host must be ipaddr of IPv6
 *          Unix: unix:/path
 *      pslot: process_slot
 */
ngx_int_t ngx_multiport_get_port(ngx_pool_t *pool, ngx_str_t *port,
        ngx_str_t *multiport, ngx_int_t pslot);


/*
 * return value:
 *      ngx_process_slot for successd, NGX_ERROR for failed
 * paras:
 *      wpid: worker process id, 0 to ccf->worker_processes - 1
 */
ngx_int_t ngx_multiport_get_slot(ngx_uint_t wpid);


#endif

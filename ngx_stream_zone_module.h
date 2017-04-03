/*
 * Copyright (C) AlexWoo(Wu Jie) wj19840501@gmail.com
 */


#ifndef _NGX_STREAM_ZONE_MODULE_H_INCLUDED_
#define _NGX_STREAM_ZONE_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * name: stream name
 * return value: process_slot for owner of stream, NGX_ERROR for error
 */
ngx_int_t ngx_stream_zone_insert_stream(ngx_str_t *name);

/*
 * name: stream name
 */
void ngx_stream_zone_delete_stream(ngx_str_t *name);

/*
 * print stream zone
 * NGX_DEBUG only
 */
void ngx_stream_zone_print();

#endif


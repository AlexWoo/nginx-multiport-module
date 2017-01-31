/*
 * Copyright (C) AlexWoo(Wu Jie) wj19840501@gmail.com
 */


#ifndef _NGX_PROCESS_SLOT_MODULE_H_INCLUDED_
#define _NGX_PROCESS_SLOT_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * wpid: worker process id, 0 to ccf->worker_processes - 1
 * return value: sucessed return ngx_process_slot, else return -1
 */
ngx_int_t ngx_process_slot_get_slot(ngx_uint_t wpid);

/*
 * print process slot
 * NGX_DEBUG only
 */
void ngx_process_slot_print();

#endif


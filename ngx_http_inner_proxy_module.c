/*
 * Copyright (C) AlexWoo(Wu Jie) wj19840501@gmail.com
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_multiport.h"


typedef struct {
	ngx_str_t                           multiport;
    ngx_str_t                           uri;
} ngx_http_inner_proxy_conf_t;

typedef struct {
    ngx_str_t                           port;
    ngx_flag_t                          sent;
} ngx_http_inner_proxy_ctx_t;


static ngx_int_t ngx_http_inner_proxy_filter_init(ngx_conf_t *cf);

static void *ngx_http_inner_proxy_create_conf(ngx_conf_t *cf);
static char *ngx_http_inner_proxy_merge_conf(ngx_conf_t *cf,
       void *parent, void *child);
static char *ngx_http_inner_proxy(ngx_conf_t *cf, ngx_command_t *cmd,
       void *conf);


static ngx_command_t  ngx_http_inner_proxy_commands[] = {

    { ngx_string("inner_proxy"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
      ngx_http_inner_proxy,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_inner_proxy_module_ctx = {
    NULL,                                   /* preconfiguration */
    ngx_http_inner_proxy_filter_init,         /* postconfiguration */

    NULL,                                   /* create main configuration */
    NULL,                                   /* init main configuration */

    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */

    ngx_http_inner_proxy_create_conf,         /* create location configuration */
    ngx_http_inner_proxy_merge_conf           /* merge location configuration */
};


ngx_module_t  ngx_http_inner_proxy_module = {
    NGX_MODULE_V1,
    &ngx_http_inner_proxy_module_ctx,         /* module context */
    ngx_http_inner_proxy_commands,            /* module directives */
    NGX_HTTP_MODULE,                        /* module type */
    NULL,                                   /* init master */
    NULL,                                   /* init module */
    NULL,                                   /* init process */
    NULL,                                   /* init thread */
    NULL,                                   /* exit thread */
    NULL,                                   /* exit process */
    NULL,                                   /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;


static void *
ngx_http_inner_proxy_create_conf(ngx_conf_t *cf)
{
    ngx_http_inner_proxy_conf_t    *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_inner_proxy_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    return conf;
}

static char *
ngx_http_inner_proxy_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_inner_proxy_conf_t    *prev = parent;
    ngx_http_inner_proxy_conf_t    *conf = child;

    ngx_conf_merge_str_value(conf->multiport, prev->multiport, "");
    ngx_conf_merge_str_value(conf->uri, prev->uri, "");

    return NGX_CONF_OK;
}

static char *
ngx_http_inner_proxy(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_inner_proxy_conf_t    *hipcf;
    ngx_str_t                      *value;

    hipcf = conf;

    if (hipcf->multiport.data != NULL) {
        return "is duplicate";
    }

    value = cf->args->elts;

    hipcf->multiport = value[1];
    hipcf->uri = value[2];

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_inner_proxy_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_http_inner_proxy_conf_t    *hipcf;
    ngx_http_inner_proxy_ctx_t     *ctx;
    ngx_http_request_t             *sr;
    ngx_int_t                       rc;
    ngx_str_t                       uri;
    ngx_buf_t                      *b;
    ngx_chain_t                     cl;

    ctx = ngx_http_get_module_ctx(r->main, ngx_http_inner_proxy_module);

    if (ctx == NULL) { /* not configured */
        return ngx_http_next_body_filter(r, in);
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
            "inner proxy body filter, r:%p r->main:%p", r, r->main);

    if (r != r->main) { /* send subrequest */
        return ngx_http_next_body_filter(r, in);
    }

    if (ctx->sent == 0) {
        hipcf = ngx_http_get_module_loc_conf(r, ngx_http_inner_proxy_module);

        uri.len = hipcf->uri.len + 1 + ctx->port.len + 2 + r->uri.len;
        uri.data = ngx_pcalloc(r->pool, uri.len);
        ngx_snprintf(uri.data, uri.len, "%V/%V:/%V",
                &hipcf->uri, &ctx->port, &r->uri);

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                "inner proxy send request to %V", &ctx->port);
        rc = ngx_http_subrequest(r, &uri, &r->args, &sr, NULL, 0);
        sr->method = r->method;
        sr->method_name = r->method_name;

        ctx->sent = 1;

        return rc;
    }

    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));

    if (b == NULL) {
        return NGX_ERROR;
    }

    b->last_buf = 1;
    cl.buf = b;
    cl.next = NULL;

    return ngx_http_next_body_filter(r, &cl);
}


static ngx_int_t
ngx_http_inner_proxy_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_inner_proxy_body_filter;

    return NGX_OK;
}


ngx_int_t
ngx_http_inner_proxy_request(ngx_http_request_t *r, ngx_uint_t wpid)
{
    ngx_http_inner_proxy_conf_t    *hipcf;
    ngx_http_inner_proxy_ctx_t     *ctx;

    hipcf = ngx_http_get_module_loc_conf(r, ngx_http_inner_proxy_module);

    if (hipcf == NULL || hipcf->multiport.len == 0) { /* not configured */
        return NGX_DECLINED;
    }

    if (wpid == ngx_worker) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                "inner proxy send request to self: %i", ngx_worker);
        return NGX_DECLINED;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_inner_proxy_module);
    if (ctx) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "inner proxy has been called in this request");
        return NGX_ERROR;
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_inner_proxy_ctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }
    ngx_http_set_ctx(r, ctx, ngx_http_inner_proxy_module);

    if (ngx_multiport_get_port(r->pool, &ctx->port, &hipcf->multiport,
                               ngx_multiport_get_slot(wpid)) == NGX_ERROR)
    {
        return NGX_ERROR;
    }

    r->headers_out.status = NGX_HTTP_OK;
    ngx_http_send_header(r);

    return ngx_http_output_filter(r, NULL);
}

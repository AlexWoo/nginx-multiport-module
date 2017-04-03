/*
 * Copyright (C) AlexWoo(Wu Jie) wj19840501@gmail.com
 */


#include <ngx_config.h>
#include <ngx_core.h>


static ngx_int_t ngx_stream_zone_init_process(ngx_cycle_t *cycle);
static void ngx_stream_zone_exit_process(ngx_cycle_t *cycle);
static void *ngx_stream_zone_create_conf(ngx_cycle_t *cf);
static char *ngx_stream_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


#define NAME_LEN    256

static ngx_str_t stream_zone_key = ngx_string("stream_zone");

typedef struct {
    u_char                              name[NAME_LEN];
    ngx_int_t                           slot; /* process slot */
    ngx_int_t                           idx;
    ngx_int_t                           next; /* idx of stream node */
} ngx_stream_zone_node_t;

typedef struct {
    ngx_atomic_t                        lock; /* mutex lock */
    ngx_int_t                           node; /* idx of stream node */
} ngx_stream_zone_hash_t;

typedef struct {
    ngx_int_t                           nbuckets;
    ngx_int_t                           nstreams;

    ngx_shm_zone_t                     *shm_zone;
    ngx_stream_zone_hash_t             *hash;
    ngx_stream_zone_node_t             *stream_node;
    ngx_int_t                          *free_node;
} ngx_stream_zone_conf_t;


static ngx_command_t  ngx_stream_zone_commands[] = {

    { ngx_string("rtmp_stream_zone"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE2,
      ngx_stream_zone,
      0,
      0,
      NULL },

      ngx_null_command
};


static ngx_core_module_t  ngx_stream_zone_module_ctx = {
    ngx_string("rtmp_stream_zone"),
    ngx_stream_zone_create_conf,            /* create conf */
    NULL                                    /* init conf */
};


ngx_module_t  ngx_stream_zone_module = {
    NGX_MODULE_V1,
    &ngx_stream_zone_module_ctx,            /* module context */
    ngx_stream_zone_commands,               /* module directives */
    NGX_CORE_MODULE,                        /* module type */
    NULL,                                   /* init master */
    NULL,                                   /* init module */
    ngx_stream_zone_init_process,           /* init process */
    NULL,                                   /* init thread */
    NULL,                                   /* exit thread */
    ngx_stream_zone_exit_process,           /* exit process */
    NULL,                                   /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_stream_zone_node_t *
ngx_stream_zone_get_node(ngx_str_t *name, ngx_int_t pslot)
{
    ngx_stream_zone_conf_t             *szcf;
    ngx_stream_zone_node_t             *node;

    szcf = (ngx_stream_zone_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                      ngx_stream_zone_module);

    if (*szcf->free_node == -1) {
        return NULL;
    }

    node = &szcf->stream_node[*szcf->free_node];
    *szcf->free_node = node->next;

    *ngx_copy(node->name, name->data, ngx_min(NAME_LEN, name->len)) = '\0';
    node->slot = pslot;
    node->next = -1;

    return node;
}

static void
ngx_stream_zone_put_node(ngx_int_t idx)
{
    ngx_stream_zone_conf_t             *szcf;
    ngx_stream_zone_node_t             *node;

    szcf = (ngx_stream_zone_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                      ngx_stream_zone_module);

    node = &szcf->stream_node[idx];

    node->next = *szcf->free_node;
    *szcf->free_node = idx;
}

static void *
ngx_stream_zone_create_conf(ngx_cycle_t *cf)
{
    ngx_stream_zone_conf_t             *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_stream_zone_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->nbuckets = NGX_CONF_UNSET;
    conf->nstreams = NGX_CONF_UNSET;

    return conf;
}

static void
ngx_stream_zone_clear(ngx_cycle_t *cycle)
{
    ngx_stream_zone_conf_t             *szcf;
    ngx_int_t                           idx, cur, next;

    szcf = (ngx_stream_zone_conf_t *) ngx_get_conf(cycle->conf_ctx,
                                      ngx_stream_zone_module);

    if (szcf->nbuckets <= 0 || szcf->nstreams <= 0) {
        return;
    }

    for (idx = 0; idx < szcf->nbuckets; ++idx) {

        ngx_spinlock(&szcf->hash[idx].lock, 1, 2048);
        cur = -1;

        while (1) {
            if (cur == -1) {
                next = szcf->hash[idx].node;
            } else {
                next = szcf->stream_node[cur].next;
            }

            if (next == -1) {
                break;
            }

            if (szcf->stream_node[next].slot == ngx_process_slot) {
                if (cur == -1) {
                    szcf->hash[idx].node = szcf->stream_node[next].next;
                } else {
                    szcf->stream_node[cur].next = szcf->stream_node[next].next;
                }

                ngx_stream_zone_put_node(next);
                continue;
            }

            cur = next;
        }
        ngx_unlock(&szcf->hash[idx].lock);
    }
}

static ngx_int_t
ngx_stream_zone_init_process(ngx_cycle_t *cycle)
{
    ngx_stream_zone_clear(cycle);

    return NGX_OK;
}

static void
ngx_stream_zone_exit_process(ngx_cycle_t *cycle)
{
    ngx_stream_zone_clear(cycle);
}

static void
ngx_stream_zone_shm_init(ngx_shm_t *shm, ngx_stream_zone_conf_t *szcf)
{
    u_char                             *p;
    ngx_int_t                           i, next;

    p = shm->addr;

    szcf->hash = (ngx_stream_zone_hash_t *) p;
    p += sizeof(ngx_stream_zone_hash_t) * szcf->nbuckets;

    szcf->stream_node = (ngx_stream_zone_node_t *) p;
    p += sizeof(ngx_stream_zone_node_t) * szcf->nstreams;

    szcf->free_node = (ngx_int_t *) p;

    /* init shm zone */
    for (i = 0; i < szcf->nbuckets; ++i) {
        szcf->hash[i].node = -1;
    }

    next = -1;
    i = szcf->nstreams;

    do {
        --i;

        szcf->stream_node[i].slot = -1;
        szcf->stream_node[i].idx = i;
        szcf->stream_node[i].next = next;
        next = i;
    } while (i);

    *szcf->free_node = i;
}

static char *
ngx_stream_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_uint_t                          i;
    ngx_str_t                          *value;
    size_t                              len;
    ngx_shm_t                           shm;
    ngx_stream_zone_conf_t             *szcf = conf;

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; ++i) {

        if (ngx_strncmp(value[i].data, "buckets=", 8) == 0) {
            szcf->nbuckets = ngx_atoi(value[i].data + 8, value[i].len - 8);

            if (szcf->nbuckets <= 0) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                        "invalid buckets \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "streams=", 8) == 0) {
            szcf->nstreams = ngx_atoi(value[i].data + 8, value[i].len - 8);

            if (szcf->nstreams <= 0) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                        "invalid streams \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    /* create shm zone */
    len = sizeof(ngx_stream_zone_hash_t) * szcf->nbuckets
        + sizeof(ngx_stream_zone_node_t) * szcf->nstreams
        + sizeof(ngx_int_t);

    shm.size = len;
    shm.name = stream_zone_key;
    shm.log = cf->cycle->log;

    if (ngx_shm_alloc(&shm) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    ngx_stream_zone_shm_init(&shm, szcf);

    return NGX_CONF_OK;
}


ngx_int_t
ngx_stream_zone_insert_stream(ngx_str_t *name)
{
    ngx_stream_zone_conf_t             *szcf;
    ngx_uint_t                          idx;
    ngx_int_t                           i, pslot;
    ngx_stream_zone_node_t             *node;

    szcf = (ngx_stream_zone_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                      ngx_stream_zone_module);

    if (szcf->nbuckets <= 0 || szcf->nstreams <= 0) {
        return NGX_ERROR;
    }

    idx = ngx_hash_key(name->data, name->len) % szcf->nbuckets;

    ngx_spinlock(&szcf->hash[idx].lock, 1, 2048);
    i = szcf->hash[idx].node;
    pslot = -1;
    while (i != -1) {
        if (ngx_strncmp(szcf->stream_node[i].name, name->data, name->len)
                == 0)
        {
            pslot = szcf->stream_node[i].slot;
            break;
        }

        i = szcf->stream_node[i].next;
    }

    if (i == -1) { /* stream not in hash */
        node = ngx_stream_zone_get_node(name, ngx_process_slot);
        if (node == NULL) {
            ngx_unlock(&szcf->hash[idx].lock);
            return NGX_ERROR;
        }
        node->slot = ngx_process_slot;

        node->next = szcf->hash[idx].node;
        szcf->hash[idx].node = node->idx;

        pslot = ngx_process_slot;
    }
    ngx_unlock(&szcf->hash[idx].lock);

    return pslot;
}

void
ngx_stream_zone_delete_stream(ngx_str_t *name)
{
    ngx_stream_zone_conf_t             *szcf;
    ngx_uint_t                          idx;
    ngx_int_t                           cur, next;

    szcf = (ngx_stream_zone_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                      ngx_stream_zone_module);

    if (szcf->nbuckets <= 0 || szcf->nstreams <= 0) {
        return;
    }

    idx = ngx_hash_key(name->data, name->len) % szcf->nbuckets;

    ngx_spinlock(&szcf->hash[idx].lock, 1, 2048);
    cur = -1;
    next = szcf->hash[idx].node;
    while (next != -1) {
        if (ngx_strncmp(szcf->stream_node[next].name, name->data, name->len)
                == 0)
        {
            if (cur == -1) { /* link header */
                szcf->hash[idx].node = szcf->stream_node[next].next;
            } else {
                szcf->stream_node[cur].next = szcf->stream_node[next].next;
            }
            ngx_stream_zone_put_node(next);
            break;
        }

        cur = next;
        next = szcf->stream_node[next].next;
    }
    ngx_unlock(&szcf->hash[idx].lock);
}

void ngx_stream_zone_print()
{
#if (NGX_DEBUG)
    ngx_stream_zone_conf_t             *szcf;
    ngx_int_t                           idx, i;

    szcf = (ngx_stream_zone_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                      ngx_stream_zone_module);

    ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "!!!!!!! hash table");
    for (idx = 0; idx < szcf->nbuckets; ++idx) {
        if (szcf->hash[idx].node != -1) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0,
                    "  !!!!!!! idx %i", idx);
            i = szcf->hash[idx].node;
            while (i != -1) {
                ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0,
                        "    [%i]%s: %i %i", szcf->stream_node[i].idx,
                        szcf->stream_node[i].name, szcf->stream_node[i].slot,
                        szcf->stream_node[i].next);
                i = szcf->stream_node[i].next;
            }
        }
    }

    ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "!!!!!!! free node");
    i = *szcf->free_node;
    while (i != -1) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "    %i: %i",
                szcf->stream_node[i].idx, szcf->stream_node[i].next);
        i = szcf->stream_node[i].next;
    }
#endif
}


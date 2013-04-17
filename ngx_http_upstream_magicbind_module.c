/*
 * Copyright (C) 2013 Grzegorz Łyczba <grzegorz.lyczba@gmail.com>
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
        ngx_http_upstream_init_pt          original_init_upstream;
        ngx_http_upstream_init_peer_pt     original_init_peer;
        ngx_addr_t                          *addrs;
        ngx_uint_t                          addrslen;
        ngx_uint_t                          lastused;
} ngx_http_upstream_magicbind_srv_conf_t;

static char * ngx_http_upstream_magicbind(ngx_conf_t *, ngx_command_t *, void *);
static ngx_int_t ngx_http_upstream_init_magicbind(ngx_conf_t *, ngx_http_upstream_srv_conf_t *);
static void * ngx_http_upstream_magicbind_create_conf(ngx_conf_t *);

static ngx_command_t  ngx_http_upstream_magicbind_commands[] = {
        {
                ngx_string("magicbind"),
                NGX_HTTP_UPS_CONF|NGX_CONF_1MORE,
                ngx_http_upstream_magicbind,
                0,
                0,
                NULL
        },
        ngx_null_command
};



static void *
ngx_http_upstream_magicbind_create_conf(ngx_conf_t *cf)
{
        ngx_http_upstream_magicbind_srv_conf_t  *conf;

        conf = ngx_pcalloc(cf->pool,
                        sizeof(ngx_http_upstream_magicbind_srv_conf_t));
        if (conf == NULL) {
                return NULL;
        }

        conf->addrs = NULL;
        conf->lastused = 0;

        return conf;
}


static ngx_http_module_t
ngx_http_upstream_magicbind_module_ctx = {
        NULL,                                  /* preconfiguration */
        NULL,                                  /* postconfiguration */

        NULL,                                  /* create main configuration */
        NULL,                                  /* init main configuration */

        ngx_http_upstream_magicbind_create_conf, /* create server configuration */
        NULL,                                  /* merge server configuration */

        NULL,                                  /* create location configuration */
        NULL                                   /* merge location configuration */
};


ngx_module_t
ngx_http_upstream_magicbind_module = {
        NGX_MODULE_V1,
        &ngx_http_upstream_magicbind_module_ctx, /* module context */
        ngx_http_upstream_magicbind_commands,    /* module directives */
        NGX_HTTP_MODULE,                       /* module type */
        NULL,                                  /* init master */
        NULL,                                  /* init module */
        NULL,                                  /* init process */
        NULL,                                  /* init thread */
        NULL,                                  /* exit thread */
        NULL,                                  /* exit process */
        NULL,                                  /* exit master */
        NGX_MODULE_V1_PADDING
};

static char *
ngx_http_upstream_magicbind(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
        ngx_http_upstream_srv_conf_t            *uscf;
        ngx_http_upstream_magicbind_srv_conf_t  *mbcf;
        ngx_uint_t n, ret;
        ngx_str_t        *value;

        uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);
        mbcf = ngx_http_conf_upstream_srv_conf(uscf,
                        ngx_http_upstream_magicbind_module);

        mbcf->original_init_upstream = uscf->peer.init_upstream
                ? uscf->peer.init_upstream
                : ngx_http_upstream_init_round_robin;

        value = cf->args->elts;
        ngx_addr_t *addrs = ngx_palloc(cf->pool, (cf->args->nelts - 1) * sizeof(ngx_addr_t));
        for (n = 1; n < cf->args->nelts; n++) {
                ret = ngx_parse_addr(cf->pool, &addrs[n-1], value[n].data, value[n].len);
                if (ret != NGX_OK) {
                        ngx_log_error(NGX_LOG_CRIT, cf->log, 0, "Unable to \
parse IP address \"%V\"", &value[n]);
                        return NGX_CONF_ERROR;
                }
        }
        mbcf->addrs = addrs;
        mbcf->addrslen = cf->args->nelts - 1;

        uscf->peer.init_upstream = ngx_http_upstream_init_magicbind;
        return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_upstream_init_magicbind_peer(ngx_http_request_t *r,
                ngx_http_upstream_srv_conf_t *us)
{

        ngx_http_upstream_magicbind_srv_conf_t  *mbcf;

        mbcf = ngx_http_conf_upstream_srv_conf(us,
                        ngx_http_upstream_magicbind_module);
        r->upstream->peer.local = &mbcf->addrs[mbcf->lastused++];
        if (mbcf->lastused == mbcf->addrslen) {
                mbcf->lastused = 0;
        }

        return mbcf->original_init_peer(r, us);
}


static ngx_int_t
ngx_http_upstream_init_magicbind(ngx_conf_t *cf,
                ngx_http_upstream_srv_conf_t *us)
{
        ngx_http_upstream_magicbind_srv_conf_t  *mbcf;

        mbcf = ngx_http_conf_upstream_srv_conf(us, ngx_http_upstream_magicbind_module);
        if (mbcf->original_init_upstream(cf, us) != NGX_OK) {
                return NGX_ERROR;
        }

        mbcf->original_init_peer = us->peer.init;
        us->peer.init = ngx_http_upstream_init_magicbind_peer;
        return NGX_OK;
}

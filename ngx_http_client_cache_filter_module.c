#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_http_client_cache_filter_module.h>
#include <db.h>

static char* ngx_http_client_cache(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_client_cache_filter_init(ngx_conf_t *cf);
static void* ngx_http_client_cache_filter_create_loc_conf(ngx_conf_t *cf);
static char* ngx_http_client_cache_filter_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_client_cache_filter_init_process(ngx_cycle_t *c);
static void ngx_http_client_cache_filter_exit_process(ngx_cycle_t *c);

static ngx_command_t ngx_http_client_cache_filter_command[] = {
    { ngx_string("client_cache"),
      NGX_HTTP_LOC_CONF | NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_CONF_TAKE1,
      ngx_http_client_cache,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_client_cache_filter_loc_conf_t, path),
      NULL},
    ngx_null_command
};

static ngx_http_module_t ngx_http_client_cache_filter_ctx = {
    NULL,                          /* preconfiguration */
    ngx_http_client_cache_filter_init,                          /* postconfiguration */

    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */

    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */

    ngx_http_client_cache_filter_create_loc_conf,                          /* create location configuration */
    ngx_http_client_cache_filter_merge_loc_conf                           /* merge location configuration */
};

ngx_module_t ngx_http_client_cache_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_client_cache_filter_ctx,
    ngx_http_client_cache_filter_command,
    NGX_HTTP_MODULE,
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    ngx_http_client_cache_filter_init_process,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    ngx_http_client_cache_filter_exit_process,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;

static ngx_int_t ngx_http_set_expire(ngx_http_request_t *r, time_t e)
{
    size_t            len;
    time_t            now, expires_time, max_age;
    ngx_uint_t        i;
    ngx_table_elt_t  *expires, *cc, **ccp;

    expires = r->headers_out.expires;

    if (expires == NULL) {

        expires = ngx_list_push(&r->headers_out.headers);
        if (expires == NULL) {
            return NGX_ERROR;
        }

        r->headers_out.expires = expires;

        expires->hash = 1;
        expires->key.len = sizeof("Expires") - 1;
        expires->key.data = (u_char *) "Expires";
    }

    len = sizeof("Mon, 28 Sep 1970 06:00:00 GMT");
    expires->value.len = len - 1;

    ccp = r->headers_out.cache_control.elts;

    if (ccp == NULL) {

        if (ngx_array_init(&r->headers_out.cache_control, r->pool,
                           1, sizeof(ngx_table_elt_t *))
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        ccp = ngx_array_push(&r->headers_out.cache_control);
        if (ccp == NULL) {
            return NGX_ERROR;
        }

        cc = ngx_list_push(&r->headers_out.headers);
        if (cc == NULL) {
            return NGX_ERROR;
        }

        cc->hash = 1;
        cc->key.len = sizeof("Cache-Control") - 1;
        cc->key.data = (u_char *) "Cache-Control";

        *ccp = cc;

    } else {
        for (i = 1; i < r->headers_out.cache_control.nelts; i++) {
            ccp[i]->hash = 0;
        }

        cc = ccp[0];
    }

    expires->value.data = ngx_palloc(r->pool, len);
    if (expires->value.data == NULL) {
        return NGX_ERROR;
    }
    now = ngx_time();
    expires_time = now + e;
    ngx_http_time(expires->value.data, now + e);

    cc->value.data = ngx_palloc(r->pool,
                                sizeof("max-age=") + NGX_TIME_T_LEN + 1);
    if (cc->value.data == NULL) {
        return NGX_ERROR;
    }
    max_age = e;
    cc->value.len = ngx_sprintf(cc->value.data, "max-age=%T", max_age)
        - cc->value.data;

    return NGX_DECLINED;
}

static ngx_int_t
ngx_http_client_cache_header_filter(ngx_http_request_t *r)
{
    ngx_int_t rc;
    ngx_http_client_cache_filter_loc_conf_t *lcf;
    time_t expire = 0;

    if (r != r->main) {
        return ngx_http_next_header_filter(r);
    }

    lcf = ngx_http_get_module_loc_conf(r, ngx_http_client_cache_filter_module);

    if (!lcf->client_cache) {
        return ngx_http_next_header_filter(r);
    }

    if (!(r->method &(NGX_HTTP_GET))) {
        return ngx_http_next_header_filter(r);
    }

    rc = lcf->methods.get_expire_pair(r, lcf, &expire);

    if (rc == NGX_OK) {
        rc = ngx_http_set_expire(r, expire);
    }

    return ngx_http_next_header_filter(r);
}



static char*
ngx_http_client_cache(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_client_cache_filter_loc_conf_t *lcf = conf;
    ngx_str_t *value;

    if (lcf->client_cache == 1) {
        return "is duplicate";
    }

    lcf->client_cache = 1;

    if (cf->args->nelts != 2) {
        return "invalid parameters";
    }

    value = cf->args->elts;

    ngx_http_client_cache_source_t *source;
    for (source = client_cache_sources; source->type != NULL; source++) {
        if (!ngx_strncmp(value[1].data, source->prefix, strlen(source->prefix))) {
            lcf->path.data = value[1].data + strlen(source->prefix);
            lcf->path.len = value[1].len - strlen(source->prefix);
            lcf->methods = source->methods;
            break;
        }
    }

    if (lcf->path.len == 0) {
        return "not supported data source";
    }

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_client_cache_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_client_cache_header_filter;

    return NGX_OK;
}

static void*
ngx_http_client_cache_filter_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_client_cache_filter_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_client_cache_filter_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    conf->client_cache = 0;

    return conf;
}

static char*
ngx_http_client_cache_filter_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_client_cache_filter_loc_conf_t *prev = parent;
    ngx_http_client_cache_filter_loc_conf_t *conf = child;

    if (prev->client_cache) {
        conf->client_cache = prev->client_cache;
    }

    ngx_conf_merge_str_value(prev->path, conf->path, "");

    return NGX_CONF_OK;
}

static ngx_int_t
init_client_cache_locations(ngx_cycle_t *c, ngx_array_t *locations)
{
    ngx_int_t ret = NGX_OK;
    ngx_http_core_loc_conf_t **clcfp;
    ngx_http_client_cache_filter_loc_conf_t *llcf;
    ngx_uint_t i;

    clcfp = locations->elts;

    for(i = 0; i < locations->nelts; i++) {
        llcf = clcfp[i]->loc_conf[ngx_http_client_cache_filter_module.ctx_index];
        if (llcf->client_cache != 0) {
            ret = llcf->methods.init_process(c, llcf);
            if (ret != NGX_OK) {
                ngx_log_error(NGX_LOG_ERR, c->log, 0,
                              "Initilaize client cache module failed"
                              " when starting process.");
                return ret;
            }
        }

        if (clcfp[i]->locations == NULL) {
            continue;
        }

        ret = init_client_cache_locations(c, clcfp[i]->locations);
        if (ret != NGX_OK) {
            return ret;
        }
    }

    return ret;
}

static ngx_int_t
ngx_http_client_cache_filter_init_process(ngx_cycle_t *c)
{
    ngx_int_t ret = NGX_OK;
    ngx_http_conf_ctx_t *ctx;
    ngx_http_core_main_conf_t *cmcf;
    ngx_http_core_srv_conf_t **cscfp;
    ngx_uint_t i;

    ctx = (ngx_http_conf_ctx_t *)ngx_get_conf(c->conf_ctx, ngx_http_module);

    cmcf = ctx->main_conf[ngx_http_core_module.ctx_index];
    cscfp = cmcf->servers.elts;

    for(i = 0; i< cmcf->servers.nelts; i++) {
        ret = init_client_cache_locations(c, &(cscfp[i]->locations));
        if (ret != NGX_OK) {
            return ret;
        }
    }

    return ret;
}

static void
exit_client_cache_locations(ngx_cycle_t *c, ngx_array_t *locations)
{
    ngx_int_t ret = NGX_OK;
    ngx_http_core_loc_conf_t **clcfp;
    ngx_http_client_cache_filter_loc_conf_t *llcf;
    ngx_uint_t i;

    clcfp = locations->elts;

    for(i = 0; i < locations->nelts; i++) {
        llcf = clcfp[i]->loc_conf[ngx_http_client_cache_filter_module.ctx_index];
        if (llcf->client_cache != 0) {
            ret = llcf->methods.exit_process(c, llcf);
            if (ret != NGX_OK) {
                ngx_log_error(NGX_LOG_ERR, c->log, 0,
                              "Clear client cache module failed"
                              " when exit process.");
            }
        }

        if (clcfp[i]->locations == NULL) {
            continue;
        }

        ret = init_client_cache_locations(c, clcfp[i]->locations);
    }

    return;
}

static void
ngx_http_client_cache_filter_exit_process(ngx_cycle_t *c)
{
    ngx_http_conf_ctx_t *ctx;
    ngx_http_core_main_conf_t *cmcf;
    ngx_http_core_srv_conf_t **cscfp;
    ngx_uint_t i;

    ctx = (ngx_http_conf_ctx_t *)ngx_get_conf(c->conf_ctx, ngx_http_module);

    cmcf = ctx->main_conf[ngx_http_core_module.ctx_index];
    cscfp = cmcf->servers.elts;

    for(i = 0; i< cmcf->servers.nelts; i++) {
        exit_client_cache_locations(c, &(cscfp[i]->locations));
    }

    return;
}

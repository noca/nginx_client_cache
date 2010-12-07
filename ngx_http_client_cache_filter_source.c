#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_http_client_cache_filter_module.h>
#include <db.h>

static ngx_int_t
ngx_http_upstream_init_process(ngx_cycle_t *c, ngx_http_client_cache_filter_loc_conf_t *cf)
{
    return NGX_OK;
}

static ngx_int_t
ngx_http_upstream_exit_process(ngx_cycle_t *c, ngx_http_client_cache_filter_loc_conf_t *cf)
{
    return NGX_OK;
}

static ngx_int_t
ngx_http_upstream_get_expire_pair(ngx_http_request_t *r, ngx_http_client_cache_filter_loc_conf_t *lcf, time_t *e)
{
    return NGX_OK;
}

typedef struct {
    DB *db;
} ngx_http_client_cache_filter_bdb_ctx;

static ngx_int_t
ngx_http_bdb_init_process(ngx_cycle_t *c, ngx_http_client_cache_filter_loc_conf_t *lcf)
{
    DB *dbp;
    ngx_uint_t open_flags;
    ngx_int_t ret;
    ngx_http_client_cache_filter_bdb_ctx *ctx;

    if (lcf->ctx == NULL) {
        lcf->ctx = ngx_palloc(c->pool, sizeof(ngx_http_client_cache_filter_bdb_ctx));
        if(lcf->ctx == NULL) {
            return NGX_ERROR;
        }

    }

    ctx = lcf->ctx;

    if (!ngx_strcmp(lcf->path.data, "")) {
        return NGX_OK;
    }

    ret = db_create(&dbp, NULL, 0);
    if (ret) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                          "Create database handler failed. %s",
                      db_strerror(ret));
        return NGX_ERROR;
    }

    ret = dbp->set_cachesize(dbp,
                             0,
                             lcf->cache_size,
                             0);

    if (ret) {
        ngx_log_error(NGX_LOG_ERR, c->log, 0,
                      "Set cache size to %u(KB) failed. %s",
                      lcf->cache_size, db_strerror(ret));
        return NGX_ERROR;
    } else {
        ngx_log_error(NGX_LOG_INFO, c->log, 0,
                      "Set cache size to %u(KB) successfully.",
                      lcf->cache_size);
    }

    open_flags = DB_RDONLY;
    ret = dbp->open(dbp,
                    NULL,
                    (char *)lcf->path.data,
                    NULL,
                    DB_BTREE,
                    open_flags,
                    0);
    if (ret) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "Open database '%s' failed. %s",
                      lcf->path.data, db_strerror(ret));
        return NGX_ERROR;
    }

    ctx->db = dbp;
    return NGX_OK;
}

static ngx_int_t
ngx_http_bdb_exit_process(ngx_cycle_t *c, ngx_http_client_cache_filter_loc_conf_t *lcf)
{
    ngx_int_t ret = NGX_OK;
    DB *dbp;

    if (lcf->ctx == NULL) {
        return NGX_OK;
    }

    dbp = ((ngx_http_client_cache_filter_bdb_ctx *)lcf->ctx)->db;


    if (dbp != NULL) {
        ret = dbp->close(dbp, 0);
        if (ret) {
            ngx_log_error(NGX_LOG_ERR, c->log, 0,
                          "Database '%s' close failed. %s",
                          lcf->path.data, db_strerror(ret));
        }
    }


    return ret;
}

static ngx_int_t
ngx_http_bdb_get_expire_pair(ngx_http_request_t *r, ngx_http_client_cache_filter_loc_conf_t *lcf, time_t *e)
{
    ngx_int_t ret, err = NGX_OK;
    DB *dbp;
    DBT key, value;
    u_char *iter;
    ngx_uint_t cache_time;
    ngx_connection_t *c;

    if (lcf->ctx == NULL) {
        return NGX_OK;
    }

    c = r->connection;
    dbp = ((ngx_http_client_cache_filter_bdb_ctx *)lcf->ctx)->db;

    if (dbp == NULL) {
        *e = 0;
        return NGX_OK;
    }


    memset(&key, 0, sizeof(DBT));
    memset(&value, 0, sizeof(DBT));
    key.data = r->uri.data;
    value.data = &cache_time;
    value.ulen = sizeof(cache_time);
    value.flags = DB_DBT_USERMEM;

    key.size = r->uri.len;
    iter = r->uri.data + r->uri.len - 1;
    if (lcf->match_type == NGX_HTTP_CLIENT_CACHE_FILTER_MATCH_TYPE_PREFIX) {
        for(;;) {
            ret = dbp->get(dbp, NULL, &key, &value, 0);
            if(ret == DB_NOTFOUND) {
                /* iter reach the start of the uri */
                if (iter == r->uri.data) {
                    ngx_log_error(NGX_LOG_INFO, c->log, 0,
                              "No corresponding cache time for"
                                  " uri '%s'.", r->uri.data);
                    err = NGX_ERROR;
                    break;
                }
                /* find the prefix split by the '/' */
                for(;;) {
                    iter--;
                    key.size--;
                    if (*iter == '/' || iter == r->uri.data) {
                    break;
                    }
                }
            } else if (ret) {
                ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                              "Get cache time for %s failed. %s",
                              r->uri.data, db_strerror(ret));
                break;
                err = NGX_ERROR;
            } else {
                *e = cache_time;
                err = NGX_OK;
                break;
            }
        }
    } else {
        ret = dbp->get(dbp, NULL, &key, &value, 0);
        if(ret == DB_NOTFOUND) {
            ngx_log_error(NGX_LOG_INFO, c->log, 0,
                          "No corresponding cache time for"
                          " uri '%s'.", r->uri.data);
            err = NGX_ERROR;
        } else if (ret) {
            ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                          "Get cache time for %s failed. %s",
                          r->uri.data, db_strerror(ret));
            err = NGX_ERROR;
        } else {
            *e = cache_time;
            err = NGX_OK;
        }
    }

    return err;
}

ngx_http_client_cache_source_t client_cache_sources[] = {
    {"bdb",
     "bdb:",
     {ngx_http_bdb_init_process,
      ngx_http_bdb_exit_process,
      ngx_http_bdb_get_expire_pair
     }
    },
    {"upstream",
     "upstream:",
     {ngx_http_upstream_init_process,
      ngx_http_upstream_exit_process,
      ngx_http_upstream_get_expire_pair
     }
    },
    {NULL, NULL, {NULL, NULL, NULL}}
};

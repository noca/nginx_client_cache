#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
typedef struct ngx_http_client_cache_filter_loc_conf_s ngx_http_client_cache_filter_loc_conf_t;

typedef struct ngx_http_client_cache_filter_methods_s {
    ngx_int_t (*init_process)(ngx_cycle_t *, ngx_http_client_cache_filter_loc_conf_t *);
    ngx_int_t (*exit_process)(ngx_cycle_t *, ngx_http_client_cache_filter_loc_conf_t *);
    ngx_int_t (*get_expire_pair)(ngx_http_request_t *, ngx_http_client_cache_filter_loc_conf_t *, time_t *);
} ngx_http_client_cache_filter_methods_t;

struct ngx_http_client_cache_filter_loc_conf_s{
    ngx_uint_t client_cache;
    ngx_str_t path;
    void *ctx;
    ngx_http_client_cache_filter_methods_t methods;
};

typedef struct {
    char *type;
    char *prefix;
    ngx_http_client_cache_filter_methods_t methods;
}ngx_http_client_cache_source_t;

extern ngx_http_client_cache_source_t client_cache_sources[];

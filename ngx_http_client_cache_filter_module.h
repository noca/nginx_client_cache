#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
typedef struct ngx_http_client_cache_filter_loc_conf_s ngx_http_client_cache_filter_loc_conf_t;

struct ngx_http_client_cache_filter_loc_conf_s{
    ngx_uint_t client_cache;
    ngx_str_t path;
    ngx_int_t (*get_expire_pair)(ngx_http_request_t *, ngx_http_client_cache_filter_loc_conf_t *, time_t *);
};


typedef struct {
    char *type;
    char *prefix;
    ngx_int_t (*get_expire_pair)(ngx_http_request_t *, ngx_http_client_cache_filter_loc_conf_t *, time_t *);
}ngx_http_client_cache_source_t;

extern ngx_http_client_cache_source_t client_cache_sources[];

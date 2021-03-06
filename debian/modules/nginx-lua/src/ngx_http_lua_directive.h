/* vim:set ft=c ts=4 sw=4 et fdm=marker: */

#ifndef NGX_HTTP_LUA_DIRECTIVE_H
#define NGX_HTTP_LUA_DIRECTIVE_H

#include "ngx_http_lua_common.h"


extern unsigned  ngx_http_lua_requires_rewrite;
extern unsigned  ngx_http_lua_requires_access;
extern unsigned  ngx_http_lua_requires_header_filter;
extern unsigned  ngx_http_lua_requires_capture_filter;


char * ngx_http_lua_package_cpath(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);
char * ngx_http_lua_package_path(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);
char * ngx_http_lua_content_by_lua(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);
char * ngx_http_lua_rewrite_by_lua(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);
char * ngx_http_lua_access_by_lua(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);
char * ngx_http_lua_header_filter_by_lua(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);
char * ngx_http_lua_code_cache(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

#if defined(NDK) && NDK
char * ngx_http_lua_set_by_lua(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);
ngx_int_t ngx_http_lua_filter_set_by_lua_inline(ngx_http_request_t *r,
        ngx_str_t *val, ngx_http_variable_value_t *v, void *data);
ngx_int_t ngx_http_lua_filter_set_by_lua_file(ngx_http_request_t *r,
        ngx_str_t *val, ngx_http_variable_value_t *v, void *data);
#endif


#endif /* NGX_HTTP_LUA_DIRECTIVE_H */

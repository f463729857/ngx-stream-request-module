//
//  ngx_stream_variable_module.c
//  nginx1.10Xcode
//
//  Created by xpwu on 16/10/9.
//  Copyright © 2016年 xpwu. All rights reserved.
//

#include "ngx_stream_variable_module.h"
#include "ngx_str_str_rbtree.h"
#include "ngx_stream_util.h"

#ifdef this_module
#undef this_module
#endif
#define this_module ngx_stream_variable_module

static void *ngx_stream_variable_create_srv_conf(ngx_conf_t *cf);
static char *ngx_stream_variable_merge_srv_conf(ngx_conf_t *cf
                                              , void *parent
                                              , void *child);


typedef ngx_str_str_rbtree ngx_stream_variable_ctx_t;

typedef struct {
  ngx_array_t* var_conf;
  ngx_array_t* if_empty;
} ngx_stream_variable_srv_conf_t;

char* ngx_conf_var_post_handler (ngx_conf_t *cf, void *data, void *conf) {
  ngx_keyval_t* keyval = conf;
  ngx_str_t key = keyval->key;
  if (key.len <= 1 || key.data[0] != '$') {
    return "variable must be ahead of '$'";
  }
  
  ngx_memmove(key.data, key.data+1, key.len-1);
  keyval->key.len -= 1;
  
  return NGX_CONF_OK;
}

static ngx_conf_post_t conf_post = {ngx_conf_var_post_handler};

static ngx_command_t  ngx_stream_variable_commands[] = {
  { ngx_string("set"),
    NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_TAKE2,
    ngx_conf_set_keyval_slot,
    NGX_STREAM_SRV_CONF_OFFSET,
    offsetof(ngx_stream_variable_srv_conf_t, var_conf),
    &conf_post },
  
  { ngx_string("set_if_empty"),
    NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_TAKE2,
    ngx_conf_set_keyval_slot,
    NGX_STREAM_SRV_CONF_OFFSET,
    offsetof(ngx_stream_variable_srv_conf_t, if_empty),
    &conf_post },
  
  ngx_null_command
};

static ngx_stream_module_t  ngx_stream_variable_module_ctx = {
  NULL,                                  /* postconfiguration */
  
  NULL,                               /* create main configuration */
  NULL,                                  /* init main configuration */
  
  ngx_stream_variable_create_srv_conf,   /* create server configuration */
  ngx_stream_variable_merge_srv_conf     /* merge server configuration */
};

ngx_module_t  ngx_stream_variable_module = {
  NGX_MODULE_V1,
  &ngx_stream_variable_module_ctx,           /* module context */
  ngx_stream_variable_commands,              /* module directives */
  NGX_STREAM_MODULE,                     /* module type */
  NULL,                                  /* init master */
  NULL,                                  /* init module */
  NULL,                                  /* init process */
  NULL,                                  /* init thread */
  NULL,                                  /* exit thread */
  NULL,                                  /* exit process */
  NULL,                                  /* exit master */
  NGX_MODULE_V1_PADDING
};

static void *ngx_stream_variable_create_srv_conf(ngx_conf_t *cf) {
  ngx_stream_variable_srv_conf_t  *wscf;
  
  wscf = ngx_pcalloc(cf->pool, sizeof(ngx_stream_variable_srv_conf_t));
  if (wscf == NULL) {
    return NULL;
  }
  
  /*
   * set by ngx_pcalloc():
   *
   *  wscf->var_conf = NULL;
   *  wscf->if_empty = NULL;
   *
   */
  
  return wscf;
}

static char *ngx_stream_variable_merge_srv_conf(ngx_conf_t *cf
                                                , void *parent
                                                , void *child) {
  ngx_stream_variable_srv_conf_t *prev = parent;
  ngx_stream_variable_srv_conf_t *conf = child;
  
  conf->var_conf = ngx_merge_key_val_array(cf->pool, prev->var_conf, conf->var_conf);
  conf->if_empty = ngx_merge_key_val_array(cf->pool, prev->if_empty, conf->if_empty);
  
  return NGX_CONF_OK;
}

static void add_conf_var_to_session(ngx_stream_session_t* s) {
  ngx_stream_variable_srv_conf_t* vscf;
  vscf = ngx_stream_get_module_srv_conf(s, this_module);
  
  if (vscf->var_conf != NULL) {
    ngx_keyval_t* keyval = vscf->var_conf->elts;
    ngx_uint_t n = vscf->var_conf->nelts;
    ngx_uint_t i = 0;
    for (i = 0; i < n; ++i) {
      ngx_stream_set_variable_value(s, keyval[i].key, keyval[i].value, 1);
    }
  }
  
  if (vscf->if_empty != NULL) {
    ngx_keyval_t* keyval = vscf->if_empty->elts;
    ngx_uint_t n = vscf->if_empty->nelts;
    ngx_uint_t i = 0;
    for (i = 0; i < n; ++i) {
      ngx_stream_set_variable_value(s, keyval[i].key, keyval[i].value, 0);
    }
  }
}

static ngx_stream_variable_ctx_t* get_ctx(ngx_stream_session_t* s) {
  ngx_stream_variable_ctx_t* variable = ngx_stream_get_module_ctx(s, this_module);
  if (variable == NULL) {
    variable = ngx_pcalloc(s->connection->pool, sizeof(ngx_stream_variable_ctx_t));
    ngx_str_str_rbtree_init(variable, s->connection->pool, s->connection->log);
    ngx_stream_set_ctx(s, variable, this_module);
    
    // must be after ngx_stream_set_ctx
    add_conf_var_to_session(s);
  }
  return variable;
}

extern ngx_str_t ngx_stream_get_variable_value(ngx_stream_session_t* s
                                               , ngx_str_t variable_name) {
  ngx_stream_variable_ctx_t* variable = get_ctx(s);
  
  return ngx_str_str_rbtree_get_value(variable, variable_name);

}

extern void ngx_stream_set_variable_value(ngx_stream_session_t* s
                                          , ngx_str_t variable_name
                                          , ngx_str_t variable_value
                                          , ngx_int_t force_rewrite) {
  ngx_stream_variable_ctx_t* variable = get_ctx(s);
  
  ngx_str_str_rbtree_set_value(variable, variable_name
                               , variable_value, force_rewrite);
}





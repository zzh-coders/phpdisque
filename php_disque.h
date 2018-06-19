/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2018 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_DISQUE_H
#define PHP_DISQUE_H

extern zend_module_entry disque_module_entry;
#define phpext_disque_ptr &disque_module_entry

#define PHP_DISQUE_VERSION "develop" /* Replace with version number for your extension */
#define _NL "\r\n"
#define UNSERIALIZE_NONE 0
#define UNSERIALIZE_KEYS 1
#define UNSERIALIZE_VALS 2
#define UNSERIALIZE_ALL  3

/* {{{ struct DisqueSock */
typedef struct {
    php_stream *stream;
    zend_string *host;
    short port;
    zend_string *auth;
    double timeout;
    double read_timeout;
    long retry_interval;
    int failed;
    int status;
    int persistent;
    zend_string *persistent_id;

    int            serializer;
    int            compression;

    zend_string *err;
    zend_string    *prefix;

    int readonly;
    int tcp_keepalive;
} DisqueSock;
/* }}} */

#if (PHP_MAJOR_VERSION < 7)
typedef struct {
    zend_object std;
    DisqueSock *sock;
} disque_object;
#else
typedef struct {
    DisqueSock *sock;
    zend_object std;
} disque_object;
#endif

#ifdef PHP_WIN32
    #	define PHP_DISQUE_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
    #	define PHP_DISQUE_API __attribute__ ((visibility("default")))
#else
#	define PHP_DISQUE_API
#endif

#ifdef ZTS
    #include "TSRM.h"
#endif

#define PHP_DISQUE_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(disque, v)

#if defined(ZTS) && defined(COMPILE_DL_PHP_DISQUE)
    ZEND_TSRMLS_CACHE_EXTERN()
#endif


    #ifndef PHP_FE_END
    #define PHP_FE_END { NULL, NULL, NULL }
    #endif
#define PHPDISQUE_NOTUSED(v) ((void)v)


#include <zend_smart_str.h>
#include <ext/standard/php_smart_string.h>
typedef size_t strlen_t;
#define PHPDISQUE_GET_OBJECT(class_entry, z) (class_entry *)((char *)Z_OBJ_P(z) - XtOffsetOf(class_entry, std))

#define DISQUE_SOCK_STATUS_FAILED       0  /*连接失败*/
#define DISQUE_SOCK_STATUS_DISCONNECTED 1  /*断开连接*/
#define DISQUE_SOCK_STATUS_CONNECTED    2  /*已经连接*/

PHP_MINIT_FUNCTION(disque);
PHP_MSHUTDOWN_FUNCTION(disque);
PHP_MINFO_FUNCTION(disque);

#define DISQUE_STREAM_CLOSE_MARK_FAILED(disque_sock) \
    disque_stream_close(disque_sock TSRMLS_CC); \
    disque_sock->stream = NULL; \
    disque_sock->status = DISQUE_SOCK_STATUS_FAILED


#define PIPELINE_ENQUEUE_COMMAND(cmd, cmd_len) do { \
    if (disque_sock->pipeline_cmd == NULL) { \
        disque_sock->pipeline_cmd = estrndup(cmd, cmd_len); \
    } else { \
        disque_sock->pipeline_cmd = erealloc(disque_sock->pipeline_cmd, \
            disque_sock->pipeline_len + cmd_len); \
        memcpy(&disque_sock->pipeline_cmd[disque_sock->pipeline_len], \
            cmd, cmd_len); \
    } \
    disque_sock->pipeline_len += cmd_len; \
} while (0)

#define SOCKET_WRITE_COMMAND(disque_sock, cmd, cmd_len) \
    if(disque_sock_write(disque_sock, cmd, cmd_len TSRMLS_CC) < 0) { \
    efree(cmd); \
    RETURN_FALSE; \
}
#define DISQUE_PROCESS_REQUEST(disque_sock, cmd, cmd_len) \
    SOCKET_WRITE_COMMAND(disque_sock, cmd, cmd_len); \
    efree(cmd);

#define DISQUE_PROCESS_KW_CMD(kw, cmdfunc, resp_func) \
    DisqueSock *disque_sock; char *cmd; int cmd_len; \
    if ((disque_sock = disque_sock_get(getThis() TSRMLS_CC, 0)) == NULL || \
       cmdfunc(INTERNAL_FUNCTION_PARAM_PASSTHRU, disque_sock, kw, &cmd, \
               &cmd_len)==FAILURE) { \
            RETURN_FALSE; \
    } \
    DISQUE_PROCESS_REQUEST(disque_sock, cmd, cmd_len); \
    resp_func(INTERNAL_FUNCTION_PARAM_PASSTHRU, disque_sock, NULL)

/* serializers */
#define DISQUE_SERIALIZER_NONE        0
#define DISQUE_SERIALIZER_PHP         1
#define DISQUE_SERIALIZER_IGBINARY    2
/* compression */
#define DISQUE_COMPRESSION_NONE 0
#define DISQUE_COMPRESSION_LZF  1

#define DISQUE_CMD_SPPRINTF(ret, kw, fmt, ...) \
    disque_spprintf(disque_sock, ret, kw, fmt, ##__VA_ARGS__)

int disque_cmd_init_sstr(smart_string *str, int num_args, char *keyword, int keyword_len);
int disque_cmd_append_sstr(smart_string *str, char *append, int append_len);
int disque_cmd_append_sstr_int(smart_string *str, int append);
int disque_cmd_append_sstr_long(smart_string *str, long append);
int disque_cmd_append_sstr_dbl(smart_string *str, double value);
int disque_key_long_cmd(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, char *kw, char **cmd, int *cmd_len);
int disque_str_cmd(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, char *kw, char **cmd, int *cmd_len);
int disque_ids_cmd(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, char *kw, char **cmd, int *cmd_len);
int disque_empty_cmd(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, char *kw, char **cmd, int *cmd_len);

PHP_DISQUE_API int disque_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent);
PHP_DISQUE_API int disque_sock_connect(DisqueSock *disque_sock TSRMLS_DC);
PHP_DISQUE_API void disque_sock_set_err(DisqueSock *disque_sock, const char *msg, int msg_len);
PHP_DISQUE_API int disque_sock_server_open(DisqueSock *disque_sock TSRMLS_DC);
PHP_DISQUE_API int disque_sock_disconnect(DisqueSock *disque_sock TSRMLS_DC);
PHP_DISQUE_API void disque_free_socket(DisqueSock *disque_sock);
PHP_DISQUE_API DisqueSock *disque_sock_create(char *host, int host_len, unsigned short port,
                                              double timeout, double read_timeout,
                                              int persistent, char *persistent_id,
                                              long retry_interval);

PHP_DISQUE_API DisqueSock *disque_sock_get(zval *id TSRMLS_DC, int no_throw);
PHP_DISQUE_API DisqueSock *disque_sock_get_connected(INTERNAL_FUNCTION_PARAMETERS);

PHP_DISQUE_API int disque_spprintf(DisqueSock *disque_sock, char **ret, char *kw, char *fmt, ...);

PHP_DISQUE_API int disque_sock_write(DisqueSock *disque_sock, char *cmd, size_t sz TSRMLS_DC);
PHP_DISQUE_API void disque_stream_close(DisqueSock *disque_sock TSRMLS_DC);
PHP_DISQUE_API int disque_key_prefix(DisqueSock *disque_sock, char **key, strlen_t *key_len);
PHP_DISQUE_API int disque_check_eof(DisqueSock *disque_sock, int no_throw TSRMLS_DC);
PHP_DISQUE_API int disque_sock_gets(DisqueSock *disque_sock, char *buf, int buf_size, size_t *line_size TSRMLS_DC);
PHP_DISQUE_API char *disque_sock_read_bulk_reply(DisqueSock *disque_sock, int bytes TSRMLS_DC);
PHP_DISQUE_API char *disque_sock_read(DisqueSock *disque_sock, int *buf_len TSRMLS_DC);
PHP_DISQUE_API int disque_pack(DisqueSock *disque_sock, zval *z, char **val, strlen_t *val_len TSRMLS_DC);

PHP_DISQUE_API int disque_unpack(DisqueSock *disque_sock, const char *val, int val_len, zval *z_ret TSRMLS_DC);
PHP_DISQUE_API int disque_serialize(DisqueSock *disque_sock, zval *z, char **val, strlen_t *val_lenTSRMLS_DC);
PHP_DISQUE_API int disque_unserialize(DisqueSock *disque_sock, const char *val, int val_len, zval *z_ret TSRMLS_DC);
PHP_DISQUE_API void disque_ping_response(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, zval *z_tab);
PHP_DISQUE_API void disque_parse_job_response(zval *z_tab, zval *z_ret);
PHP_DISQUE_API void disque_parse_info_response(char *response, zval *z_ret);
PHP_DISQUE_API int disque_mbulk_reply_raw(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, zval *z_tab);
PHP_DISQUE_API void disque_mbulk_reply_loop(INTERNAL_FUNCTION_PARAMETERS,DisqueSock *disque_sock, zval *z_tab, int count, int unserialize);
PHP_DISQUE_API void disque_multi_job_respone(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, zval *z_tab);

PHP_METHOD (Disque, __construct);
PHP_METHOD (Disque, __destruct);
PHP_METHOD (Disque, connect);
PHP_METHOD (Disque, pconnect);
PHP_METHOD (Disque, close);
PHP_METHOD (Disque, hello);//实现disque的hello命令
PHP_METHOD (Disque, ping);
PHP_METHOD (Disque, info);
PHP_METHOD (Disque, addJob);
PHP_METHOD (Disque, getJob);
PHP_METHOD (Disque, ackJob);
PHP_METHOD (Disque, fastAck);
PHP_METHOD (Disque, delJob);
PHP_METHOD (Disque, show);
PHP_METHOD (Disque, qlen);
PHP_METHOD (Disque, qpeek);
PHP_METHOD(Disque, enqueue);
PHP_METHOD(Disque, dequeue);
#endif    /* PHP_DISQUE_H */
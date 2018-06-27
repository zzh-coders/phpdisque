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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include <ext/standard/php_var.h>
#include <ext/standard/php_math.h>
#include "ext/standard/info.h"
#include "common.h"
#include "php_disque.h"
#include <zend_exceptions.h>

#include "php_network.h"
#include <sys/types.h>
#include <ext/standard/php_rand.h>

zend_class_entry *disque_ce;
zend_class_entry *disque_exception_ce;

ZEND_BEGIN_ARG_INFO_EX(arginfo_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_key, 0, 0, 0)
                ZEND_ARG_INFO(0, job_id)
ZEND_END_ARG_INFO()

//定义参数为空的情况
ZEND_BEGIN_ARG_INFO_EX(arginfo_connect, 0, 0, 1)
                ZEND_ARG_INFO(0, host)
                ZEND_ARG_INFO(0, port)
                ZEND_ARG_INFO(0, timeout)
                ZEND_ARG_INFO(0, retry_interval)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_auth, 0, 0, 1)
                ZEND_ARG_INFO(0, password)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_pconnect, 0, 0, 1)
                ZEND_ARG_INFO(0, host)
                ZEND_ARG_INFO(0, port)
                ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_addjob, 0, 0, 1)
                ZEND_ARG_INFO(0, key)
                ZEND_ARG_INFO(0, job)
                ZEND_ARG_ARRAY_INFO(0, opstions, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getjob, 0, 0, 1)
                ZEND_ARG_INFO(0, key)
                ZEND_ARG_ARRAY_INFO(0, opstions, 0)
ZEND_END_ARG_INFO()


ZEND_BEGIN_ARG_INFO_EX(arginfo_ids, 0, 0, 1)
#if PHP_VERSION_ID >= 50600
                ZEND_ARG_VARIADIC_INFO(0, args)
#else
                ZEND_ARG_INFO(0, ...)
#endif
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_qpeek, 0, 0, 1)
                ZEND_ARG_INFO(0, qname)
                ZEND_ARG_INFO(0, count)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_array, 0, 0, 1)
                ZEND_ARG_ARRAY_INFO(0, array, 0)
ZEND_END_ARG_INFO()

static zend_function_entry disque_functions[] = {
        PHP_ME(Disque, __construct, arginfo_void, ZEND_ACC_CTOR | ZEND_ACC_PUBLIC)
        PHP_ME(Disque, __destruct, arginfo_void, ZEND_ACC_DTOR | ZEND_ACC_PUBLIC)
        PHP_ME(Disque, connect, arginfo_connect, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, pconnect, arginfo_pconnect, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, auth, arginfo_auth, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, close, arginfo_void, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, hello, arginfo_void, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, ping, arginfo_void, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, info, arginfo_void, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, addJob, arginfo_addjob, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, getJob, arginfo_getjob, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, ackJob, arginfo_ids, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, fastAck, arginfo_ids, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, delJob, arginfo_ids, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, show, arginfo_key, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, qlen, arginfo_key, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, qpeek, arginfo_qpeek, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, enqueue, arginfo_ids, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, dequeue, arginfo_ids, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, working, arginfo_key, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, nack, arginfo_ids, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, qstat, arginfo_key, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, qscan, arginfo_array, ZEND_ACC_PUBLIC)
        PHP_ME(Disque, jscan, arginfo_array, ZEND_ACC_PUBLIC)
        PHP_FE_END
};

zend_object_handlers disque_object_handlers;

void free_disque_object(zend_object *object) {
    disque_object *disque = (disque_object *) ((char *) (object) - XtOffsetOf(disque_object, std));

    zend_object_std_dtor(&disque->std TSRMLS_CC);
    if (disque->sock) {
        disque_sock_disconnect(disque->sock TSRMLS_CC);
        disque_free_socket(disque->sock);
    }
}

zend_object *create_disque_object(zend_class_entry *ce TSRMLS_DC) {
    disque_object *disque = ecalloc(1, sizeof(disque_object) + zend_object_properties_size(ce));

    disque->sock = NULL;

    zend_object_std_init(&disque->std, ce TSRMLS_CC);
    object_properties_init(&disque->std, ce);

    memcpy(&disque_object_handlers, zend_get_std_object_handlers(), sizeof(disque_object_handlers));
    disque_object_handlers.offset = XtOffsetOf(disque_object, std);
    disque_object_handlers.free_obj = free_disque_object;
    disque->std.handlers = &disque_object_handlers;

    return &disque->std;
}

PHP_MINIT_FUNCTION (disque) {
    zend_class_entry disque_class_entry;
    zend_class_entry disque_exception_class_entry;
    zend_class_entry * exception_ce = NULL;

    /* Disque class */
    INIT_CLASS_ENTRY(disque_class_entry, "Disque", disque_functions);
    disque_ce = zend_register_internal_class(&disque_class_entry TSRMLS_CC);
    disque_ce->create_object = create_disque_object;

#if HAVE_SPL
    exception_ce = zend_hash_str_find_ptr(CG(class_table), "RuntimeException", sizeof("RuntimeException") - 1);
#endif
    if (exception_ce == NULL) {
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 2)
        exception_ce = zend_exception_get_default();
#else
        exception_ce = zend_exception_get_default(TSRMLS_C);
#endif
    }

    /* DisqueException class */
    INIT_CLASS_ENTRY(disque_exception_class_entry, "DisqueException", NULL);
    disque_exception_ce = zend_register_internal_class_ex(
            &disque_exception_class_entry,
#if (PHP_MAJOR_VERSION < 7)
            exception_ce, NULL TSRMLS_CC
#else
            exception_ce
#endif
    );

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION (disque) {
    return SUCCESS;
}


PHP_RINIT_FUNCTION (disque) {
#if defined(COMPILE_DL_DISQUE) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION (disque) {
    return SUCCESS;
}

PHP_MINFO_FUNCTION (disque) {
    php_info_print_table_start();
    php_info_print_table_header(2, "Disque Support", "enabled");
    php_info_print_table_row(2, "Disque Version", PHP_DISQUE_VERSION);
    php_info_print_table_end();
}

zend_module_entry disque_module_entry = {
        STANDARD_MODULE_HEADER,
        "disque",
        NULL,
        PHP_MINIT(disque),
        PHP_MSHUTDOWN(disque),
        NULL, NULL,
        PHP_MINFO(disque),
        PHP_DISQUE_VERSION,
        STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_DISQUE
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(disque)
#endif

PHP_DISQUE_API int disque_mconnect(INTERNAL_FUNCTION_PARAMETERS, int persistent) {

    zval * object;
    char *host = NULL, *persistent_id = "";
    zend_long port = -1, retry_interval = 0;
    strlen_t host_len, persistent_id_len;
    double timeout = 0.0, read_timeout = 0.0;
    disque_object *disque;

#ifdef ZTS
    /* not sure how in threaded mode this works so disabled persistence at
     * first */
    persistent = 0;
#endif

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(),
                                     "Os|ldsld", &object, disque_ce, &host,
                                     &host_len, &port, &timeout, &persistent_id,
                                     &persistent_id_len, &retry_interval,
                                     &read_timeout) == FAILURE) {
        return FAILURE;
    } else if (!persistent) {
        persistent_id = NULL;
    }
    if (timeout < 0L || timeout > INT_MAX) {
        zend_throw_exception(disque_exception_ce,
                             "Invalid connect timeout", 0 TSRMLS_CC);
        return FAILURE;
    }

    if (read_timeout < 0L || read_timeout > INT_MAX) {
        zend_throw_exception(disque_exception_ce,
                             "Invalid read timeout", 0 TSRMLS_CC);
        return FAILURE;
    }

    if (retry_interval < 0L || retry_interval > INT_MAX) {
        zend_throw_exception(disque_exception_ce, "Invalid retry interval",
                             0 TSRMLS_CC);
        return FAILURE;
    }
    /* If it's not a unix socket, set to default */
    if (port == -1 && host_len && host[0] != '/') {
        port = 7711;
    }

    disque = PHPDISQUE_GET_OBJECT(disque_object, object);
    if (disque->sock) {
        disque_sock_disconnect(disque->sock TSRMLS_CC);
        disque_free_socket(disque->sock);
    }
    disque->sock = disque_sock_create(host, host_len, port, timeout, read_timeout, persistent,
                                      persistent_id, retry_interval);
    if (disque_sock_server_open(disque->sock TSRMLS_CC) < 0) {
        if (disque->sock->err) {
            zend_throw_exception(disque_exception_ce, ZSTR_VAL(disque->sock->err), 0 TSRMLS_CC);
        }
        disque_free_socket(disque->sock);
        disque->sock = NULL;
        return FAILURE;
    }

    return SUCCESS;
}


PHP_DISQUE_API int disque_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent) {

    zval * object;
    char *host = NULL, *persistent_id = "";
    zend_long port = -1, retry_interval = 0;
    strlen_t host_len, persistent_id_len;
    double timeout = 0.0, read_timeout = 0.0;
    disque_object *disque;

#ifdef ZTS
    /* not sure how in threaded mode this works so disabled persistence at
     * first */
    persistent = 0;
#endif

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(),
                                     "Os|ldsld", &object, disque_ce, &host,
                                     &host_len, &port, &timeout, &persistent_id,
                                     &persistent_id_len, &retry_interval,
                                     &read_timeout) == FAILURE) {
        return FAILURE;
    } else if (!persistent) {
        persistent_id = NULL;
    }
    if (timeout < 0L || timeout > INT_MAX) {
        zend_throw_exception(disque_exception_ce,
                             "Invalid connect timeout", 0 TSRMLS_CC);
        return FAILURE;
    }

    if (read_timeout < 0L || read_timeout > INT_MAX) {
        zend_throw_exception(disque_exception_ce,
                             "Invalid read timeout", 0 TSRMLS_CC);
        return FAILURE;
    }

    if (retry_interval < 0L || retry_interval > INT_MAX) {
        zend_throw_exception(disque_exception_ce, "Invalid retry interval",
                             0 TSRMLS_CC);
        return FAILURE;
    }
    /* If it's not a unix socket, set to default */
    if (port == -1 && host_len && host[0] != '/') {
        port = 7711;
    }

    disque = PHPDISQUE_GET_OBJECT(disque_object, object);
    if (disque->sock) {
        disque_sock_disconnect(disque->sock TSRMLS_CC);
        disque_free_socket(disque->sock);
    }
    disque->sock = disque_sock_create(host, host_len, port, timeout, read_timeout, persistent,
                                      persistent_id, retry_interval);
    if (disque_sock_server_open(disque->sock TSRMLS_CC) < 0) {
        if (disque->sock->err) {
            zend_throw_exception(disque_exception_ce, ZSTR_VAL(disque->sock->err), 0 TSRMLS_CC);
        }
        disque_free_socket(disque->sock);
        disque->sock = NULL;
        return FAILURE;
    }

    return SUCCESS;
}

PHP_DISQUE_API int disque_sock_connect(DisqueSock *disque_sock TSRMLS_DC) {
    struct timeval tv, read_tv, *tv_ptr = NULL;
    char host[1024], *persistent_id = NULL;
    const char *fmtstr = "%s:%d";
    int host_len, usocket = 0, err = 0;
    php_netstream_data_t *sock;
    int tcp_flag = 1;
#if (PHP_MAJOR_VERSION < 7)
    char *estr = NULL;
#else
    zend_string * estr = NULL;
#endif

    if (disque_sock->stream != NULL) {
        disque_sock_disconnect(disque_sock TSRMLS_CC);
    }

    tv.tv_sec = (time_t) disque_sock->timeout;
    tv.tv_usec = (int) ((disque_sock->timeout - tv.tv_sec) * 1000000);
    if (tv.tv_sec != 0 || tv.tv_usec != 0) {
        tv_ptr = &tv;
    }

    read_tv.tv_sec = (time_t) disque_sock->read_timeout;
    read_tv.tv_usec = (int) ((disque_sock->read_timeout - read_tv.tv_sec) * 1000000);

    if (ZSTR_VAL(disque_sock->host)[0] == '/' && disque_sock->port < 1) {
        host_len = snprintf(host, sizeof(host), "unix://%s", ZSTR_VAL(disque_sock->host));
        usocket = 1;
    } else {
        if (disque_sock->port == 0)
            disque_sock->port = 7711;

#ifdef HAVE_IPV6
        /* If we've got IPv6 and find a colon in our address, convert to proper
         * IPv6 [host]:port format */
        if (strchr(ZSTR_VAL(disque_sock->host), ':') != NULL) {
            fmtstr = "[%s]:%d";
        }
#endif
        host_len = snprintf(host, sizeof(host), fmtstr, ZSTR_VAL(disque_sock->host), disque_sock->port);
    }

    if (disque_sock->persistent) {
        if (disque_sock->persistent_id) {
            spprintf(&persistent_id, 0, "phpdisque:%s:%s", host,
                     ZSTR_VAL(disque_sock->persistent_id));
        } else {
            spprintf(&persistent_id, 0, "phpdisque:%s:%f", host,
                     disque_sock->timeout);
        }
    }

    disque_sock->stream = php_stream_xport_create(host, host_len,
                                                  0, STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT,
                                                  persistent_id, tv_ptr, NULL, &estr, &err);

    if (persistent_id) {
        efree(persistent_id);
    }

    if (!disque_sock->stream) {
        if (estr) {
#if (PHP_MAJOR_VERSION < 7)
            disque_sock_set_err(disque_sock, estr, strlen(estr));
            efree(estr);
#else
            disque_sock_set_err(disque_sock, ZSTR_VAL(estr), ZSTR_LEN(estr));
            zend_string_release(estr);
#endif
        }
        return -1;
    }

    /* Attempt to set TCP_NODELAY/TCP_KEEPALIVE if we're not using a unix socket. */
    sock = (php_netstream_data_t *) disque_sock->stream->abstract;
    if (!usocket) {
        err = setsockopt(sock->socket, IPPROTO_TCP, TCP_NODELAY, (char *) &tcp_flag, sizeof(tcp_flag));
        PHPDISQUE_NOTUSED(err);
        err = setsockopt(sock->socket, SOL_SOCKET, SO_KEEPALIVE, (char *) &disque_sock->tcp_keepalive,
                         sizeof(disque_sock->tcp_keepalive));
        PHPDISQUE_NOTUSED(err);
    }

    php_stream_auto_cleanup(disque_sock->stream);

    if (read_tv.tv_sec != 0 || read_tv.tv_usec != 0) {
        php_stream_set_option(disque_sock->stream, PHP_STREAM_OPTION_READ_TIMEOUT,
                              0, &read_tv);
    }
    php_stream_set_option(disque_sock->stream,
                          PHP_STREAM_OPTION_WRITE_BUFFER, PHP_STREAM_BUFFER_NONE, NULL);

    disque_sock->status = DISQUE_SOCK_STATUS_CONNECTED;

    return 0;
}

PHP_DISQUE_API void disque_sock_set_err(DisqueSock *disque_sock, const char *msg, int msg_len) {
    // Free our last error
    if (disque_sock->err != NULL) {
        zend_string_release(disque_sock->err);
        disque_sock->err = NULL;
    }

    if (msg != NULL && msg_len > 0) {
        // Copy in our new error message
        disque_sock->err = zend_string_init(msg, msg_len, 0);
    }
}

PHP_DISQUE_API int disque_sock_server_open(DisqueSock *disque_sock TSRMLS_DC) {
    int res = -1;

    switch (disque_sock->status) {
        case DISQUE_SOCK_STATUS_DISCONNECTED:
            return disque_sock_connect(disque_sock TSRMLS_CC);
        case DISQUE_SOCK_STATUS_CONNECTED:
            res = 0;
            break;
    }

    return res;
}

PHP_DISQUE_API int disque_sock_disconnect(DisqueSock *disque_sock TSRMLS_DC) {
    if (disque_sock == NULL) {
        return 1;
    }

    if (disque_sock->stream != NULL) {
        disque_sock->status = DISQUE_SOCK_STATUS_DISCONNECTED;

        /* Stil valid? */
        if (!disque_sock->persistent) {
            php_stream_close(disque_sock->stream);
        }

        disque_sock->stream = NULL;

        return 1;
    }

    return 0;
}

PHP_DISQUE_API void disque_free_socket(DisqueSock *disque_sock) {
    if (disque_sock->err) {
        zend_string_release(disque_sock->err);
    }
    if (disque_sock->auth) {
        zend_string_release(disque_sock->auth);
    }
    if (disque_sock->persistent_id) {
        zend_string_release(disque_sock->persistent_id);
    }
    if (disque_sock->host) {
        zend_string_release(disque_sock->host);
    }
    efree(disque_sock);
}

PHP_DISQUE_API DisqueSock *
disque_sock_create(char *host, int host_len, unsigned short port, double timeout, double read_timeout, int persistent,
                   char *persistent_id, long retry_interval) {
    DisqueSock *disque_sock;

    disque_sock = ecalloc(1, sizeof(DisqueSock));
    disque_sock->host = zend_string_init(host, host_len, 0);
    disque_sock->stream = NULL;
    disque_sock->status = DISQUE_SOCK_STATUS_DISCONNECTED;
    disque_sock->retry_interval = retry_interval * 1000;
    disque_sock->persistent = persistent;
    disque_sock->persistent_id = NULL;

    if (persistent && persistent_id != NULL) {
        disque_sock->persistent_id = zend_string_init(persistent_id, strlen(persistent_id), 0);
    }

    disque_sock->port = port;
    disque_sock->timeout = timeout;
    disque_sock->read_timeout = read_timeout;

    disque_sock->serializer = DISQUE_SERIALIZER_NONE;
    disque_sock->compression = DISQUE_COMPRESSION_NONE;

    disque_sock->err = NULL;

    disque_sock->readonly = 0;
    disque_sock->tcp_keepalive = 0;

    return disque_sock;
}

static zend_always_inline DisqueSock *disque_sock_get_instance(zval *id TSRMLS_DC, int no_throw) {
    disque_object *disque;

    if (Z_TYPE_P(id) == IS_OBJECT) {
        disque = PHPDISQUE_GET_OBJECT(disque_object, id);
        if (disque->sock) {
            return disque->sock;
        }
    }
    // Throw an exception unless we've been requested not to
    if (!no_throw) {
        zend_throw_exception(disque_exception_ce, "Disque server went away", 0 TSRMLS_CC);
    }
    return NULL;
}

PHP_DISQUE_API DisqueSock *disque_sock_get(zval *id TSRMLS_DC, int no_throw) {
    DisqueSock *disque_sock;

    if ((disque_sock = disque_sock_get_instance(id TSRMLS_CC, no_throw)) == NULL) {
        return NULL;
    }

    return disque_sock;
}

PHP_DISQUE_API DisqueSock *disque_sock_get_connected(INTERNAL_FUNCTION_PARAMETERS) {
    zval * object;
    DisqueSock *disque_sock;

// If we can't grab our object, or get a socket, or we're not connected,
// return NULL
    if ((zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O",
                                      &object, disque_ce) == FAILURE) ||
        (disque_sock = disque_sock_get(object TSRMLS_CC, 1)) == NULL ||
        disque_sock->status != DISQUE_SOCK_STATUS_CONNECTED) {
        return NULL;
    }

/* Return our socket */
    return disque_sock;
}

PHP_DISQUE_API int disque_key_prefix(DisqueSock *disque_sock, char **key, strlen_t *key_len) {
    int ret_len;
    char *ret;

    if (disque_sock->prefix == NULL) {
        return 0;
    }

    ret_len = ZSTR_LEN(disque_sock->prefix) + *key_len;
    ret = ecalloc(1 + ret_len, 1);
    memcpy(ret, ZSTR_VAL(disque_sock->prefix), ZSTR_LEN(disque_sock->prefix));
    memcpy(ret + ZSTR_LEN(disque_sock->prefix), *key, *key_len);

    *key = ret;
    *key_len = ret_len;
    return 1;
}

/* A printf like method to construct a Redis RESP command.  It has been extended
 * to take a few different format specifiers that are convienient to phpredis.
 *
 * s - C string followed by length as a strlen_t
 * S - Pointer to a zend_string
 * k - Same as 's' but the value will be prefixed if phpredis is set up do do
 *     that and the working slot will be set if it has been passed.
 * v - A z_val which will be serialized if phpredis is configured to serialize.
 * f - A double value
 * F - Alias to 'f'
 * i - An integer
 * d - Alias to 'i'
 * l - A long
 * L - Alias to 'l'
 */
PHP_DISQUE_API int disque_spprintf(DisqueSock *disque_sock, char **ret, char *kw, char *fmt, ...) {
    smart_string cmd = {0};
    va_list ap;
    union resparg arg;
    char *dup;
    int argfree;
    strlen_t arglen;

    va_start(ap, fmt);

    /* Header */
    disque_cmd_init_sstr(&cmd, strlen(fmt), kw, strlen(kw));

    while (*fmt) {
        switch (*fmt) {
            case 's':
                arg.str = va_arg(ap, char*);
                arglen = va_arg(ap, strlen_t);
                disque_cmd_append_sstr(&cmd, arg.str, arglen);
                break;
            case 'S':
                arg.zstr = va_arg(ap, zend_string *);
                disque_cmd_append_sstr(&cmd, ZSTR_VAL(arg.zstr), ZSTR_LEN(arg.zstr));
                break;
            case 'k':
                arg.str = va_arg(ap, char*);
                arglen = va_arg(ap, strlen_t);
                argfree = disque_key_prefix(disque_sock, &arg.str, &arglen);
                disque_cmd_append_sstr(&cmd, arg.str, arglen);
                if (argfree) efree(arg.str);
                break;
            case 'v':
                arg.zv = va_arg(ap, zval *);
                argfree = disque_pack(disque_sock, arg.zv, &dup, &arglen TSRMLS_CC);
                disque_cmd_append_sstr(&cmd, dup, arglen);
                if (argfree) efree(dup);
                break;
            case 'f':
            case 'F':
                arg.dval = va_arg(ap, double);
                disque_cmd_append_sstr_dbl(&cmd, arg.dval);
                break;
            case 'i':
            case 'd':
                arg.ival = va_arg(ap, int);
                disque_cmd_append_sstr_int(&cmd, arg.ival);
                break;
            case 'l':
            case 'L':
                arg.lval = va_arg(ap, long);
                disque_cmd_append_sstr_long(&cmd, arg.lval);
                break;
        }

        fmt++;
    }
    /* varargs cleanup */
    va_end(ap);

    /* Null terminate */
    smart_string_0(&cmd);

    /* Push command string, return length */
    *ret = cmd.c;
    return cmd.len;
}

int disque_cmd_init_sstr(smart_string *str, int num_args, char *keyword, int keyword_len) {
    smart_string_appendc(str, '*');
    smart_string_append_long(str, num_args + 1);
    smart_string_appendl(str, _NL, sizeof(_NL) - 1);
    smart_string_appendc(str, '$');
    smart_string_append_long(str, keyword_len);
    smart_string_appendl(str, _NL, sizeof(_NL) - 1);
    smart_string_appendl(str, keyword, keyword_len);
    smart_string_appendl(str, _NL, sizeof(_NL) - 1);
    return str->len;
}

int disque_cmd_append_sstr(smart_string *str, char *append, int append_len) {
    smart_string_appendc(str, '$');
    smart_string_append_long(str, append_len);
    smart_string_appendl(str, _NL, sizeof(_NL) - 1);
    smart_string_appendl(str, append, append_len);
    smart_string_appendl(str, _NL, sizeof(_NL) - 1);

    /* Return our new length */
    return str->len;
}

int disque_cmd_append_sstr_int(smart_string *str, int append) {
    char int_buf[32];
    int int_len = snprintf(int_buf, sizeof(int_buf), "%d", append);
    return disque_cmd_append_sstr(str, int_buf, int_len);
}

int disque_cmd_append_sstr_long(smart_string *str, long append) {
    char long_buf[32];
    int long_len = snprintf(long_buf, sizeof(long_buf), "%ld", append);
    return disque_cmd_append_sstr(str, long_buf, long_len);
}

int disque_cmd_append_sstr_dbl(smart_string *str, double value) {
    char tmp[64];
    int len, retval;

    /* Convert to string */
    len = snprintf(tmp, sizeof(tmp), "%.16g", value);

    // Append the string
    retval = disque_cmd_append_sstr(str, tmp, len);

    /* Return new length */
    return retval;
}

PHP_DISQUE_API int disque_sock_write(DisqueSock *disque_sock, char *cmd, size_t sz TSRMLS_DC) {
    if (!disque_sock || disque_sock->status == DISQUE_SOCK_STATUS_DISCONNECTED) {
        zend_throw_exception(disque_exception_ce, "Connection closed",
                             0 TSRMLS_CC);
    } else if (disque_check_eof(disque_sock, 0 TSRMLS_CC) == 0 &&
               php_stream_write(disque_sock->stream, cmd, sz) == sz
            ) {
        return sz;
    }
    return -1;
}

static int resend_auth(DisqueSock *disque_sock TSRMLS_DC) {
    char *cmd, *response;
    int cmd_len, response_len;

    cmd_len = disque_spprintf(disque_sock, &cmd, "AUTH", "s",
                              ZSTR_VAL(disque_sock->auth), ZSTR_LEN(disque_sock->auth));

    if (disque_sock_write(disque_sock, cmd, cmd_len TSRMLS_CC) < 0) {
        efree(cmd);
        return -1;
    }

    efree(cmd);

    response = disque_sock_read(disque_sock, &response_len TSRMLS_CC);
    if (response == NULL) {
        return -1;
    }

    if (strncmp(response, "+OK", 3)) {
        efree(response);
        return -1;
    }

    efree(response);
    return 0;
}

static void disque_error_throw(DisqueSock *disque_sock TSRMLS_DC) {
    if (disque_sock != NULL && disque_sock->err != NULL &&
        memcmp(ZSTR_VAL(disque_sock->err), "ERR", sizeof("ERR") - 1) != 0 &&
        memcmp(ZSTR_VAL(disque_sock->err), "NOSCRIPT", sizeof("NOSCRIPT") - 1) != 0 &&
        memcmp(ZSTR_VAL(disque_sock->err), "WRONGTYPE", sizeof("WRONGTYPE") - 1) != 0
            ) {
        zend_throw_exception(disque_exception_ce, ZSTR_VAL(disque_sock->err), 0 TSRMLS_CC);
    }
}

PHP_DISQUE_API void disque_stream_close(DisqueSock *disque_sock TSRMLS_DC) {
    if (!disque_sock->persistent) {
        php_stream_close(disque_sock->stream);
    } else {
        php_stream_pclose(disque_sock->stream);
    }
}

PHP_DISQUE_API int disque_check_eof(DisqueSock *disque_sock, int no_throw TSRMLS_DC) {
    int count;

    if (!disque_sock->stream) {
        return -1;
    }

    /* NOITCE: set errno = 0 here
     *
     * There is a bug in php socket stream to check liveness of a connection:
     * if (0 >= recv(sock->socket, &buf, sizeof(buf), MSG_PEEK) && php_socket_errno() != EWOULDBLOCK) {
     *    alive = 0;
     * }
     * If last errno is EWOULDBLOCK and recv returns 0 because of connection closed, alive would not be
     * set to 0. However, the connection is close indeed. The php_stream_eof is not reliable. This will
     * cause a "read error on connection" exception when use a closed persistent connection.
     *
     * We work around this by set errno = 0 first.
     *
     * Bug fix of php: https://github.com/php/php-src/pull/1456
     * */
    errno = 0;
    if (php_stream_eof(disque_sock->stream) == 0) {
        /* Success */
        return 0;
    }
    /* TODO: configurable max retry count */
    for (count = 0; count < 10; ++count) {
        /* close existing stream before reconnecting */
        if (disque_sock->stream) {
            disque_stream_close(disque_sock TSRMLS_CC);
            disque_sock->stream = NULL;
        }
        // Wait for a while before trying to reconnect
        if (disque_sock->retry_interval) {
            // Random factor to avoid having several (or many) concurrent connections trying to reconnect at the same time
            long retry_interval = (count ? disque_sock->retry_interval : (php_rand(TSRMLS_C) %
                                                                          disque_sock->retry_interval));
            usleep(retry_interval);
        }
        /* reconnect */
        if (disque_sock_connect(disque_sock TSRMLS_CC) == 0) {
            /* check for EOF again. */
            errno = 0;
            if (php_stream_eof(disque_sock->stream) == 0) {
                /* If we're using a password, attempt a reauthorization */
                if (disque_sock->auth && resend_auth(disque_sock TSRMLS_CC) != 0) {
                    break;
                }
                /* Success */
                return 0;
            }
        }
    }
    /* close stream if still here */
    if (disque_sock->stream) {
        DISQUE_STREAM_CLOSE_MARK_FAILED(disque_sock);
    }
    if (!no_throw) {
        zend_throw_exception(disque_exception_ce, "Connection lost", 0 TSRMLS_CC);
    }
    return -1;
}

PHP_DISQUE_API int disque_sock_gets(DisqueSock *disque_sock, char *buf, int buf_size, size_t *line_size TSRMLS_DC) {
    // Handle EOF
    if (-1 == disque_check_eof(disque_sock, 0 TSRMLS_CC)) {
        return -1;
    }

    if (php_stream_get_line(disque_sock->stream, buf, buf_size, line_size)
        == NULL) {
        // Close, put our socket state into error
        DISQUE_STREAM_CLOSE_MARK_FAILED(disque_sock);

        // Throw a read error exception
        zend_throw_exception(disque_exception_ce, "read error on connection",
                             0 TSRMLS_CC);
        return -1;
    }

    /* We don't need \r\n */
    *line_size -= 2;
    buf[*line_size] = '\0';

    /* Success! */
    return 0;
}

PHP_DISQUE_API char *disque_sock_read_bulk_reply(DisqueSock *disque_sock, int bytes TSRMLS_DC) {
    int offset = 0;
    char *reply, c[2];
    size_t got;

    if (-1 == bytes || -1 == disque_check_eof(disque_sock, 0 TSRMLS_CC)) {
        return NULL;
    }

    /* Allocate memory for string */
    reply = emalloc(bytes + 1);

    /* Consume bulk string */
    while (offset < bytes) {
        got = php_stream_read(disque_sock->stream, reply + offset, bytes - offset);
        if (got == 0) break;
        offset += got;
    }

    /* Protect against reading too few bytes */
    if (offset < bytes) {
        /* Error or EOF */
        zend_throw_exception(disque_exception_ce,
                             "socket error on read socket", 0 TSRMLS_CC);
        efree(reply);
        return NULL;
    }

    /* Consume \r\n and null terminate reply string */
    php_stream_read(disque_sock->stream, c, 2);
    reply[bytes] = '\0';

    return reply;
}

PHP_DISQUE_API char *disque_sock_read(DisqueSock *disque_sock, int *buf_len TSRMLS_DC) {
    char inbuf[4096];
    size_t len;

    *buf_len = 0;
    if (disque_sock_gets(disque_sock, inbuf, sizeof(inbuf) - 1, &len TSRMLS_CC) < 0) {
        return NULL;
    }

    switch (inbuf[0]) {
        case '-':
            disque_sock_set_err(disque_sock, inbuf + 1, len);

            /* Filter our ERROR through the few that should actually throw */
            disque_error_throw(disque_sock TSRMLS_CC);

            return NULL;
        case '$':
            *buf_len = atoi(inbuf + 1);
            return disque_sock_read_bulk_reply(disque_sock, *buf_len TSRMLS_CC);
        case '+':
            *buf_len = len - 1;
            char *p;
            p = (char *) emalloc(len);
            if (UNEXPECTED(p == NULL)) {
                return p;
            }
            memcpy(p, inbuf + 1, len - 1);   //分配空间
            p[len - 1] = '\0';
            return p;
        case '*':
            /* For null multi-bulk replies (like timeouts from brpoplpush): */
            if (memcmp(inbuf + 1, "-1", 2) == 0) {
                return NULL;
            }
        case ':':
            /* Single Line Reply */
            /* +OK or :123 */
            if (len > 1) {
                *buf_len = len;
                return estrndup(inbuf, *buf_len);
            }
        default:
            zend_throw_exception_ex(
                    disque_exception_ce,
                    0 TSRMLS_CC,
                    "protocol error, got '%c' as reply type byte\n",
                    inbuf[0]
            );
    }

    return NULL;
}

PHP_DISQUE_API int disque_pack(DisqueSock *disque_sock, zval *z, char **val, strlen_t *val_len TSRMLS_DC) {
    char *buf, *data;
    int valfree;
    strlen_t len;
    uint32_t res;

    valfree = disque_serialize(disque_sock, z, &buf, &len TSRMLS_CC);
    switch (disque_sock->compression) {
        case DISQUE_COMPRESSION_LZF:
#ifdef HAVE_REDIS_LZF
            data = emalloc(len);
            res = lzf_compress(buf, len, data, len - 1);
            if (res > 0 && res < len) {
                if (valfree) efree(buf);
                *val = data;
                *val_len = res;
                 return 1;
            }
            efree(data);
#endif
            break;
    }
    *val = buf;
    *val_len = len;
    return valfree;
}

PHP_DISQUE_API int disque_unpack(DisqueSock *disque_sock, const char *val, int val_len, zval *z_ret TSRMLS_DC) {
    char *data;
    int i;
    uint32_t res;

    switch (disque_sock->compression) {
        case DISQUE_COMPRESSION_LZF:
#ifdef HAVE_REDIS_LZF
            errno = E2BIG;
            /* start from two-times bigger buffer and
             * increase it exponentially  if needed */
            for (i = 2; errno == E2BIG; i *= 2) {
                data = emalloc(i * val_len);
                if ((res = lzf_decompress(val, val_len, data, i * val_len)) == 0) {
                    /* errno != E2BIG will brake for loop */
                    efree(data);
                    continue;
                } else if (redis_unserialize(redis_sock, data, res, z_ret TSRMLS_CC) == 0) {
                    ZVAL_STRINGL(z_ret, data, res);
                }
                efree(data);
                return 1;
            }
#endif
            break;
    }
    return disque_unserialize(disque_sock, val, val_len, z_ret TSRMLS_CC);
}

PHP_DISQUE_API int disque_serialize(DisqueSock *disque_sock, zval *z, char **val, strlen_t *val_len TSRMLS_DC) {
#if ZEND_MODULE_API_NO >= 20100000
    php_serialize_data_t ht;
#else
    HashTable ht;
#endif
    smart_str sstr = {0};
#ifdef HAVE_REDIS_IGBINARY
    size_t sz;
    uint8_t *val8;
#endif

    *val = NULL;
    *val_len = 0;
    switch (disque_sock->serializer) {
        case DISQUE_SERIALIZER_NONE:
            switch (Z_TYPE_P(z)) {

                case IS_STRING:
                    *val = Z_STRVAL_P(z);
                    *val_len = Z_STRLEN_P(z);
                    break;

                case IS_OBJECT:
                    *val = "Object";
                    *val_len = 6;
                    break;

                case IS_ARRAY:
                    *val = "Array";
                    *val_len = 5;
                    break;

                default: { /* copy */
                    zend_string * zstr = zval_get_string(z);
                    *val = estrndup(ZSTR_VAL(zstr), ZSTR_LEN(zstr));
                    *val_len = ZSTR_LEN(zstr);
                    zend_string_release(zstr);
                    return 1;
                }
            }
            break;
        case DISQUE_SERIALIZER_PHP:

#if ZEND_MODULE_API_NO >= 20100000
            PHP_VAR_SERIALIZE_INIT(ht);
#else
            zend_hash_init(&ht, 10, NULL, NULL, 0);
#endif
            php_var_serialize(&sstr, z, &ht);
#if (PHP_MAJOR_VERSION < 7)
        *val = estrndup(sstr.c, sstr.len);
            *val_len = sstr.len;
#else
            *val = estrndup(ZSTR_VAL(sstr.s), ZSTR_LEN(sstr.s));
            *val_len = ZSTR_LEN(sstr.s);
#endif
            smart_str_free(&sstr);
#if ZEND_MODULE_API_NO >= 20100000
            PHP_VAR_SERIALIZE_DESTROY(ht);
#else
            zend_hash_destroy(&ht);
#endif

            return 1;

        case DISQUE_SERIALIZER_IGBINARY:
#ifdef HAVE_REDIS_IGBINARY
            if(igbinary_serialize(&val8, (size_t *)&sz, z TSRMLS_CC) == 0) {
                *val = (char*)val8;
                *val_len = sz;
                return 1;
            }
#endif
            break;
    }
    return 0;
}

PHP_DISQUE_API int disque_unserialize(DisqueSock *disque_sock, const char *val, int val_len, zval *z_ret TSRMLS_DC) {
    php_unserialize_data_t var_hash;
    int ret = 0;

    switch (disque_sock->serializer) {
        case DISQUE_SERIALIZER_PHP:
#if ZEND_MODULE_API_NO >= 20100000
            PHP_VAR_UNSERIALIZE_INIT(var_hash);
#else
            memset(&var_hash, 0, sizeof(var_hash));
#endif
            if (php_var_unserialize(z_ret, (const unsigned char **) &val,
                                    (const unsigned char *) val + val_len, &var_hash)
                    ) {
                ret = 1;
            }
#if ZEND_MODULE_API_NO >= 20100000
            PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
#else
            var_destroy(&var_hash);
#endif
            break;

        case DISQUE_SERIALIZER_IGBINARY:
#ifdef HAVE_REDIS_IGBINARY
            /*
             * Check if the given string starts with an igbinary header.
             *
             * A modern igbinary string consists of the following format:
             *
             * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
             * | header (4) | type (1) | ... (n) |  NUL (1) |
             * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
             *
             * With header being either 0x00000001 or 0x00000002
             * (encoded as big endian).
             *
             * Not all versions contain the trailing NULL byte though, so
             * do not check for that.
             */
            if (val_len < 5
                    || (memcmp(val, "\x00\x00\x00\x01", 4) != 0
                    && memcmp(val, "\x00\x00\x00\x02", 4) != 0))
            {
                /* This is most definitely not an igbinary string, so do
                   not try to unserialize this as one. */
                break;
            }

#if (PHP_MAJOR_VERSION < 7)
            INIT_PZVAL(z_ret);
            ret = !igbinary_unserialize((const uint8_t *)val, (size_t)val_len, &z_ret TSRMLS_CC);
#else
            ret = !igbinary_unserialize((const uint8_t *)val, (size_t)val_len, z_ret TSRMLS_CC);
#endif

#endif
            break;
    }
    return ret;
}

PHP_DISQUE_API void disque_ping_response(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, zval *z_tab) {

    char *response;
    int response_len;

    if ((response = disque_sock_read(disque_sock, &response_len TSRMLS_CC))
        == NULL) {

        RETURN_FALSE;
    }

    RETVAL_STRINGL(response, response_len);

    efree(response);
}

int disque_str_cmd(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, char *kw, char **cmd, int *cmd_len) {
    char *arg;
    strlen_t arg_len;

    // Parse args
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len)
        == FAILURE) {
        return FAILURE;
    }

    // Build the command without molesting the string
    *cmd_len = DISQUE_CMD_SPPRINTF(cmd, kw, "s", arg, arg_len);

    return SUCCESS;
}

int disque_auth_cmd(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, char **cmd, int *cmd_len) {
    char *pw;
    strlen_t pw_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &pw, &pw_len)
        == FAILURE) {
        return FAILURE;
    }

    // Construct our AUTH command
    *cmd_len = DISQUE_CMD_SPPRINTF(cmd, "AUTH", "s", pw, pw_len);

    // Free previously allocated password, and update
    if (disque_sock->auth) zend_string_release(disque_sock->auth);
    disque_sock->auth = zend_string_init(pw, pw_len, 0);

    // Success
    return SUCCESS;
}

int disque_empty_cmd(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, char *kw, char **cmd, int *cmd_len) {
    *cmd_len = DISQUE_CMD_SPPRINTF(cmd, kw, "");
    return SUCCESS;
}

int disque_ids_cmd(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, char *kw, char **cmd, int *cmd_len) {
    int argc = ZEND_NUM_ARGS();
    zval * z_args;

    z_args = emalloc(argc * sizeof(zval));
    if (zend_get_parameters_array(ht, argc, z_args) == FAILURE) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING,
                         "Internal PHP error parsing arguments");
        efree(z_args);
        return FAILURE;
    }

    smart_string cmdstr = {0};
    int i;
    /* Initialize our command string */
    disque_cmd_init_sstr(&cmdstr, argc, kw, strlen(kw));

    for (i = 0; i < argc; i++) {
        switch (Z_TYPE(z_args[i])) {
            case IS_STRING:
                disque_cmd_append_sstr(&cmdstr, Z_STRVAL(z_args[i]),
                                       Z_STRLEN(z_args[i]));
                break;
            default:
                php_error_docref(NULL TSRMLS_CC, E_WARNING,
                                 "Raw command arguments must be scalar values!");
                efree(cmdstr.c);
                return FAILURE;
        }
    }
    efree(z_args);
    /* Push command and length to caller */
    *cmd = cmdstr.c;
    *cmd_len = cmdstr.len;

    return SUCCESS;
}

int disque_add_job_cmd(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, char *kw, char **cmd, int *cmd_len) {
    char *key, *job;
    strlen_t key_len, job_len;
    zval * z_opt = NULL;
    int argc = ZEND_NUM_ARGS();
    smart_string cmdstr = {0};
    HashTable * ht_opt;

    int arg_count = 2, timeout = 0, async = IS_FALSE;

    /* We need either 3 or 5 arguments for this to be valid */
    if (argc != 2 && argc != 3) {
        php_error_docref(0 TSRMLS_CC, E_WARNING, "Must pass either 2 or 3 arguments");
        return FAILURE;
    }

    if (zend_parse_parameters(argc TSRMLS_CC, "ss|a", &key, &key_len, &job, &job_len, &z_opt) == FAILURE) {
        return FAILURE;
    }

    if (z_opt && Z_TYPE_P(z_opt) == IS_ARRAY && (zend_hash_num_elements(Z_ARRVAL_P(z_opt)) > 0)) {
        HashTable * ht_opt = Z_ARRVAL_P(z_opt);
        zval * v;
        int opt_count = zend_hash_num_elements(ht_opt);
        if (((v = zend_hash_str_find(ht_opt, "timeout", sizeof("timeout") - 1)) != NULL) &&
            Z_TYPE_P(v) == IS_LONG) {
            arg_count++;
            opt_count--;
            timeout = zval_get_long(v);
        }

        if (((v = zend_hash_str_find(ht_opt, "async", sizeof("async") - 1)) != NULL) &&
            Z_TYPE_P(v) == IS_TRUE) {
            arg_count++;
            opt_count--;
            async = IS_TRUE;
        }

        arg_count += (opt_count * 2);
        /* Build our command header */
        disque_cmd_init_sstr(&cmdstr, arg_count, kw, strlen(kw));
        disque_cmd_append_sstr(&cmdstr, key, key_len);
        disque_cmd_append_sstr(&cmdstr, job, job_len);
        disque_cmd_append_sstr_int(&cmdstr, timeout);
        if (IS_TRUE == async) {
            disque_cmd_append_sstr(&cmdstr, "ASYNC", sizeof("ASYNC") - 1);
        }
        zend_string * zkey;
        ulong idx;
        PHPDISQUE_NOTUSED(idx);
        zval * z_ele;
        ZEND_HASH_FOREACH_KEY_VAL(ht_opt, idx, zkey, z_ele)
                {
                    /* All options require a string key type */
                    if (!zkey) continue;
                    ZVAL_DEREF(z_ele);
                    if (IS_REPLICATE_ARG(ZSTR_VAL(zkey), ZSTR_LEN(zkey)) && Z_TYPE_P(z_ele) == IS_LONG) {
                        disque_cmd_append_sstr(&cmdstr, "REPLICATE", strlen("REPLICATE"));
                    } else if (IS_DELAY_ARG(ZSTR_VAL(zkey), ZSTR_LEN(zkey)) && Z_TYPE_P(z_ele) == IS_LONG) {
                        disque_cmd_append_sstr(&cmdstr, "DELAY", strlen("DELAY"));
                    } else if (IS_MAXLEN_ARG(ZSTR_VAL(zkey), ZSTR_LEN(zkey)) && Z_TYPE_P(z_ele) == IS_LONG) {
                        disque_cmd_append_sstr(&cmdstr, "MAXLEN", strlen("MAXLEN"));
                    } else if (IS_TTL_ARG(ZSTR_VAL(zkey), ZSTR_LEN(zkey)) && Z_TYPE_P(z_ele) == IS_LONG) {
                        disque_cmd_append_sstr(&cmdstr, "TTL", strlen("TTL"));
                    } else if (IS_RETRY_ARG(ZSTR_VAL(zkey), ZSTR_LEN(zkey)) && Z_TYPE_P(z_ele) == IS_LONG) {
                        disque_cmd_append_sstr(&cmdstr, "RETRY", strlen("RETRY"));
                    } else {
                        continue;
                    }
                    disque_cmd_append_sstr_int(&cmdstr, zval_get_long(z_ele));
                }ZEND_HASH_FOREACH_END();

    } else {
        disque_cmd_init_sstr(&cmdstr, 3, kw, strlen(kw));
        disque_cmd_append_sstr(&cmdstr, key, key_len);
        disque_cmd_append_sstr(&cmdstr, job, job_len);
        disque_cmd_append_sstr_int(&cmdstr, 0);
    }

    // Push values out
    *cmd_len = cmdstr.len;
    *cmd = cmdstr.c;
    return SUCCESS;
}

int disque_key_long_cmd(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, char *kw, char **cmd, int *cmd_len) {
    char *key;
    strlen_t keylen;
    zend_long lval;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &key, &keylen, &lval)
        == FAILURE) {
        return FAILURE;
    }

    *cmd_len = DISQUE_CMD_SPPRINTF(cmd, kw, "kl", key, keylen, lval);

    // Success!
    return SUCCESS;
}

int disque_get_job_cmd(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, char *kw, char **cmd, int *cmd_len) {
    char *key;
    strlen_t key_len;
    zval * z_opt = NULL, *z_ele;
    int argc = ZEND_NUM_ARGS();
    zend_string * zkey;
    smart_string cmdstr = {0};
    ulong idx;
    int arg_count = 2;

    int nohang = IS_FALSE, withcounters = IS_FALSE;
    PHPDISQUE_NOTUSED(idx);

    if (argc != 1 && argc != 2) {
        php_error_docref(0 TSRMLS_CC, E_WARNING, "Must pass either 1 or 2 arguments");
        return FAILURE;
    }

    if (zend_parse_parameters(argc TSRMLS_CC, "s|z", &key, &key_len, &z_opt) == FAILURE) {
        return FAILURE;
    }
    if (z_opt && Z_TYPE_P(z_opt) == IS_ARRAY && (zend_hash_num_elements(Z_ARRVAL_P(z_opt)) > 0)) {
        HashTable * ht_opt = Z_ARRVAL_P(z_opt);
        int opt_count = zend_hash_num_elements(ht_opt);
        if (((z_ele = zend_hash_str_find(ht_opt, "nohang", sizeof("nohang") - 1)) != NULL) &&
            Z_TYPE_P(z_ele) == IS_TRUE) {
            arg_count++;
            opt_count--;
            nohang = IS_TRUE;
//            zend_hash_str_del(ht_opt, "nohang", sizeof("nohang") - 1);
        }

        if (((z_ele = zend_hash_str_find(ht_opt, "withcounters", sizeof("withcounters") - 1)) != NULL) &&
            Z_TYPE_P(z_ele) == IS_TRUE) {
            arg_count++;
            opt_count--;
            withcounters = IS_TRUE;
//            zend_hash_str_del(ht_opt, "withcounters", sizeof("withcounters") - 1);
        }


        arg_count += (opt_count * 2);
        /* Build our command header */
        disque_cmd_init_sstr(&cmdstr, arg_count, kw, strlen(kw));
        if (nohang == IS_TRUE) {
            disque_cmd_append_sstr(&cmdstr, "NOHANG", sizeof("NOHANG") - 1);
        }
        if (withcounters == IS_TRUE) {
            disque_cmd_append_sstr(&cmdstr, "WITHCOUNTERS", sizeof("WITHCOUNTERS") - 1);
        }
        ZEND_HASH_FOREACH_KEY_VAL(ht_opt, idx, zkey, z_ele)
                {
                    /* All options require a string key type */
                    if (!zkey) continue;
                    ZVAL_DEREF(z_ele);
                    if (IS_TIMEOUT_ARG(ZSTR_VAL(zkey), ZSTR_LEN(zkey)) && Z_TYPE_P(z_ele) == IS_LONG) {
                        disque_cmd_append_sstr(&cmdstr, "TIMEOUT", sizeof("TIMEOUT") - 1);
                    } else if (IS_COUNT_ARG(ZSTR_VAL(zkey), ZSTR_LEN(zkey)) && Z_TYPE_P(z_ele) == IS_LONG) {
                        disque_cmd_append_sstr(&cmdstr, "COUNT", strlen("COUNT"));
                    } else {
                        continue;
                    }
                    disque_cmd_append_sstr_int(&cmdstr, zval_get_long(z_ele));
                }ZEND_HASH_FOREACH_END();

    } else {
        disque_cmd_init_sstr(&cmdstr, arg_count, kw, strlen(kw));
    }
    disque_cmd_append_sstr(&cmdstr, "FROM", 4);
    disque_cmd_append_sstr(&cmdstr, key, strlen(key));
    // Push values out
    *cmd_len = cmdstr.len;
    *cmd = cmdstr.c;

    return SUCCESS;
}

int disque_qscan_cmd(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, char *kw, char **cmd, int *cmd_len) {
    char *key;
    strlen_t key_len;
    zval * z_opt = NULL, *z_ele;
    int argc = ZEND_NUM_ARGS();
    zend_string * zkey;
    smart_string cmdstr = {0};
    ulong idx;
    int arg_count = 0;
    zend_bool busyloop = IS_FALSE;
    PHPDISQUE_NOTUSED(idx);

    if (argc > 1) {
        php_error_docref(0 TSRMLS_CC, E_WARNING, "Must pass either 0 or 1 arguments");
        return FAILURE;
    }

    if (zend_parse_parameters(argc TSRMLS_CC, "|z", &z_opt) == FAILURE) {
        return FAILURE;
    }
    if (z_opt && Z_TYPE_P(z_opt) == IS_ARRAY && (zend_hash_num_elements(Z_ARRVAL_P(z_opt)) > 0)) {
        HashTable * ht_opt = Z_ARRVAL_P(z_opt);
        int opt_count = zend_hash_num_elements(ht_opt);
        if (((z_ele = zend_hash_str_find(ht_opt, "busyloop", sizeof("busyloop") - 1)) != NULL) &&
            Z_TYPE_P(z_ele) == IS_TRUE) {
            arg_count++;
            opt_count--;
            busyloop = IS_TRUE;
//            zend_hash_str_del(ht_opt, "busyloop", sizeof("busyloop") - 1);
        }


        arg_count += (opt_count * 2);
        /* Build our command header */
        disque_cmd_init_sstr(&cmdstr, arg_count, kw, strlen(kw));
        if (IS_TRUE == busyloop) {
            disque_cmd_append_sstr(&cmdstr, "BUSYLOOP", sizeof("BUSYLOOP") - 1);
        }
        ZEND_HASH_FOREACH_KEY_VAL(ht_opt, idx, zkey, z_ele)
                {
                    /* All options require a string key type */
                    if (!zkey) continue;
                    ZVAL_DEREF(z_ele);

                    if (IS_COUNT_ARG(ZSTR_VAL(zkey), ZSTR_LEN(zkey)) && Z_TYPE_P(z_ele) == IS_LONG) {
                        disque_cmd_append_sstr(&cmdstr, "COUNT", sizeof("COUNT") - 1);
                    } else if (IS_MAXLEN_ARG(ZSTR_VAL(zkey), ZSTR_LEN(zkey)) && Z_TYPE_P(z_ele) == IS_LONG) {
                        disque_cmd_append_sstr(&cmdstr, "MAXLEN", sizeof("MAXLEN") - 1);
                    } else if (IS_MINLEN_ARG(ZSTR_VAL(zkey), ZSTR_LEN(zkey)) && Z_TYPE_P(z_ele) == IS_LONG) {
                        disque_cmd_append_sstr(&cmdstr, "MINLEN", sizeof("MINLEN") - 1);
                    } else if (IS_IMPORTRATE_ARG(ZSTR_VAL(zkey), ZSTR_LEN(zkey)) && Z_TYPE_P(z_ele) == IS_LONG) {
                        disque_cmd_append_sstr(&cmdstr, "IMPORTRATE", sizeof("IMPORTRATE") - 1);
                    } else {
                        continue;
                    }
                    disque_cmd_append_sstr_int(&cmdstr, zval_get_long(z_ele));
                }ZEND_HASH_FOREACH_END();

    } else {
        disque_cmd_init_sstr(&cmdstr, arg_count, kw, strlen(kw));
    }
    // Push values out
    *cmd_len = cmdstr.len;
    *cmd = cmdstr.c;

    return SUCCESS;
}

int disque_jscan_cmd(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, char *kw, char **cmd, int *cmd_len) {
    char *key;
    strlen_t key_len;
    zval * z_opt = NULL, *z_ele, *z_state;
    int argc = ZEND_NUM_ARGS();
    zend_string * zkey;
    smart_string cmdstr = {0};
    ulong idx, cursor = 0;
    HashTable * ht_state;
    int arg_count = 0;
    zend_bool busyloop = IS_FALSE;
    PHPDISQUE_NOTUSED(idx);

    if (argc > 1) {
        php_error_docref(0 TSRMLS_CC, E_WARNING, "Must pass either 0 or 1 arguments");
        return FAILURE;
    }

    if (zend_parse_parameters(argc TSRMLS_CC, "|z", &z_opt) == FAILURE) {
        return FAILURE;
    }
    if (z_opt && Z_TYPE_P(z_opt) == IS_ARRAY && (zend_hash_num_elements(Z_ARRVAL_P(z_opt)) > 0)) {
        HashTable * ht_opt = Z_ARRVAL_P(z_opt);
        int opt_count = zend_hash_num_elements(ht_opt);
        if (((z_ele = zend_hash_str_find(ht_opt, "busyloop", sizeof("busyloop") - 1)) != NULL) &&
            Z_TYPE_P(z_ele) == IS_TRUE) {
            arg_count++;
            opt_count--;
            busyloop = IS_TRUE;
//            zend_hash_str_del(ht_opt, "busyloop", sizeof("busyloop") - 1);
        }
        if (((z_ele = zend_hash_str_find(ht_opt, "cursor", sizeof("cursor") - 1)) != NULL) &&
            Z_TYPE_P(z_ele) == IS_LONG) {
            arg_count++;
            opt_count--;
            cursor = Z_LVAL_P(z_ele);
//            zend_hash_str_del(ht_opt, "cursor", sizeof("cursor") - 1);
        }

        if (((z_ele = zend_hash_str_find(ht_opt, "state", sizeof("state") - 1)) != NULL) &&
            Z_TYPE_P(z_ele) == IS_ARRAY) {
            ZVAL_DEREF(z_ele);
            ht_state = Z_ARRVAL_P(z_ele);
            arg_count += (zend_hash_num_elements(ht_state) * 2);
            opt_count--;
        }

        arg_count += (opt_count * 2);
        /* Build our command header */
        disque_cmd_init_sstr(&cmdstr, arg_count, kw, strlen(kw));
        disque_cmd_append_sstr_long(&cmdstr, cursor);
        if (IS_TRUE == busyloop) {
            disque_cmd_append_sstr(&cmdstr, "BUSYLOOP", sizeof("BUSYLOOP") - 1);
        }
        if (ht_state) {
            ulong t_idx;
            zend_string * t_zkey;
            zval * t_z_ele;
            ZEND_HASH_FOREACH_KEY_VAL(ht_state, t_idx, t_zkey, t_z_ele)
                    {
                        disque_cmd_append_sstr(&cmdstr, "STATE", sizeof("STATE") - 1);
                        disque_cmd_append_sstr(&cmdstr, Z_STRVAL_P(t_z_ele), Z_STRLEN_P(t_z_ele));
                    }
            ZEND_HASH_FOREACH_END();
        }
        ZEND_HASH_FOREACH_KEY_VAL(ht_opt, idx, zkey, z_ele)
                {
                    /* All options require a string key type */
                    if (!zkey) continue;
                    ZVAL_DEREF(z_ele);

                    if (IS_COUNT_ARG(ZSTR_VAL(zkey), ZSTR_LEN(zkey)) && Z_TYPE_P(z_ele) == IS_LONG) {
                        disque_cmd_append_sstr(&cmdstr, "COUNT", sizeof("COUNT") - 1);
                        disque_cmd_append_sstr_int(&cmdstr, zval_get_long(z_ele));
                    } else if (IS_REPLY_ARG(ZSTR_VAL(zkey), ZSTR_LEN(zkey)) && Z_TYPE_P(z_ele) == IS_STRING) {
                        disque_cmd_append_sstr(&cmdstr, "REPLY", sizeof("REPLY") - 1);
                        disque_cmd_append_sstr(&cmdstr, Z_STRVAL_P(z_ele), Z_STRLEN_P(z_ele));
                    } else if (IS_QUEUE_ARG(ZSTR_VAL(zkey), ZSTR_LEN(zkey)) && Z_TYPE_P(z_ele) == IS_STRING) {
                        disque_cmd_append_sstr(&cmdstr, "QUEUE", sizeof("QUEUE") - 1);
                        disque_cmd_append_sstr(&cmdstr, Z_STRVAL_P(z_ele), Z_STRLEN_P(z_ele));
                    } else {
                        continue;
                    }

                }ZEND_HASH_FOREACH_END();

    } else {
        disque_cmd_init_sstr(&cmdstr, arg_count, kw, strlen(kw));
    }
    // Push values out
    *cmd_len = cmdstr.len;
    *cmd = cmdstr.c;

    return SUCCESS;
}

PHP_DISQUE_API void disque_info_response(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, zval *z_tab) {
    char *response;
    int response_len;
    zval zv = {{0}}, *z_ret = &zv;

    /* Read bulk response */
    if ((response = disque_sock_read(disque_sock, &response_len TSRMLS_CC)) == NULL) {
        RETURN_FALSE;
    }

    /* Parse it into a zval array */
    disque_parse_info_response(response, z_ret);

    /* Free source response */
    efree(response);


    RETVAL_ZVAL(z_ret, 0, 1);
}

PHP_DISQUE_API void disque_long_response(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, zval *z_tab) {

    char *response;
    int response_len;

    if ((response = disque_sock_read(disque_sock, &response_len TSRMLS_CC))
        == NULL) {

        RETURN_FALSE;
    }

    if (response[0] == ':') {
#ifdef PHP_WIN32
        __int64 ret = _atoi64(response + 1);
#else
        long long ret = atoll(response + 1);
#endif

        if (ret > LONG_MAX) { /* overflow */
            RETVAL_STRINGL(response + 1, response_len - 1);
        } else {
            RETVAL_LONG((long) ret);
        }
    } else {

        RETVAL_FALSE;
    }
    efree(response);
}

PHP_DISQUE_API void disque_multi_job_respone(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, zval *z_tab) {

    disque_mbulk_reply_raw(INTERNAL_FUNCTION_PARAM_PASSTHRU, disque_sock, z_tab);
    zval zv = {{0}}, *z_ret = &zv;
#if (PHP_MAJOR_VERSION < 7)
    MAKE_STD_ZVAL(z_ret);
#endif
    array_init(z_ret);

    if (return_value && Z_TYPE_P(return_value) == IS_ARRAY) {
        disque_parse_job_response(return_value, z_ret);
    }


    RETVAL_ZVAL(z_ret, 0, 1);
}

PHP_DISQUE_API int disque_mbulk_reply_raw(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, zval *z_tab) {
    char inbuf[4096];
    int numElems;
    size_t len;

    if (disque_sock_gets(disque_sock, inbuf, sizeof(inbuf) - 1, &len TSRMLS_CC) < 0) {
        return -1;
    }

    if (inbuf[0] != '*') {

        if (inbuf[0] == '-') {
            disque_sock_set_err(disque_sock, inbuf + 1, len);
        }
        RETVAL_FALSE;
        return -1;
    }
    numElems = atoi(inbuf + 1);
    zval zv, *z_multi_result = &zv;
#if (PHP_MAJOR_VERSION < 7)
    MAKE_STD_ZVAL(z_multi_result);
#endif
    array_init(z_multi_result); /* pre-allocate array for multi's results. */

    disque_mbulk_reply_loop(INTERNAL_FUNCTION_PARAM_PASSTHRU, disque_sock,
                            z_multi_result, numElems, UNSERIALIZE_NONE);


    RETVAL_ZVAL(z_multi_result, 0, 1);

    /*zval_copy_ctor(return_value); */
    return 0;
}

PHP_DISQUE_API void
disque_mbulk_reply_loop(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, zval *z_tab, int count,
                        int unserialize) {
    char *line;
    int i, len;

    for (i = 0; i < count; ++i) {
        if ((line = disque_sock_read(disque_sock, &len TSRMLS_CC)) == NULL) {
            add_next_index_bool(z_tab, 0);
            continue;
        }
        if (line[0] == ':') {
            /*
             *这里返回时个整数
             */
#ifdef PHP_WIN32
            __int64 ret = _atoi64(line + 1);
#else
            long long ret = atoll(line + 1);
#endif

            if (ret > LONG_MAX) { /* overflow */
                add_next_index_stringl(z_tab, line + 1, len - 1);
            } else {
                add_next_index_long(z_tab, (long) ret);
            }

            continue;
        }
        /*
         * 如果这里转出来是*开头的,那么这里就是转成zval结构了
         */
        if (line[0] == '*') {
            int numElems = atoi(line + 1);

            zval zv = {{0}}, *z_multi_result = &zv;
#if (PHP_MAJOR_VERSION < 7)
            MAKE_STD_ZVAL(z_multi_result);
#endif
            array_init(z_multi_result);
            disque_mbulk_reply_loop(INTERNAL_FUNCTION_PARAM_PASSTHRU, disque_sock, z_multi_result, numElems,
                                    UNSERIALIZE_NONE);
            add_next_index_zval(z_tab, z_multi_result);
            continue;

        }
        /* We will attempt unserialization, if we're unserializing everything,
         * or if we're unserializing keys and we're on a key, or we're
         * unserializing values and we're on a value! */
        int unwrap = (
                (unserialize == UNSERIALIZE_ALL) ||
                (unserialize == UNSERIALIZE_KEYS && i % 2 == 0) ||
                (unserialize == UNSERIALIZE_VALS && i % 2 != 0)
        );
        zval zv, *z = &zv;
        if (unwrap && disque_unpack(disque_sock, line, len, z TSRMLS_CC)) {
#if (PHP_MAJOR_VERSION < 7)
            MAKE_STD_ZVAL(z);
            *z = zv;
#endif
            add_next_index_zval(z_tab, z);
        } else {
            add_next_index_stringl(z_tab, line, len);
        }
        efree(line);
    }
}

PHP_DISQUE_API int disque_parse_job_response(zval *z_tab, zval *z_ret) {
    ulong idx;
    zend_string * zkey;
    HashTable * ht_opt, *ht_z_ele;
    zval * z_ele;


    ht_opt = Z_ARRVAL_P(z_tab);
    ZEND_HASH_FOREACH_KEY_VAL(ht_opt, idx, zkey, z_ele)
            {
                if (!z_ele || Z_TYPE_P(z_ele) != IS_ARRAY) {
                    continue;
                }
                ZVAL_DEREF(z_ele);
                ht_z_ele = Z_ARRVAL_P(z_ele);
                ulong tmp_idx;
                zval * z_v;
                zval zv, *tmp_zval = &zv;
                array_init(tmp_zval);
                ZEND_HASH_FOREACH_NUM_KEY_VAL(Z_ARRVAL_P(z_ele), tmp_idx, z_v)
                        {
                            if (!z_v || Z_TYPE_P(z_v) != IS_STRING) {
                                continue;
                            }
                            ZVAL_DEREF(z_v);
                            switch (tmp_idx) {
                                case 0:
                                    add_assoc_stringl(tmp_zval, "queue", Z_STRVAL_P(z_v), Z_STRLEN_P(z_v));
                                    break;
                                case 1:
                                    add_assoc_stringl(tmp_zval, "id", Z_STRVAL_P(z_v), Z_STRLEN_P(z_v));
                                    break;
                                case 2:
                                    add_assoc_stringl(tmp_zval, "body", Z_STRVAL_P(z_v), Z_STRLEN_P(z_v));
                                    break;
                                default:
                                    php_error_docref(NULL TSRMLS_CC, E_WARNING,
                                                     "array key not found");
                                    return FAILURE;

                            }
                        }
                ZEND_HASH_FOREACH_END();
                add_next_index_zval(z_ret, tmp_zval);

            }ZEND_HASH_FOREACH_END();
    return SUCCESS;
}

PHP_DISQUE_API int disque_parse_show_response(zval *z_tab, zval *z_ret) {
    ulong idx;
    zval * z_ele;

    char *key = 0;
    int key_len = 0;

    ZEND_HASH_FOREACH_NUM_KEY_VAL(Z_ARRVAL_P(z_tab), idx, z_ele)
            {
                if (!z_ele) {
                    continue;
                }
                ZVAL_DEREF(z_ele);
                if ((idx & 1) == 0) {
                    key = Z_STRVAL_P(z_ele);
                    key_len = Z_STRLEN_P(z_ele);
                    key[key_len] = '\0';

                } else {
                    switch (Z_TYPE_P(z_ele)) {
                        case IS_ARRAY:
                            add_assoc_zval(z_ret, key, z_ele);
                            break;
                        case IS_LONG:
                            add_assoc_long(z_ret, key, Z_LVAL_P(z_ele));
                            break;
                        case IS_STRING:
                            add_assoc_stringl(z_ret, key, Z_STRVAL_P(z_ele), Z_STRLEN_P(z_ele));
                            break;
                        default:
                            php_error_docref(NULL TSRMLS_CC, E_WARNING,
                                             "array key not found");
                            return FAILURE;

                    }
                }

            }ZEND_HASH_FOREACH_END();

    return SUCCESS;
}

PHP_DISQUE_API void disque_show_response(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, zval *z_tab) {
    disque_mbulk_reply_raw(INTERNAL_FUNCTION_PARAM_PASSTHRU, disque_sock, z_tab);
    zval zv = {{0}}, *z_ret = &zv;
#if (PHP_MAJOR_VERSION < 7)
    MAKE_STD_ZVAL(z_ret);
#endif
    array_init(z_ret);

    if (return_value && Z_TYPE_P(return_value) == IS_ARRAY) {
        disque_parse_show_response(return_value, z_ret);
    }

    RETVAL_ZVAL(z_ret, 0, 1);
}

PHP_DISQUE_API void disque_parse_info_response(char *response, zval *z_ret) {
    char *cur, *pos;

    array_init(z_ret);

    cur = response;
    while (1) {
        /* skip comments and empty lines */
        if (*cur == '#' || *cur == '\r') {
            if ((cur = strstr(cur, _NL)) == NULL) {
                break;
            }
            cur += 2;
            continue;
        }

        /* key */
        if ((pos = strchr(cur, ':')) == NULL) {
            break;
        }
        char *key = cur;
        int key_len = pos - cur;
        key[key_len] = '\0';

        /* value */
        cur = pos + 1;
        if ((pos = strstr(cur, _NL)) == NULL) {
            break;
        }
        char *value = cur;
        int value_len = pos - cur;
        value[value_len] = '\0';

        double dval;
        zend_long lval;
        zend_uchar type = is_numeric_string(value, value_len, &lval, &dval, 0);
        if (type == IS_LONG) {
            add_assoc_long_ex(z_ret, key, key_len, lval);
        } else if (type == IS_DOUBLE) {
            add_assoc_double_ex(z_ret, key, key_len, dval);
        } else {
            add_assoc_stringl_ex(z_ret, key, key_len, value, value_len);
        }

        cur = pos + 2; /* \r, \n */
    }
}

PHP_DISQUE_API void disque_boolean_response_impl(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, zval *z_tab,
                                                 SuccessCallback success_callback) {

    char *response;
    int response_len;
    zend_bool ret = 0;

    if ((response = disque_sock_read(disque_sock, &response_len TSRMLS_CC)) != NULL) {
        ret = (*response == '+');
        efree(response);
    }

    if (ret && success_callback != NULL) {
        success_callback(disque_sock);
    }
    RETURN_BOOL(ret);
}

PHP_DISQUE_API void disque_boolean_response(INTERNAL_FUNCTION_PARAMETERS, DisqueSock *disque_sock, zval *z_tab) {
    disque_boolean_response_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, disque_sock, z_tab, NULL);
}


PHP_METHOD (Disque, __construct) {
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
        RETURN_FALSE;
    }
}

PHP_METHOD (Disque, __destruct) {
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
        RETURN_FALSE;
    }
}

PHP_METHOD (Disque, close) {
    DisqueSock *disque_sock = disque_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU);

    if (disque_sock && disque_sock_disconnect(disque_sock TSRMLS_CC)) {
        RETURN_TRUE;
    }
    RETURN_FALSE;
}

PHP_METHOD (Disque, hello) {
    DISQUE_PROCESS_KW_CMD("HELLO", disque_empty_cmd, disque_mbulk_reply_raw);
}

PHP_METHOD (Disque, connect) {
    if (disque_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0) == FAILURE) {
        RETURN_FALSE;
    } else {
        RETURN_TRUE;
    }
}

PHP_METHOD (Disque, pconnect) {
    if (disque_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1) == FAILURE) {
        RETURN_FALSE;
    } else {
        RETURN_TRUE;
    }
}

PHP_METHOD (Disque, auth) {
    DISQUE_PROCESS_CMD(auth, disque_boolean_response);
}

PHP_METHOD (Disque, ping) {
    DISQUE_PROCESS_KW_CMD("PING", disque_empty_cmd, disque_ping_response);
}

PHP_METHOD (Disque, info) {
    DISQUE_PROCESS_KW_CMD("INFO", disque_empty_cmd, disque_info_response);
}

PHP_METHOD (Disque, addJob) {
    DISQUE_PROCESS_KW_CMD("ADDJOB", disque_add_job_cmd, disque_ping_response);
}

PHP_METHOD (Disque, getJob) {
    DISQUE_PROCESS_KW_CMD("GETJOB", disque_get_job_cmd, disque_multi_job_respone);
}

PHP_METHOD (Disque, ackJob) {
    DISQUE_PROCESS_KW_CMD("ACKJOB", disque_ids_cmd, disque_long_response);
}

PHP_METHOD (Disque, fastAck) {
    DISQUE_PROCESS_KW_CMD("FASTACK", disque_ids_cmd, disque_long_response);
}

PHP_METHOD (Disque, delJob) {
    DISQUE_PROCESS_KW_CMD("DELJOB", disque_ids_cmd, disque_long_response);
}

PHP_METHOD (Disque, show) {
    DISQUE_PROCESS_KW_CMD("SHOW", disque_str_cmd, disque_show_response);
}

PHP_METHOD (Disque, qlen) {
    DISQUE_PROCESS_KW_CMD("QLEN", disque_str_cmd, disque_long_response);
}

PHP_METHOD (Disque, qpeek) {
    DISQUE_PROCESS_KW_CMD("QPEEK", disque_key_long_cmd, disque_multi_job_respone);
}

PHP_METHOD (Disque, enqueue) {
    DISQUE_PROCESS_KW_CMD("ENQUEUE", disque_ids_cmd, disque_long_response);
}

PHP_METHOD (Disque, dequeue) {
    DISQUE_PROCESS_KW_CMD("DEQUEUE", disque_ids_cmd, disque_long_response);
}

PHP_METHOD (Disque, working) {
    DISQUE_PROCESS_KW_CMD("WORKING", disque_str_cmd, disque_long_response);
}

PHP_METHOD (Disque, nack) {
    DISQUE_PROCESS_KW_CMD("NACK", disque_ids_cmd, disque_long_response);
}

PHP_METHOD (Disque, qstat) {
    DISQUE_PROCESS_KW_CMD("QSTAT", disque_str_cmd, disque_show_response);
}

PHP_METHOD (Disque, qscan) {
    DISQUE_PROCESS_KW_CMD("QSCAN", disque_qscan_cmd, disque_mbulk_reply_raw);
}

PHP_METHOD (Disque, jscan) {
    DISQUE_PROCESS_KW_CMD("JSCAN", disque_jscan_cmd, disque_mbulk_reply_raw);
}


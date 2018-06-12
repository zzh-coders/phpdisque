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
#include "ext/standard/info.h"

#include "php_disque.h"
#include <zend_exceptions.h>

#include "php_network.h"
#include <sys/types.h>

zend_class_entry *disque_ce;
zend_class_entry *disque_exception_ce;

ZEND_BEGIN_ARG_INFO_EX(arginfo_void, 0, 0, 0)
ZEND_END_ARG_INFO()

//定义参数为空的情况
ZEND_BEGIN_ARG_INFO_EX(arginfo_connect, 0, 0, 1)
                ZEND_ARG_INFO(0, host)
                ZEND_ARG_INFO(0, port)
                ZEND_ARG_INFO(0, timeout)
                ZEND_ARG_INFO(0, retry_interval)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_pconnect, 0, 0, 1)
                ZEND_ARG_INFO(0, host)
                ZEND_ARG_INFO(0, port)
                ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()


static zend_function_entry php_disque_functions[] = {
        PHP_ME(Disque, __construct, arginfo_void, ZEND_ACC_CTOR | ZEND_ACC_PUBLIC)
        PHP_ME(Disque, __destruct, arginfo_void, ZEND_ACC_DTOR | ZEND_ACC_PUBLIC)
        PHP_ME(Disque, connect, arginfo_connect, ZEND_ACC_CTOR | ZEND_ACC_PUBLIC)
        PHP_ME(Disque, pconnect, arginfo_pconnect, ZEND_ACC_DTOR | ZEND_ACC_PUBLIC)
        PHP_ME(Disque, close, arginfo_void, ZEND_ACC_CTOR | ZEND_ACC_PUBLIC)
        PHP_ME(Disque, hello, arginfo_void, ZEND_ACC_DTOR | ZEND_ACC_PUBLIC)
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


PHP_MINIT_FUNCTION (php_disque) {
    zend_class_entry disque_class_entry;
    zend_class_entry disque_exception_class_entry;
    zend_class_entry * exception_ce = NULL;

    /* Disque class */
    INIT_CLASS_ENTRY(disque_class_entry, "Disque", php_disque_functions);
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

PHP_MSHUTDOWN_FUNCTION (php_disque) {
    return SUCCESS;
}


PHP_RINIT_FUNCTION (php_disque) {
#if defined(COMPILE_DL_PHP_DISQUE) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION (php_disque) {
    return SUCCESS;
}

PHP_MINFO_FUNCTION (php_disque) {
    php_info_print_table_start();
    php_info_print_table_header(2, "Disque Support", "enabled");
    php_info_print_table_row(2, "Disque Version", PHP_DISQUE_VERSION);
    php_info_print_table_end();
}

zend_module_entry php_disque_module_entry = {
        STANDARD_MODULE_HEADER,
        "php_disque",
        NULL,
        PHP_MINIT(php_disque),
        PHP_MSHUTDOWN(php_disque),
        NULL, NULL,
        PHP_MINFO(php_disque),
        PHP_DISQUE_VERSION,
        STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PHP_DISQUE
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(php_disque)
#endif

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
                                      persistent_id, retry_interval, 0);
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
        disque_sock->watching = 0;

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
    if (disque_sock->pipeline_cmd) {
        efree(disque_sock->pipeline_cmd);
    }
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

PHP_DISQUE_API DisqueSock *disque_sock_create(char *host, int host_len, unsigned short port,
                                              double timeout, double read_timeout,
                                              int persistent, char *persistent_id,
                                              long retry_interval, zend_bool lazy_connect) {
    DisqueSock *disque_sock;

    disque_sock = ecalloc(1, sizeof(DisqueSock));
    disque_sock->host = zend_string_init(host, host_len, 0);
    disque_sock->stream = NULL;
    disque_sock->status = DISQUE_SOCK_STATUS_DISCONNECTED;
    disque_sock->watching = 0;
    disque_sock->retry_interval = retry_interval * 1000;
    disque_sock->persistent = persistent;
    disque_sock->lazy_connect = lazy_connect;
    disque_sock->persistent_id = NULL;

    if (persistent && persistent_id != NULL) {
        disque_sock->persistent_id = zend_string_init(persistent_id, strlen(persistent_id), 0);
    }

    disque_sock->port = port;
    disque_sock->timeout = timeout;
    disque_sock->read_timeout = read_timeout;

    disque_sock->head = NULL;
    disque_sock->current = NULL;

    disque_sock->pipeline_cmd = NULL;
    disque_sock->pipeline_len = 0;

    disque_sock->err = NULL;

    disque_sock->readonly = 0;
    disque_sock->tcp_keepalive = 0;

    return disque_sock;
}

static zend_always_inline DisqueSock *
disque_sock_get_instance(zval *id TSRMLS_DC, int no_throw) {
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

    if (disque_sock->lazy_connect) {
        disque_sock->lazy_connect = 0;
        if (disque_sock_server_open(disque_sock TSRMLS_CC) < 0) {
            return NULL;
        }
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
    php_printf("这是hello方法");
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
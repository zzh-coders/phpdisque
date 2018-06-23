//
// Created by zzh on 2018/6/16.
//

#ifndef PHP_DISQUE_COMMON_H
#define PHP_DISQUE_COMMON_H

#define IS_TIMEOUT_ARG(s, l) \
    (l == sizeof("timeout") - 1 && !strncasecmp(s, "timeout", l))
#define IS_REPLICATE_ARG(s, l) \
    (l == sizeof("replicate") - 1 && !strncasecmp(s,"replicate", l))
#define IS_DELAY_ARG(s, l) \
    (l == sizeof("delay") - 1 && !strncasecmp(s,"delay", l))
#define IS_RETRY_ARG(s, l) \
    (l == sizeof("retry") - 1 && !strncasecmp(s,"retry", l))
#define IS_TTL_ARG(s, l) \
    (l == sizeof("ttl") - 1 && !strncasecmp(s,"ttl", l))
#define IS_MAXLEN_ARG(s, l) \
    (l == sizeof("maxlen") - 1 && !strncasecmp(s,"maxlen", l))

#define IS_MINLEN_ARG(s, l) \
    (l == sizeof("minlen") - 1 && !strncasecmp(s,"minlen", l))

#define IS_ASYNC_ARG(s, l) \
    (l == sizeof("async") - 1 && !strncasecmp(s,"async", l))
#define IS_NOHANG_ARG(s, l) \
    (l == sizeof("nohang") - 1 && !strncasecmp(s,"nohang", l))
#define IS_COUNT_ARG(s, l) \
    (l == sizeof("count") - 1 && !strncasecmp(s,"count", l))
#define IS_WITHCOUNTERS_ARG(s, l) \
    (l == sizeof("withcounters") - 1 && !strncasecmp(s,"withcounters", l))

#define IS_IMPORTRATE_ARG(s, l) \
    (l == sizeof("importrate") - 1 && !strncasecmp(s,"importrate", l))

#define IS_BUSYLOOP_ARG(s, l) \
    (l == sizeof("busyloop") - 1 && !strncasecmp(s,"busyloop", l))

#define IS_QUEUE_ARG(s, l) \
    (l == sizeof("queue") - 1 && !strncasecmp(s,"queue", l))

#define IS_REPLY_ARG(s, l) \
    (l == sizeof("reply") - 1 && !strncasecmp(s,"reply", l))

#define IS_STATE_ARG(s, l) \
    (l == sizeof("state") - 1 && !strncasecmp(s,"state", l))

#endif //PHP_DISQUE_COMMON_H

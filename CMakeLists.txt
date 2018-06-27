cmake_minimum_required(VERSION 3.10)
project(phpdisque C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(phpdisque
        disque.c
        php_disque.h
        common.h)


#定义php源码路径，这里根据自己的真实路径来更改
set(PHP_SOURCE /Users/zzh/soft/php-7.1.18)
#引入php需要的扩展源码，这里也是根据自己需要的来更改
include_directories(${PHP_SOURCE}/main)
include_directories(${PHP_SOURCE}/Zend)
include_directories(${PHP_SOURCE}/sapi)
include_directories(${PHP_SOURCE}/pear)
include_directories(${PHP_SOURCE})

set(PHP_BIN_SOURCE /usr/local/php7-debug/bin)
add_custom_target(makefile COMMAND sudo make clean && sudo ${PHP_BIN_SOURCE}/phpize &&sudo ./configure --with-php-config=${PHP_BIN_SOURCE}/php-config && sudo make && sudo make install
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

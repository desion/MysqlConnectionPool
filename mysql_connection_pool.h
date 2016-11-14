/* mysql_connection_pool.h - A Mysql connection pool implementation
 *
 * Copyright (c) 2016-2017, Desion Wang <wdxin1322 at qq dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MYSQL_CONNECTION_POOL_H
#define __MYSQL_CONNECTION_POOL_H

#include <mysql/mysql.h>
#include <semaphore.h>
#include <pthread.h>
#include <deque>

#define MAX_SQL_LEN             4096

#define MAX_SETTING_STRING_LEN  256
#define MAX_POOL_SIZE           256

#define MCP_LOG(fmt, ...) {\
    fprintf(stderr, fmt, ##__VA_ARGS__);\
}

/* mysql connection setting option. */
typedef struct {
   char host[MAX_SETTING_STRING_LEN];
   int  port;
   char database[MAX_SETTING_STRING_LEN];
   char user[MAX_SETTING_STRING_LEN];
   char password[MAX_SETTING_STRING_LEN];
   char charset[MAX_SETTING_STRING_LEN];
   unsigned int  timeout;
} connectionSetting;

/* mysql connection struct */
typedef struct {
    connectionSetting   *ptrConnSetting;
    MYSQL               *sock;
    MYSQL               mysql;
    int                 res;
} mysqlConnection;


class MysqlConnectionPool{
public:
    /* init the connection pool */
    int initMysqlConnPool(const char* host,int port,const char* user,const char* password,const char* database);
    /* recycle the connection to pool. */
    void recycleConnection(mysqlConnection *conn);
    /* execute the mysql query */
    int executeSql(mysqlConnection *conn, const char* sql);
    /* fetch the connection from the pool. */
    mysqlConnection *fetchConnection();
    /* open mysql connection pool with connection num is coreConnNum */
    int openConnPool(int coreConnNum);
    /* set the charset of connection */
    void setCharsetOption(connectionSetting *connSetting, const char* charset);

public:
    MysqlConnectionPool(){
    }

    ~MysqlConnectionPool(){
        closeConnPool();
    }
private:
    int lockPool();
    /* close connection pool*/
    void closeConnPool();
private:
    int  connNum; // Number of connections
    sem_t  sem;
    std::deque<mysqlConnection*> connPool;
    connectionSetting *connSetting;
    pthread_mutex_t mutex;
};

#endif

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

#include <mysql.h>
#include <semaphore.h>
#include <pthread.h>
#include <deque>

#define MAX_SQL_LEN             4096

#define MAX_SETTING_STRING_LEN  256
#define MAX_POOL_SIZE           256
#define MAX_DATA_LEN            1024*100

#define conn_fetch_row(conn) mysql_fetch_row((conn)->res)

/* mysql connection setting option. */
typedef struct {
   char host[MAX_SETTING_STRING_LEN];
   int  port;
   char database[MAX_SETTING_STRING_LEN];
   char user[MAX_SETTING_STRING_LEN];
   char password[MAX_SETTING_STRING_LEN];
   unsigned int  timeout;
} connectionSetting;

/* mysql connection struct */
typedef struct {
    ConnectionSetting   *ptrConnSetting;
    MYSQL               *sock;
    MYSQL               mysql;
    MYSQL_RES           *res;
} mysqlConnection;


class MysqlConnectionPool{
public:
    //初始化连接池
    bool init(const char* section, const char* conf);
    /* recycle the connection to pool. */
    void recycleConnection(mysqlConnection *conn){
        conn_free_result(conn);
    }
    //查询
    int mysqlQuery(MysqlConnection *conn,char *sql, int *lockTime, unsigned int id);
    /* fetch the connection from the pool. */
    mysqlConnection *fetchConnection(){
        return connpool_getConn();
    }
    //迭代结果
    MysqlConnection* conn_next_result(MysqlConnection *conn);

public:
    MysqlConnectionPool(){
    }

    ~MysqlConnectionPool(){
        poolDestroy();
    }
private:
    void poolDestroy(){
        connpool_close();
    }
    //创建连接池
    int openConnPool(int connNum);
    //关闭连接池
    void closeConnPool();
    //执行sql，被conn_new_result内部调用
    MYSQL_RES *mysql_new_result(MYSQL *sock,char *sql, char* database, unsigned int id);
    //回收连接
    void conn_free_result(MysqlConnection *conn);
    //获取连接
    MysqlConnection *connpool_getConn();
    //通过配置打开连接
    void conn_open(MysqlConnection* conn, ConnectionSetting *s);
    //关闭连接
    void conn_close(MysqlConnection* conn);
    int conn_lock();
    //初始化mysql连接池配置
    int init_mysql(const char* section, const char* mysql_conf);
private:
    int  connNum; // Number of connections
    sem_t  sem;
    std::deque<mysqlConnection*> connPool;
    //配置管理
    connectionSetting *connSetting;
    //全局锁
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
};

#endif

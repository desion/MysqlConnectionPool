#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "mysql_connection_pool.h"

int MysqlConnectionPool::openConnPool(int coreConnNum) {
    if (coreConnNum > MAX_POOL_SIZE || coreConnNum <= 0){
        coreConnNum = MAX_POOL_SIZE;
    }
    pthread_mutex_init(&mutex, NULL);

    connNum = coreConnNum;
    sem_init(&sem, 0, connNum);

    for (int i = 0; i < connNum; i++) {
        mysqlConnection *conn = new mysqlConnection;
        conn->ptrConnSetting = connSetting;

        if (mysql_init(&conn->mysql) == NULL) {
            MCP_LOG("ERROR: mysql_init() %s\n", mysql_error(&conn->mysql));
            return 1;
        }

        mysql_options(&conn->mysql,MYSQL_OPT_CONNECT_TIMEOUT,&connSetting->timeout);
        mysql_options(&conn->mysql,MYSQL_OPT_READ_TIMEOUT,&connSetting->timeout);
        mysql_options(&conn->mysql,MYSQL_SET_CHARSET_NAME, connSetting->charset);

        conn->sock = mysql_real_connect(&conn->mysql, connSetting->host,
                connSetting->user, connSetting->password, connSetting->database, connSetting->port, NULL, CLIENT_MULTI_STATEMENTS);

        if (!conn->sock) {
            MCP_LOG("ERROR mysql_real_connect(): %s\n", mysql_error(& conn->mysql));
            return 1;
        }
        connPool.push_back(conn);
    }
    return 0;
}

void MysqlConnectionPool::closeConnPool() {
    pthread_mutex_destroy(&mutex);
    std::deque<mysqlConnection*>::iterator iter;
    for(iter = connPool.begin(); iter != connPool.end(); ++iter){
        mysqlConnection *conn = *iter;

        if (conn->sock!=NULL) {
            mysql_close(conn->sock);
            conn->sock=NULL;
        }
        delete conn;
    }
    connPool.clear();
    sem_destroy(&sem);
    free(connSetting);
}

int MysqlConnectionPool::lockPool() {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
       MCP_LOG("Function clock_gettime failed");
       return -1;
    }

    ts.tv_sec += 1;
    int ret=0;
    while ((ret = sem_timedwait(&sem, &ts)) == -1 && errno == EINTR)
        continue;       // Restart when interrupted by handler
    if (ret==-1) {
        if (errno == ETIMEDOUT) {
            MCP_LOG( "Timeout occurred in locking connection, \
                database is, pool connNum is %d",
                connNum);
        }
        else {
           MCP_LOG( "Unknown error in locking connection, \
                database is, pool connNum is %d",
                connNum);
        }
        return -2;
    }
    return 0;
}

mysqlConnection* MysqlConnectionPool::fetchConnection() {
    if (lockPool() != 0) {
        return NULL;
    }
    pthread_mutex_lock(&mutex);

    mysqlConnection* conn = connPool.front();
    connPool.pop_front();

    pthread_mutex_unlock(&mutex);
    return conn;
}

int MysqlConnectionPool::executeSql(mysqlConnection *conn, const char* sql) {
    if(NULL == conn || NULL == sql){
        return 1;
    }
    if (conn->sock) {
        conn->res = mysql_query(conn->sock, sql);
    } else {
        conn->res = 1;
    }
    //reconnect
    if (conn->res) {
        mysql_close(conn->sock);
        mysql_init(&(conn->mysql));

        connectionSetting *s = conn->ptrConnSetting;
        mysql_options(&conn->mysql,MYSQL_OPT_CONNECT_TIMEOUT,&s->timeout);
        mysql_options(&conn->mysql,MYSQL_OPT_READ_TIMEOUT,&s->timeout);
        mysql_options(&conn->mysql,MYSQL_SET_CHARSET_NAME, s->charset);

        conn->sock=mysql_real_connect(&(conn->mysql),
                s->host, s->user, s->password, s->database, s->port, NULL, CLIENT_MULTI_STATEMENTS);

        if (!conn->sock) {
            MCP_LOG("Failed to connect to database: Error: %s, database is %s\n",
                mysql_error(& conn->mysql), conn->ptrConnSetting->database);
        }

        conn->res = mysql_query(conn->sock, sql);
    }

    return conn->res;
}

//recycle connection
void MysqlConnectionPool::recycleConnection(mysqlConnection *conn) {
   pthread_mutex_lock(&mutex);

   connPool.push_back(conn);

   pthread_mutex_unlock(&mutex);

   sem_post(&sem);
   return ;
}


//初始化mysql
int MysqlConnectionPool::initMysqlConnPool(
        const char* host, 
        int port, 
        const char* user, 
        const char* password, 
        const char* database){
    if(NULL == host){
        MCP_LOG("ERROR: host is NULL");
        return 1;
    }
    if(NULL == user){
        MCP_LOG("ERROR: user is NULL");
        return 1;
    }
    if(NULL == password){
        MCP_LOG("ERROR: password is NULL");
        return 1;
    }
    if(NULL == database){
        MCP_LOG("ERROR: database is NULL");
        return 1;
    }
    connSetting = (connectionSetting*)malloc(sizeof(connectionSetting));
    assert(connSetting != NULL);
    memset(connSetting, 0, sizeof(connectionSetting));
    strncpy(connSetting->host, host, MAX_SETTING_STRING_LEN);
    strncpy(connSetting->user, user, MAX_SETTING_STRING_LEN);
    strncpy(connSetting->password, password, MAX_SETTING_STRING_LEN);
    connSetting->port = port;
    strncpy(connSetting->database, database, MAX_SETTING_STRING_LEN);
    //set the default charset utf-8
    sprintf(connSetting->charset, "utf8");
    //set default timeout 2s 
    connSetting->timeout = 2;
    return 0;
}

//set the connection charset
void MysqlConnectionPool::setCharsetOption(connectionSetting *connSetting, const char* charset){
    if(NULL != connSetting && NULL != charset){
        strncpy(connSetting->charset, charset, MAX_SETTING_STRING_LEN);
    }
}

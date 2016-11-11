#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include "mysql_connection_pool.h"

int MysqlConnectionPool::openConnPool(int connNum) {
    if (connNum > MAX_POOL_SIZE) {connNum = MAX_POOL_SIZE;}
    pthread_mutex_init(&mutex, NULL);

    connNum = connNum;
    sem_init(&sem, 0, connNum);

    for (int i=0; i<connNum; i++) {
        mysqlConnection *conn = new mysqlConnection;
        conn->pConnSetting = conn_setting;

        if (mysql_init(&conn->mysql) == NULL) {
                LOG_ERROR("ERROR: mysql_init() %s\n", mysql_error(&conn->mysql));
                return 1;
            }

        mysql_options(&conn->mysql,MYSQL_OPT_CONNECT_TIMEOUT,&conn_setting->timeout);
        mysql_options(&conn->mysql,MYSQL_OPT_READ_TIMEOUT,&conn_setting->timeout);
        mysql_options(&conn->mysql,MYSQL_SET_CHARSET_NAME, "utf8mb4");

        conn->sock=mysql_real_connect(&conn->mysql,conn_setting->host,
            conn_setting->user, conn_setting->password, conn_setting->database, conn_setting->port, NULL, CLIENT_MULTI_STATEMENTS);

        LOG_INFO("Mysql\tHost:%s User:%s Password:%s DataBase:%s Port:%d", conn_setting->host, conn_setting->user, conn_setting->password, conn_setting->database, conn_setting->port);
        if (!conn->sock) {
            LOG_ERROR("ERROR mysql_real_connect(): %s\n", mysql_error(& conn->mysql) );
            return 1;
        }
        conn->res=NULL;
            connPool.push_back(conn);
    }

    return 0;
}
void MysqlManager::connpool_close() {
    pthread_mutex_destroy(&mutex);
    std::deque<MysqlConnection*>::iterator iter;
    for(iter = connPool.begin(); iter != connPool.end(); ++iter){
       MysqlConnection *conn = *iter;

       if (conn->res!=NULL) {
          mysql_free_result(conn->res);
          conn->res = NULL;
       }
       if (conn->sock!=NULL) {
           mysql_close(conn->sock);
           conn->sock=NULL;
       }
    }

    connPool.clear();

    sem_destroy(&sem);

    free(conn_setting);
}

/***
* 生成者消费者锁的实现
*/
int MysqlManager::conn_lock() {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
       LOG_INFO("Function clock_gettime failed");
       return -1;
    }

    ts.tv_sec += 1;
    int ret=0;
    while ((ret = sem_timedwait(&sem, &ts)) == -1 && errno == EINTR)
        continue;       // Restart when interrupted by handler
    if (ret==-1) {
        if (errno == ETIMEDOUT) {
            LOG_WARN( "Timeout occurred in locking connection, \
                database is, pool connNum is %d",
                connNum);
        }
        else {
           LOG_WARN( "Unknown error in locking connection, \
                database is, pool connNum is %d",
                connNum);
        }
return -2;
    }
    return 0;
}
/***
* 获取mysql连接
*/
MysqlConnection * MysqlManager::connpool_getConn() {
    if (conn_lock() != 0) {return NULL;}
    pthread_mutex_lock(&mutex);

    MysqlConnection* conn = connPool.front();
    connPool.pop_front();

    pthread_mutex_unlock(&mutex);
    return conn;
}

MYSQL_RES * MysqlManager::mysql_new_result(MYSQL *sock, char *sql, char* database, unsigned int id) {
    MYSQL_RES *res;

    if (sock == NULL || sql == NULL) {
        return NULL;
    }

    if(mysql_query(sock,sql)) {
        LOG_WARN( "Failed in mysql_query, Error: %s\n", mysql_error(sock));
        LOG_WARN( "Failed in mysql_query, sql is %s, database is %s, id is %u\n", sql, database, id);
        return NULL;
    }
    if(mysql_errno(sock) != 0){
        LOG_WARN( "Failed in mysql_query, Error: %s\n", mysql_error(sock));
        return NULL;
    }

    if (!(res=mysql_store_result(sock))) {
        LOG_WARN( "Failed in mysql_store_result, Error: %s\n", mysql_error(sock));
        return NULL;
    }

    return res;
}

int MysqlManager::mysqlquery(MysqlConnection *conn,char sql[],int *lockTime, unsigned int id) {
    if (lockTime!=NULL) {*lockTime=time(NULL);}

    if (conn->sock) {
        conn->res = mysql_new_result(conn->sock,sql, conn->pConnSetting->database, id);
    } else {
        conn->res = NULL;
    }
    //进行重连
    if (conn->res == NULL) {
        mysql_close(conn->sock);
        mysql_init(&(conn->mysql));

        ConnectionSetting *s=conn->pConnSetting;
        mysql_options(&conn->mysql,MYSQL_OPT_CONNECT_TIMEOUT,&s->timeout);
        mysql_options(&conn->mysql,MYSQL_OPT_READ_TIMEOUT,&s->timeout);
        mysql_options(&conn->mysql,MYSQL_SET_CHARSET_NAME, "utf8mb4");

        conn->sock=mysql_real_connect(&(conn->mysql),
                s->host, s->user, s->password, s->database, s->port, NULL, CLIENT_MULTI_STATEMENTS);

        if (!conn->sock) {
            LOG_WARN("Failed to connect to database: Error: %s, database is %s, id is %u\n",
                mysql_error(& conn->mysql), conn->pConnSetting->database, id);
        }

        conn->res = mysql_new_result(conn->sock, sql, conn->pConnSetting->database, id);
    }

    if (conn->res) {
        return 1 ;
    } else {
       return  0;
    }
}
/***
* 迭代结果
*/
MysqlConnection* MysqlManager::conn_next_result(MysqlConnection *conn) {
   if (conn->res !=NULL ) {
       mysql_free_result(conn->res);
       conn->res = NULL;
   }

   if (0 != mysql_next_result(conn->sock)) return NULL;
   conn->res = mysql_store_result(conn->sock);
   return conn;
}

void MysqlManager::conn_free_result(MysqlConnection *conn) {
   if (conn->res !=NULL ) {
       mysql_free_result(conn->res);
       conn->res = NULL;
   }

   pthread_mutex_lock(&mutex);

   connPool.push_back(conn);

   pthread_mutex_unlock(&mutex);

   sem_post(&sem);
   return ;
}

/***
* 连接池管理初始化
*/
bool MysqlManager::init(const char* section, const char* mysql_conf){
    if(init_mysql(section, mysql_conf) != 0){
        return false;
    }
    return true;
}


//初始化mysql
int MysqlManager::init_mysql(const char* section, const char* mysql_conf){
    int pool_num = 10;
    conn_setting = (ConnectionSetting*)malloc(sizeof(ConnectionSetting));
    assert(conn_setting != NULL);
    if(!read_profile_string(section, "HOST", conn_setting->host, MAX_SETTING_STRING_LEN, "", mysql_conf)){
        LOG_ERROR("load conn_setting HOST fail!");
        goto ERROR;
    }
    if(!read_profile_string(section, "DATABASE", conn_setting->database, MAX_SETTING_STRING_LEN, "", mysql_conf)){
        LOG_ERROR("load conn_setting DATABASE fail!");
        goto ERROR;
    }
    if(!read_profile_string(section, "USER", conn_setting->user, MAX_SETTING_STRING_LEN, "", mysql_conf)){
        LOG_ERROR("load conn_setting USER fail!");
        goto ERROR;
    }
    if(!read_profile_string(section, "PASSWORD", conn_setting->password, MAX_SETTING_STRING_LEN, "", mysql_conf)){
        LOG_ERROR("load conn_setting HOST fail!");
        goto ERROR;
    }
    conn_setting->port = read_profile_int(section, "PORT", 3306, mysql_conf);
conn_setting->timeout = read_profile_int(section, "TIMEOUT", 3, mysql_conf);
    pool_num = read_profile_int(section, "POOLNUM", 3, mysql_conf);
    if(connpool_open(pool_num) != 0){
        LOG_ERROR("create mysql conn pool fail!");
        free(conn_setting);
        return -2;
    }
    return 0;
ERROR:
    free(conn_setting);
    return -1;
}

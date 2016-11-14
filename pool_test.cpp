#include <stdio.h>
#include <assert.h>
#include "mysql_connection_pool.h"

int main(int argc, char* argv[]){
    MysqlConnectionPool *pool = new MysqlConnectionPool();
    int ret = 0;
    ret = pool->initMysqlConnPool("localhost", 3306, "username", "password", "mysql");
    assert(ret == 0);
    ret = pool->openConnPool(10); 
    assert(ret == 0);
    int num = 100;
    MYSQL_RES *res_ptr;
    int i, j;
    MYSQL_ROW sqlrow;
    while(num > 0){
        mysqlConnection *mysqlConn = pool->fetchConnection();
        assert(mysqlConn != NULL);
        pool->executeSql(mysqlConn, "select 1 from user");
        res_ptr = mysql_store_result(mysqlConn->sock);
        if(res_ptr) {               
            printf("%lu Rows\n",(unsigned long)mysql_num_rows(res_ptr));   
            j = mysql_num_fields(res_ptr);          
            while((sqlrow = mysql_fetch_row(res_ptr))){
                for(i = 0; i < j; i++)
                    printf("%s\t", sqlrow[i]);
                printf("\n");          
            }
            if (mysql_errno(mysqlConn->sock)) {
                fprintf(stderr,"Retrive error:%s\n",mysql_error(mysqlConn->sock));               
            }
        }
        mysql_free_result(res_ptr);       
        pool->recycleConnection(mysqlConn);
        num--;
    }
    delete pool;
    return 0;
}


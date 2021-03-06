#include <db.h>
#include <string.h>

#define DEFAULT_DB_PATH "/usr/local/nginx/logs/pair.db"
#define NONE 0
#define GET 1
#define PUT 2
#define LIST 3


int
main(int argc, char **argv)
{
    FILE *err_file = stderr;
    int ret;
    u_int32_t db_flags;
    u_int32_t op = NONE;
    DB *dbp;
    char *db_path = DEFAULT_DB_PATH;
    char *prog_name = argv[0];
    char *url;
    u_int32_t cache_time;

    if (argc < 3) {
        fprintf(err_file, "Usage: %s <bdb_path> -r|-w|-l <url_prefix> [cache_time(s)]\n", argv[0]);
        return -1;
    }

    db_path = argv[1];

    if (!strcmp(argv[2], "-w")) {
        if (argc != 5) {
            fprintf(err_file, "Usage: %s <bdb_path> -w <url_prefix> <cache_time(s)>\n", argv[0]);
            return -1;
        }
        op = PUT;
        url = argv[3];
        cache_time = atoi(argv[4]);
    }

    if (!strcmp(argv[2], "-r")) {
        if(argc != 4) {
            fprintf(err_file, "Usage: %s <bdb_path> -r <url_prefix>\n", argv[0]);
            return -1;
        }
        op = GET;
        url = argv[3];
    }

    if (!strcmp(argv[2], "-l")) {
        if (argc != 3) {
            fprintf(err_file, "Usage: %s <bdb_path> -l\n", argv[0]);
            return -1;
        }
        op = LIST;
        char buf[1024];
        memset(buf, 0, 1024);
        url = buf;
    }

    if (op == NONE) {
        fprintf(err_file, "Usage: %s <bdb_path> -r|-w|-l <url_prefix> [cache_time(s)]\n", argv[0]);
        return -1;
    }

    /* create db */
    ret = db_create(&dbp, NULL, 0);
    if (ret) {
        fprintf(err_file, "%s: %s\n", prog_name, db_strerror(ret));
        return ret;
    }

    /* set up error handling */
    dbp->set_errfile(dbp, err_file);
    dbp->set_errpfx(dbp, prog_name);

    /* set the open flags */
    db_flags = DB_CREATE;

    ret = dbp->open(dbp,
                    NULL,
                    db_path,
                    NULL,
                    DB_BTREE,
                    db_flags,
                    0);
    if (ret) {
        dbp->err(dbp, ret, "Database '%s' open failed.", db_path);
        return ret;
    }

    DBT key, data;
    memset(&key, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));

    key.data = url;
    key.flags = DB_DBT_USERMEM;
    key.ulen = 1024;
    data.data = &cache_time;
    data.ulen = sizeof(cache_time);
    data.flags = DB_DBT_USERMEM;

    if (op == PUT) {
        key.size = strlen(url);
        data.size = sizeof(cache_time);

        ret = dbp->put(dbp, NULL, &key, &data, 0);
        if (ret) {
            dbp->err(dbp, ret, "Put %s<->%u pair failed.", key.data, *((u_int32_t *)data.data));
        } else {
            printf("Put %s<->%u(s) pair successfully.\n", (char *)key.data, *((u_int32_t *)data.data));
        }
    } else if (op == GET) {
        key.size = strlen(url);

        ret = dbp->get(dbp, NULL, &key, &data, 0);
        if (ret) {
            dbp->err(dbp, ret, "Get %s pair failed.", key.data);
        } else {
            printf("Get %s<->%u(s) pair successfully.\n", (char *)key.data, *((u_int32_t *)data.data));
        }
    } else if (op == LIST) {
        DBC *cursorp;
        ret = dbp->cursor(dbp, NULL, &cursorp, 0);
        if (ret) {
            dbp->err(dbp, ret, "Open cursor failed.");
        } else {
            /* Iterate over the database, retrieving each record in turn. */
            printf("%*s%*s", 30,"Uri", 15, "Time(s)\n");
            while ((ret = cursorp->get(cursorp, &key, &data, DB_NEXT)) == 0) {
                /* Do interesting things with the DBTs here. */
                ((char *)key.data)[key.size] = '\0';
                printf("%*s%*d\n", 30, (char *)key.data, 15, *((u_int32_t *)data.data));
            }
            if (ret != DB_NOTFOUND) {
                /* Error handling goes here */
                dbp->err(dbp, ret, "Iterate the record failed.");
            }

            if (cursorp != NULL)
                cursorp->close(cursorp);
        }
    }

    if (dbp != NULL) {
        ret = dbp->close(dbp, 0);
        if (ret) {
            fprintf(err_file,
                    "Database '%s' close failed.", db_path);
        }
    }
}

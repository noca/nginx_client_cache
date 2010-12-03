#include <db.h>
#include <string.h>

#define DEFAULT_DB_PATH "/usr/local/nginx/logs/pair.db"
#define NONE 0
#define GET 1
#define PUT 2


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

    if (argc == 1) {
        fprintf(err_file, "Usage: %s -r|-w <url_prefix> [cache_time]\n", argv[0]);
        return -1;
    }

    if (!strcmp(argv[1], "-w")) {
        if (argc != 4) {
            fprintf(err_file, "Usage: %s -w <url_prefix> <cache_time>\n", argv[0]);
            return -1;
        }
        op = PUT;
        url = argv[2];
        cache_time = atoi(argv[3]);
    }

    if (!strcmp(argv[1], "-r")) {
        if(argc != 3) {
            fprintf(err_file, "Usage: %s -r <url_prefix>\n", argv[0]);
            return -1;
        }
        op = GET;
        url = argv[2];
    }

    if (op == NONE) {
        fprintf(err_file, "Usage: %s -r|-w <url_prefix> [cache_time]\n", argv[0]);
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
    key.size = strlen(url);
    data.data = &cache_time;

    if (op == PUT) {
        data.size = sizeof(cache_time);

        ret = dbp->put(dbp, NULL, &key, &data, 0);
        if (ret) {
            dbp->err(dbp, ret, "Put %s<->%u pair failed.", key.data, *((u_int32_t *)data.data));
        } else {
            printf("Put %s<->%u pair successfully.\n", (char *)key.data, *((u_int32_t *)data.data));
        }
    } else if (op == GET) {
        data.ulen = sizeof(cache_time);
        data.flags = DB_DBT_USERMEM;

        ret = dbp->get(dbp, NULL, &key, &data, 0);
        if (ret) {
            dbp->err(dbp, ret, "Get %s pair failed.", key.data);
        } else {
            printf("Get %s<->%u pair successfully.\n", (char *)key.data, *((u_int32_t *)data.data));
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

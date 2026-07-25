/*
 * pbx3 tenant SQLite — Phase 2.2.
 * Not Asterisk AstDB (DBGet/DBPut/DBDel via AGI DATABASE *).
 *
 * Include after MAX_SQL_* and cluster_cfg_t (see pbx3cagi.h).
 */
#ifndef _AGI_SQLITE_H
#define _AGI_SQLITE_H

extern char rescols[MAX_SQL_COLS][MAX_SQL_CLEN];
extern cluster_cfg_t g_cluster_cfg;

/* Prepared SELECT: sql has exactly one ? (bind1) or two ? in order (bind2).
 * Results land in rescols[]; return value is &rescols[0][0] (or "-1" on open fail).
 */
char *sqlQueryBind1(const char *sql, const char *arg1);
char *sqlQueryBind2(const char *sql, const char *arg1, const char *arg2);

int load_cluster_cfg(const char *cluster_pkey, cluster_cfg_t *cfg);

#endif /* _AGI_SQLITE_H */

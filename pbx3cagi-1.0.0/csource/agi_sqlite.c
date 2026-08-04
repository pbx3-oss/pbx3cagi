/*
 * pbx3 tenant SQLite access (Phase 2.2).
 * Asterisk AstDB (DBGet/DBPut/DBDel) stays in pbx3cagi.c — AGI DATABASE *, not this module.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pbx3cagi.h"
#include "agi_sqlite.h"
#include "cagi.h"
#include "bsd_compat.h"
#include "sqlite3.h"

/* Defined in pbx3cagi.c — used for verbose/debug and lock Wait. */
extern AGI_TOOLS agi;
extern AGI_CMD_RESULT res;
extern char vmsg[255];
extern long debug;

char rescols[MAX_SQL_COLS][MAX_SQL_CLEN];

cluster_cfg_t g_cluster_cfg;
static void cluster_cfg_apply_defaults(cluster_cfg_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->abstimeout_sec = 14400;
    strlcpy(cfg->voipmax_str, "30", sizeof(cfg->voipmax_str));
    strlcpy(cfg->callrecord_1, "None", sizeof(cfg->callrecord_1));
    strlcpy(cfg->allowhashxfer, "enabled", sizeof(cfg->allowhashxfer));
    strlcpy(cfg->playbeep, "YES", sizeof(cfg->playbeep));
    strlcpy(cfg->playbusy, "YES", sizeof(cfg->playbusy));
    strlcpy(cfg->playcongested, "YES", sizeof(cfg->playcongested));
    strlcpy(cfg->playtransfer, "YES", sizeof(cfg->playtransfer));
    strlcpy(cfg->voiceinstr, "YES", sizeof(cfg->voiceinstr));
    strlcpy(cfg->int_ring_delay, "20", sizeof(cfg->int_ring_delay));
    strlcpy(cfg->maxin_str, "30", sizeof(cfg->maxin_str));
    strlcpy(cfg->ringdelay_str, "20", sizeof(cfg->ringdelay_str));
    strlcpy(cfg->lterm_str, "NO", sizeof(cfg->lterm_str));
    strlcpy(cfg->cfwd_progress, "enabled", sizeof(cfg->cfwd_progress));
    strlcpy(cfg->cfwd_answer, "enabled", sizeof(cfg->cfwd_answer));
    strlcpy(cfg->ivr_key_wait, "6", sizeof(cfg->ivr_key_wait));
    strlcpy(cfg->ivr_digit_wait_str, "6000", sizeof(cfg->ivr_digit_wait_str));
    strlcpy(cfg->syspass, "4444", sizeof(cfg->syspass));
    strlcpy(cfg->spy_pass, "3333", sizeof(cfg->spy_pass));
    strlcpy(cfg->chanmax_str, "3", sizeof(cfg->chanmax_str));
    strlcpy(cfg->usemohcustom, "NO", sizeof(cfg->usemohcustom));
    strlcpy(cfg->masteroclo, "AUTO", sizeof(cfg->masteroclo));
    cfg->oclo[0] = '\0';
    cfg->routeoverride[0] = '\0';
    cfg->sched_mode[0] = '\0';
    cfg->holiday_force_dest[0] = '\0';
}

static sqlite3 *g_sqlite_handle = NULL;

static const char *sqlitedb_path(void)
{
    const char *env = getenv("PBX3CAGI_SQLITE_DB");
    if (env != NULL && env[0] != '\0')
    {
        return env;
    }
    return SQLITEDB;
}

static void sqlCloseSharedHandle(void)
{
    if (g_sqlite_handle != NULL)
    {
        sqlite3_close(g_sqlite_handle);
        g_sqlite_handle = NULL;
    }
}

static sqlite3 *sqlGetSharedHandle(void)
{
    int retval;

    if (g_sqlite_handle != NULL)
    {
        return g_sqlite_handle;
    }

    retval = sqlite3_open(sqlitedb_path(), &g_sqlite_handle);
    if (retval)
    {
        if (g_sqlite_handle != NULL)
        {
            sqlite3_close(g_sqlite_handle);
            g_sqlite_handle = NULL;
        }
        snprintf(vmsg, sizeof(vmsg), "Database connection failed, retval is %i", retval);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return NULL;
    }

    atexit(sqlCloseSharedHandle);
    return g_sqlite_handle;
}

static void escape_sql_literal(char *out, size_t outlen, const char *in)
{
    size_t o = 0;
    size_t i;

    for (i = 0; in[i] != '\0' && o + 1 < outlen; i++)
    {
        if (in[i] == '\'')
        {
            if (o + 2 >= outlen)
            {
                break;
            }
            out[o++] = '\'';
            out[o++] = '\'';
        }
        else
        {
            out[o++] = in[i];
        }
    }
    out[o] = '\0';
}

int load_cluster_cfg(const char *cluster_pkey, cluster_cfg_t *cfg)
{
    char esc[128];
    char query[1400];
    sqlite3 *handle = NULL;
    sqlite3_stmt *stmt = NULL;
    int retval;
    int i;

    cluster_cfg_apply_defaults(cfg);

    if (cluster_pkey == NULL || cluster_pkey[0] == '\0')
    {
        DebugFunctionMsg(__FUNCTION__, "load_cluster_cfg: empty cluster pkey");
        return -1;
    }

    escape_sql_literal(esc, sizeof(esc), cluster_pkey);
    snprintf(
        query, sizeof(query),
        "SELECT abstimeout, voip_max, allow_hash_xfer, play_beep, play_busy, play_congested, "
        "play_transfer, voice_instr, bounce_alert, blind_busy, int_ring_delay, maxin, ringdelay, lterm, "
        "cfwd_progress, cfwd_answer, ivr_key_wait, ivr_digit_wait, syspass, spy_pass, dynamicfeatures, "
        "clusterclid, chanmax, usemohcustom, callrecord_1, masteroclo, oclo, routeoverride, "
        "sched_mode, holiday_force_dest, "
        "fqdn, cname, domain, shortuid "
        "FROM cluster WHERE pkey='%s' OR shortuid='%s'",
        esc, esc);

    handle = sqlGetSharedHandle();
    if (handle == NULL)
    {
        DebugFunctionMsg(__FUNCTION__, "load_cluster_cfg: sqlite open failed");
        return -1;
    }

    for (i = 0; i < 3; i++)
    {
        retval = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
        if (retval == SQLITE_OK)
        {
            break;
        }
        if (retval == SQLITE_LOCKED || retval == SQLITE_BUSY)
        {
            AGITool_exec(&agi, &res, "Wait", "0.5");
        }
        else
        {
            break;
        }
    }

    if (retval != SQLITE_OK)
    {
        snprintf(vmsg, sizeof(vmsg), "load_cluster_cfg: prepare failed %i", retval);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return -1;
    }

    retval = sqlite3_step(stmt);
    if (retval != SQLITE_ROW)
    {
        snprintf(vmsg, sizeof(vmsg), "load_cluster_cfg: no cluster row for pkey=%s", cluster_pkey);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        sqlite3_finalize(stmt);
        return -1;
    }

    cfg->abstimeout_sec = sqlite3_column_type(stmt, 0) == SQLITE_NULL ? 14400 : sqlite3_column_int(stmt, 0);
    {
        int vm = sqlite3_column_type(stmt, 1) == SQLITE_NULL ? 30 : sqlite3_column_int(stmt, 1);
        snprintf(cfg->voipmax_str, sizeof(cfg->voipmax_str), "%d", vm);
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 2);
        strlcpy(cfg->allowhashxfer, t ? (const char *)t : "enabled", sizeof(cfg->allowhashxfer));
    }
    {
        int v = sqlite3_column_type(stmt, 3) == SQLITE_NULL ? 1 : sqlite3_column_int(stmt, 3);
        strlcpy(cfg->playbeep, v ? "YES" : "NO", sizeof(cfg->playbeep));
    }
    {
        int v = sqlite3_column_type(stmt, 4) == SQLITE_NULL ? 1 : sqlite3_column_int(stmt, 4);
        strlcpy(cfg->playbusy, v ? "YES" : "NO", sizeof(cfg->playbusy));
    }
    {
        int v = sqlite3_column_type(stmt, 5) == SQLITE_NULL ? 1 : sqlite3_column_int(stmt, 5);
        strlcpy(cfg->playcongested, v ? "YES" : "NO", sizeof(cfg->playcongested));
    }
    {
        int v = sqlite3_column_type(stmt, 6) == SQLITE_NULL ? 1 : sqlite3_column_int(stmt, 6);
        strlcpy(cfg->playtransfer, v ? "YES" : "NO", sizeof(cfg->playtransfer));
    }
    {
        int vi = sqlite3_column_type(stmt, 7) == SQLITE_NULL ? 1 : sqlite3_column_int(stmt, 7);
        strlcpy(cfg->voiceinstr, (vi == 0) ? "NO" : "YES", sizeof(cfg->voiceinstr));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 8);
        strlcpy(cfg->bounce_alert, t ? (const char *)t : "", sizeof(cfg->bounce_alert));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 9);
        strlcpy(cfg->blind_busy, t ? (const char *)t : "", sizeof(cfg->blind_busy));
    }
    {
        int v = sqlite3_column_type(stmt, 10) == SQLITE_NULL ? 20 : sqlite3_column_int(stmt, 10);
        snprintf(cfg->int_ring_delay, sizeof(cfg->int_ring_delay), "%d", v);
    }
    {
        int v = sqlite3_column_type(stmt, 11) == SQLITE_NULL ? 30 : sqlite3_column_int(stmt, 11);
        snprintf(cfg->maxin_str, sizeof(cfg->maxin_str), "%d", v);
    }
    {
        int v = sqlite3_column_type(stmt, 12) == SQLITE_NULL ? 20 : sqlite3_column_int(stmt, 12);
        snprintf(cfg->ringdelay_str, sizeof(cfg->ringdelay_str), "%d", v);
    }
    {
        int lt = sqlite3_column_type(stmt, 13) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 13);
        strlcpy(cfg->lterm_str, lt ? "YES" : "NO", sizeof(cfg->lterm_str));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 14);
        strlcpy(cfg->cfwd_progress, t ? (const char *)t : "enabled", sizeof(cfg->cfwd_progress));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 15);
        strlcpy(cfg->cfwd_answer, t ? (const char *)t : "enabled", sizeof(cfg->cfwd_answer));
    }
    {
        int v = sqlite3_column_type(stmt, 16) == SQLITE_NULL ? 6 : sqlite3_column_int(stmt, 16);
        snprintf(cfg->ivr_key_wait, sizeof(cfg->ivr_key_wait), "%d", v);
    }
    {
        int v = sqlite3_column_type(stmt, 17) == SQLITE_NULL ? 6000 : sqlite3_column_int(stmt, 17);
        snprintf(cfg->ivr_digit_wait_str, sizeof(cfg->ivr_digit_wait_str), "%d", v);
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 18);
        strlcpy(cfg->syspass, t ? (const char *)t : "4444", sizeof(cfg->syspass));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 19);
        strlcpy(cfg->spy_pass, t ? (const char *)t : "3333", sizeof(cfg->spy_pass));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 20);
        strlcpy(cfg->dynamicfeatures, t ? (const char *)t : "", sizeof(cfg->dynamicfeatures));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 21);
        strlcpy(cfg->clusterclid, t ? (const char *)t : "", sizeof(cfg->clusterclid));
    }
    {
        int v = sqlite3_column_type(stmt, 22) == SQLITE_NULL ? 3 : sqlite3_column_int(stmt, 22);
        snprintf(cfg->chanmax_str, sizeof(cfg->chanmax_str), "%d", v);
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 23);
        strlcpy(cfg->usemohcustom, t ? (const char *)t : "NO", sizeof(cfg->usemohcustom));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 24);
        strlcpy(cfg->callrecord_1, t ? (const char *)t : "None", sizeof(cfg->callrecord_1));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 25);
        strlcpy(cfg->masteroclo, t ? (const char *)t : "AUTO", sizeof(cfg->masteroclo));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 26);
        strlcpy(cfg->oclo, t ? (const char *)t : "", sizeof(cfg->oclo));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 27);
        strlcpy(cfg->routeoverride, t ? (const char *)t : "", sizeof(cfg->routeoverride));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 28);
        strlcpy(cfg->sched_mode, t ? (const char *)t : "", sizeof(cfg->sched_mode));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 29);
        strlcpy(cfg->holiday_force_dest, t ? (const char *)t : "", sizeof(cfg->holiday_force_dest));
    }
    {
        /* Prefer fqdn, then cname, else shortuid.domain (phones register as user@tenant.fqdn). */
        const unsigned char *fq = sqlite3_column_text(stmt, 30);
        const unsigned char *cn = sqlite3_column_text(stmt, 31);
        const unsigned char *dom = sqlite3_column_text(stmt, 32);
        const unsigned char *su = sqlite3_column_text(stmt, 33);
        cfg->fqdn[0] = '\0';
        if (fq != NULL && fq[0] != '\0')
        {
            strlcpy(cfg->fqdn, (const char *)fq, sizeof(cfg->fqdn));
        }
        else if (cn != NULL && cn[0] != '\0')
        {
            strlcpy(cfg->fqdn, (const char *)cn, sizeof(cfg->fqdn));
        }
        else if (su != NULL && su[0] != '\0' && dom != NULL && dom[0] != '\0')
        {
            snprintf(cfg->fqdn, sizeof(cfg->fqdn), "%s.%s", (const char *)su, (const char *)dom);
        }
    }

    cfg->loaded = 1;
    sqlite3_finalize(stmt);
    return 0;
}


static char *sqlQueryBindInternal(const char *sql, const char *arg1, const char *arg2, int bind_count)
{
    char *pVal = &rescols[0][0];
    int retval, i;

    DebugFunctionTrace(__FUNCTION__);

    sqlite3 *handle = sqlGetSharedHandle();
    if (handle == NULL)
    {
        return "-1";
    }

    if (debug)
    {
        snprintf(vmsg, sizeof(vmsg), "Executing prepared SQL %s", sql);
        DebugFunctionMsg(__FUNCTION__, vmsg);
    }

    sqlite3_stmt *stmt = NULL;
    for (i = 0; i < 3; i++)
    {
        retval = sqlite3_prepare_v2(handle, sql, -1, &stmt, 0);
        if (retval == SQLITE_OK)
        {
            break;
        }
        if (retval == SQLITE_LOCKED || retval == SQLITE_BUSY)
        {
            DebugFunctionMsg(__FUNCTION__, "Database LOCK! retry in .5s");
            AGITool_exec(&agi, &res, "Wait", "0.5");
        }
        else
        {
            break;
        }
    }

    if (retval != SQLITE_OK)
    {
        snprintf(vmsg, sizeof(vmsg), "Prepared SQL failed, retval=%i, sql=%s", retval, sql);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return pVal;
    }

    if (bind_count >= 1)
    {
        retval = sqlite3_bind_text(stmt, 1, arg1 ? arg1 : "", -1, SQLITE_TRANSIENT);
        if (retval != SQLITE_OK)
        {
            snprintf(vmsg, sizeof(vmsg), "Bind #1 failed, retval=%i, sql=%s", retval, sql);
            DebugFunctionMsg(__FUNCTION__, vmsg);
            sqlite3_finalize(stmt);
            return pVal;
        }
    }
    if (bind_count >= 2)
    {
        retval = sqlite3_bind_text(stmt, 2, arg2 ? arg2 : "", -1, SQLITE_TRANSIENT);
        if (retval != SQLITE_OK)
        {
            snprintf(vmsg, sizeof(vmsg), "Bind #2 failed, retval=%i, sql=%s", retval, sql);
            DebugFunctionMsg(__FUNCTION__, vmsg);
            sqlite3_finalize(stmt);
            return pVal;
        }
    }

    retval = sqlite3_step(stmt);

    if (debug)
    {
        snprintf(vmsg, sizeof(vmsg), "Prepared SQL returned %i columns", sqlite3_column_count(stmt));
        DebugFunctionMsg(__FUNCTION__, vmsg);
    }

    if (sqlite3_column_count(stmt) > 0)
    {
        for (i = 0; i < sqlite3_column_count(stmt); i++)
        {
            const unsigned char *t = sqlite3_column_text(stmt, i);
            strlcpy(rescols[i], t ? (const char *)t : "", sizeof(rescols[i]));
            if (debug)
            {
                snprintf(vmsg, sizeof(vmsg), "Prepared col[%i] value='%s'", i, rescols[i]);
                DebugFunctionMsg(__FUNCTION__, vmsg);
            }
        }
    }

    sqlite3_finalize(stmt);
    return pVal;
}

char *sqlQueryBind1(const char *sql, const char *arg1)
{
    return sqlQueryBindInternal(sql, arg1, NULL, 1);
}

char *sqlQueryBind2(const char *sql, const char *arg1, const char *arg2)
{
    return sqlQueryBindInternal(sql, arg1, arg2, 2);
}


-- Synthetic tenant fixture for pbx3cagi Phase 0 tests (safe to commit).
-- No real site or person names. Extend with new rows as scenarios grow.

PRAGMA foreign_keys = OFF;

CREATE TABLE IF NOT EXISTS globals (
    "id" TEXT PRIMARY KEY,
    "shortuid" TEXT UNIQUE,
    "pkey" TEXT UNIQUE,
    "sitename" TEXT,
    "fqdn" TEXT,
    "domain" TEXT
);

CREATE TABLE IF NOT EXISTS cluster (
    "id" TEXT PRIMARY KEY,
    "shortuid" TEXT UNIQUE,
    "pkey" TEXT NOT NULL,
    "abstimeout" INTEGER DEFAULT 14400,
    "active" TEXT DEFAULT 'YES',
    "allow_hash_xfer" TEXT DEFAULT 'enabled',
    "play_beep" INTEGER DEFAULT 1,
    "play_busy" INTEGER DEFAULT 1,
    "play_congested" INTEGER DEFAULT 1,
    "play_transfer" INTEGER DEFAULT 1,
    "voice_instr" INTEGER DEFAULT 1,
    "bounce_alert" TEXT DEFAULT '',
    "blind_busy" TEXT DEFAULT '',
    "int_ring_delay" INTEGER DEFAULT 20,
    "maxin" INTEGER DEFAULT 30,
    "ringdelay" INTEGER DEFAULT 20,
    "lterm" INTEGER DEFAULT 0,
    "cfwd_progress" TEXT DEFAULT 'enabled',
    "cfwd_answer" TEXT DEFAULT 'enabled',
    "ivr_key_wait" INTEGER DEFAULT 6,
    "ivr_digit_wait" INTEGER DEFAULT 6000,
    "syspass" TEXT DEFAULT '4444',
    "spy_pass" TEXT DEFAULT '3333',
    "dynamicfeatures" TEXT DEFAULT NULL,
    "clusterclid" TEXT DEFAULT '',
    "chanmax" INTEGER DEFAULT 3,
    "usemohcustom" TEXT DEFAULT 'NO',
    "callrecord_1" TEXT DEFAULT 'None',
    "masteroclo" TEXT DEFAULT 'AUTO',
    "oclo" TEXT DEFAULT '',
    "routeoverride" TEXT DEFAULT '',
    "voip_max" INTEGER DEFAULT 30,
    "cname" TEXT,
    "fqdn" TEXT
);

CREATE TABLE IF NOT EXISTS ipphone (
    "id" TEXT PRIMARY KEY,
    "shortuid" TEXT UNIQUE,
    "pkey" TEXT NOT NULL,
    "active" TEXT DEFAULT 'YES',
    "callerid" TEXT,
    "cname" TEXT,
    "cluster" TEXT DEFAULT 'default',
    "desc" TEXT,
    "description" TEXT,
    "devicerec" TEXT DEFAULT 'default',
    "dvrvmail" TEXT,
    "extalert" TEXT DEFAULT '',
    "technology" TEXT DEFAULT 'SIP',
    "transport" TEXT DEFAULT 'udp',
    UNIQUE("cluster", "pkey")
);

DELETE FROM ipphone;
DELETE FROM cluster;
DELETE FROM globals;

INSERT INTO globals (id, shortuid, pkey, sitename, fqdn, domain) VALUES
    ('fix00000000000000000000001', 'testgl01', 'global', 'Demo Test PBX', 'test.pbx3.local', 'pbx3.local');

INSERT INTO cluster (
    id, shortuid, pkey, cname, fqdn, play_transfer, voice_instr, callrecord_1, usemohcustom
) VALUES (
    'fix00000000000000000000002', 'testtn01', 'tenant01', 'Tenant 01', 'testtn01.pbx3.local', 1, 1, 'None', 'NO'
);

INSERT INTO ipphone (
    id, shortuid, pkey, cluster, desc, cname, description, dvrvmail, devicerec, technology, callerid
) VALUES
    ('fix00000000000000000000003', 'testex01', '1101', 'testtn01', 'Ext 1101', 'Ext 1101', 'Extension 1101', '1101', 'default', 'SIP', '1101'),
    ('fix00000000000000000000004', 'testex02', '1102', 'testtn01', 'Ext 1102', 'Ext 1102', 'Extension 1102', '1102', 'default', 'SIP', '1102');

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
    "ringdelay" INTEGER DEFAULT 0,
    "lterm" INTEGER DEFAULT 0,
    "cfwd_progress" TEXT DEFAULT 'enabled',
    "cfwd_answer" TEXT DEFAULT 'enabled',
    "ivr_key_wait" INTEGER DEFAULT 6,
    "ivr_digit_wait" INTEGER DEFAULT 6000,
    "syspass" TEXT DEFAULT '',
    "spy_pass" TEXT DEFAULT '',
    "dynamicfeatures" TEXT DEFAULT NULL,
    "clusterclid" TEXT DEFAULT '',
    "chanmax" INTEGER DEFAULT 3,
    "usemohcustom" TEXT DEFAULT 'NO',
    "callrecord_1" TEXT DEFAULT 'None',
    "masteroclo" TEXT DEFAULT 'AUTO',
    "oclo" TEXT DEFAULT 'OPEN',
    "sched_mode" TEXT DEFAULT 'open',
    "routeoverride" TEXT DEFAULT '',
    "holiday_force_dest" TEXT DEFAULT '',
    "voip_max" INTEGER DEFAULT 30,
    "cname" TEXT,
    "fqdn" TEXT,
    "domain" TEXT DEFAULT ''
);

CREATE TABLE IF NOT EXISTS trunks (
    "pkey" TEXT PRIMARY KEY,
    "active" TEXT DEFAULT 'YES'
);

CREATE TABLE IF NOT EXISTS dialalias (
    "id" TEXT PRIMARY KEY,
    "shortuid" TEXT UNIQUE,
    "pkey" TEXT NOT NULL,
    "active" TEXT DEFAULT 'YES',
    "cluster" TEXT DEFAULT 'default',
    "target_cluster" TEXT,
    "target_fqdn" TEXT NOT NULL,
    "cname" TEXT,
    "description" TEXT,
    UNIQUE("cluster", "pkey")
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

CREATE TABLE IF NOT EXISTS route_profile (
    "id" TEXT PRIMARY KEY,
    "shortuid" TEXT UNIQUE,
    "pkey" TEXT,
    "cluster" TEXT DEFAULT 'default',
    "name" TEXT,
    "default_mode" TEXT DEFAULT 'open'
);

CREATE TABLE IF NOT EXISTS route_profile_line (
    "id" TEXT PRIMARY KEY,
    "shortuid" TEXT UNIQUE,
    "profile" TEXT NOT NULL,
    "cluster" TEXT DEFAULT 'default',
    "mode" TEXT NOT NULL,
    "destination" TEXT NOT NULL,
    UNIQUE("profile", "mode")
);

CREATE TABLE IF NOT EXISTS inroutes (
    "id" TEXT PRIMARY KEY,
    "shortuid" TEXT UNIQUE,
    "pkey" TEXT NOT NULL,
    "active" TEXT DEFAULT 'YES',
    "cluster" TEXT DEFAULT 'default',
    "technology" TEXT DEFAULT 'DiD',
    "tag" TEXT DEFAULT '',
    "inprefix" TEXT DEFAULT '',
    "alertinfo" TEXT DEFAULT '',
    "moh" TEXT DEFAULT 'NO',
    "swoclip" TEXT DEFAULT 'NO',
    "openroute" TEXT DEFAULT 'None',
    "closeroute" TEXT DEFAULT 'None',
    "route_profile" TEXT DEFAULT '',
    "entry_dest" TEXT DEFAULT ''
);

DELETE FROM inroutes;
DELETE FROM route_profile_line;
DELETE FROM route_profile;
DELETE FROM ipphone;
DELETE FROM dialalias;
DELETE FROM trunks;
DELETE FROM cluster;
DELETE FROM globals;

INSERT INTO globals (id, shortuid, pkey, sitename, fqdn, domain) VALUES
    ('fix00000000000000000000001', 'testgl01', 'global', 'Demo Test PBX', 'test.pbx3.local', 'pbx3.local');

INSERT INTO cluster (
    id, shortuid, pkey, cname, fqdn, domain, play_transfer, voice_instr, callrecord_1, usemohcustom,
    oclo, sched_mode, ringdelay, routeoverride, holiday_force_dest
) VALUES (
    'fix00000000000000000000002', 'testtn01', 'tenant01', 'Tenant 01', 'testtn01.pbx3.local', 'testtn01.pbx3.local',
    1, 1, 'None', 'NO', 'OPEN', 'open', 0, '', ''
);

INSERT INTO dialalias (id, shortuid, pkey, active, cluster, target_fqdn, cname, description) VALUES
    ('fix00000000000000000000010', 'tda0001', '81', 'YES', 'testtn01', 'sister.pbx3.local', 'Sister site', 'lab prefix 81'),
    ('fix00000000000000000000011', 'tda0002', '82', 'NO', 'testtn01', 'sister.pbx3.local', 'Inactive', 'inactive prefix');

INSERT INTO ipphone (
    id, shortuid, pkey, cluster, desc, cname, description, dvrvmail, devicerec, technology, callerid
) VALUES
    ('fix00000000000000000000003', 'testex01', '1101', 'testtn01', 'Ext 1101', 'Ext 1101', 'Extension 1101', '1101', 'default', 'SIP', '1101'),
    ('fix00000000000000000000004', 'testex02', '1102', 'testtn01', 'Ext 1102', 'Ext 1102', 'Extension 1102', '1102', 'default', 'SIP', '1102');

-- Day-parts lab DID pair: dual-read open/close columns + profile with lunch
INSERT INTO route_profile (id, shortuid, pkey, cluster, name, default_mode) VALUES
    ('fix00000000000000000000020', 'rpopen01', 'rpopen01', 'testtn01', 'Lab dayparts', 'open');

INSERT INTO route_profile_line (id, shortuid, profile, cluster, mode, destination) VALUES
    ('fix00000000000000000000021', 'rplnopen', 'rpopen01', 'testtn01', 'open', '1101'),
    ('fix00000000000000000000022', 'rplnclos', 'rpopen01', 'testtn01', 'closed', '9001'),
    ('fix00000000000000000000023', 'rplnlunc', 'rpopen01', 'testtn01', 'lunch', '1102');

-- Lab spare DID form (digits) used in handoff
INSERT INTO inroutes (
    id, shortuid, pkey, cluster, technology, openroute, closeroute, route_profile, swoclip, moh
) VALUES
    ('fix00000000000000000000030', 'indid01', '441924910444', 'testtn01', 'DiD', '1101', '9001', 'rpopen01', 'NO', 'NO'),
    -- Legacy-only DID (no profile) for dual-read fallback
    ('fix00000000000000000000031', 'indid02', '441924999999', 'testtn01', 'DiD', '1101', '9001', '', 'NO', 'NO');

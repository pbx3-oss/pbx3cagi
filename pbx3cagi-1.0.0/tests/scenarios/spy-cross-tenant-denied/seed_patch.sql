-- Other tenant owns pkey 1000; calling tenant testtn01 must not resolve it.
UPDATE cluster SET spy_pass = '9911' WHERE shortuid = 'testtn01';

INSERT INTO cluster (
    id, shortuid, pkey, cname, fqdn, domain, play_transfer, voice_instr, callrecord_1, usemohcustom,
    oclo, sched_mode, ringdelay, routeoverride, holiday_force_dest, spy_pass
) VALUES (
    'fix00000000000000000000040', 'testtn02', 'tenant02', 'Tenant 02', 'testtn02.pbx3.local', 'testtn02.pbx3.local',
    1, 1, 'None', 'NO', 'OPEN', 'open', 0, '', '', '9911'
);

INSERT INTO ipphone (
    id, shortuid, pkey, cluster, desc, cname, description, dvrvmail, devicerec, technology, callerid
) VALUES (
    'fix00000000000000000000041', 'otherex1', '1000', 'testtn02', 'Ext 1000', 'Ext 1000', 'Other tenant 1000', '1000', 'default', 'SIP', '1000'
);

UPDATE cluster SET maxout = 1, chanmax = NULL WHERE shortuid = 'testtn01';
INSERT INTO trunks (pkey, active, peername, description, technology) VALUES
    ('Egress', 'YES', 'Egress', 'Lab egress', 'SIP');

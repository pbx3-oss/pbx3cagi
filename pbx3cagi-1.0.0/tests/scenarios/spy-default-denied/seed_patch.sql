-- Prove stock 3333 is refused (fail-closed), not only empty.
UPDATE cluster SET spy_pass = '3333' WHERE shortuid = 'testtn01';

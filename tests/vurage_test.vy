module vurage;

db = vurage.open("tests/vurage_db");
vurage.put(db, 1, "Hero"); # Fuck yea
vurage.put_str(db, "player:hp", "100");

name = vurage.get(db, 1);
hp = vurage.get_str(db, "player:hp");

out(name);
out(hp);
out(vurage.exists(db, 1));
out(vurage.exists_str(db, "player:hp"));

vurage.sync(db);
vurage.close(db);

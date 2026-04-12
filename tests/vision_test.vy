ruleset { warnings, dynamic_casting };

# Artıq vcv modulunu çağıra bilərik
module vcv;
module vmem;

out("--- Vyne Vision System (STB Mode) ---");
out("Initial usage: " + string(vmem.usage()) + " bytes");

# Şəkli Windows-un öz baxıcısında açır (Heç bir DLL tələb etmir)
vcv.show("tests\\assets\\ferhadla_dans.jpg");

# Şəkil məlumatlarını (width/height) terminala çap edir (STB vasitəsilə)
vcv.info("tests/assets/ferhadla_dans.jpg");

out("Final usage: " + string(vmem.usage()) + " bytes");
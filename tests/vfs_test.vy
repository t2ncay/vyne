ruleset { dynamic_casting };
module vfs;

# I'll write a lot of useless test here to prove tuncay that this shit works

if (false) { # I did this to not create a useless Hello dir on yall's local repo
    vfs.mkdir("Hello"); # This will create an empty dir named "Hello"
}
out(vfs.cwd); # This is a property, it's going to output the working directory
if (vfs.exists(vfs.join("tests", "vfs_test.vy"))) { # Two birds with one rock lol
    out("THIS SHIT EXISTS OMG");
}

out(vfs.read(vfs.join("tests", "test.vy"))); # This will print whatever is in test.vy

# Anyways enough of me thank you for reading this lil bro

use extern "vcolors.vy";

out(vcolors.green("Green text"));
out(vcolors.boldRed("BOLD RED"));
out(vcolors.bgYellow("  WARNING  "));
out(vcolors.success("File saved"));

label = vcolors.bold("Name: ");
value = vcolors.green("John Doe");
out(label + value);

score = 85;
if score >= 70 {
   out(vcolors.green("Passed: " + string(score)));
} else {
   out(vcolors.red("Failed: " + string(score)));
}

out(vcolors.box("Sabah derse getmeye axot yoxduye"));

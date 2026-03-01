use lib "vstring.vy";

me = "abdullah";
u = "TUNCAY";
msg = "sabah getmeyek D....  hoca ragebait";
sp = "sWaP opEratIoN";
test1 = "did-you-get-di-aydiya";
test2 = "luk 
hiyir";
test3 = "hello world";

out(vstring.upper(me));
out(vstring.lower(u));
out(vstring.title(msg));
out(vstring.swapCase(sp));
out(vstring.split(test1, "-"));
out(vstring.split(test2, "\n"));
out(vstring.join(test3, "-"));

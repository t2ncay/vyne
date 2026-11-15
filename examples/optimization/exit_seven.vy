# V01: preserve a nonzero exit, not merely a generic failure.
out("V01 exit seven reached");
exit(7);
out("V01 unreachable (BUG)");

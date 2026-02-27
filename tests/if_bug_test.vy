interface Token {
    value :: String
}

token :: Token = Token("*");

if (token.value  == "*") || (token.value  == "-") || (token.value  == "+") || (token.value  == "/") {
    out("porno");
} else {
    out("dih");
}
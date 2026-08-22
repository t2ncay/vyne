ruleset { dynamic_casting };

config = "hi";
default_config = "hello";

value = config ?? default_config;

out(value);
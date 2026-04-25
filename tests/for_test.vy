ruleset {
    dynamic_casting
};

y :: Array = [1,1,1,1,2,2,2,3,3,3,6];

x :: Array = through y -> unique;
x :: Array = through x -> collect { _ * 10 };

out(x);
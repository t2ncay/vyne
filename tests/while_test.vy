ruleset { dynamic_casting };

group Engine {
    counter = 0;
    limit = 10000000;
    step = 1;
};

group Stats {
    total = 0;
};

out("Stress testi bashladi...");

while (Engine.counter < Engine.limit) {
    Stats.total = Stats.total + Engine.counter;
    Engine.counter = Engine.counter + Engine.step;
}

out("Dovr bitdi!");
out("Counter: " + string(Engine.counter));
out("Total: " + string(Stats.total));
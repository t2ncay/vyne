ruleset { dynamic_casting };

interface Person {
    health :: Int64
}

person = Person(100);

out(person.health);
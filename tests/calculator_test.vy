ruleset {
    dynamic_casting
};

module vcore;
module vmem;

while true {
    input_1 = number(vcore.input("Enter number 1 : "));
    input_2 = number(vcore.input("Enter number 2 : "));
    process = vcore.input("Enter the process : ");

    if process == "+" { 
        return out(input_1 + input_2); 
    }

    if input == "exit" { break; }
}
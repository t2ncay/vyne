module vcore;

# Greatest Common Divisor (GCD) Test

while true {
    input_1 = int64(vcore.input("Enter number 1 : "));
    input_2 = int64(vcore.input("Enter number 2 : "));

    a = input_1;
    b = input_2;
    
    while b > 0 || b < 0 {
        temp = b;
        b = a % b;
        a = temp;
    }
    
    gcd = a;

    temp = input_1 * input_2;

    if temp < 0 {
        temp = -temp; # Literally coding abs() function because vyne doesn't have it yet smh
    } # Ts so cringe that I have to do this, but hey, it works

    lcm = temp / gcd;

    out(lcm);

    if input_1 == "exit" || input_2 == "exit" { break; }
}

module vplot;

fn :: vplot bar_chart(data, height) {
    count = 0;
    through data -> collect {
        line = "";
        through 1..height -> collect { 
            line = line + "|"; 
        };
        
        out(string(data[count]) + " " + line); 
        
        count++;
    };
}

deploy vplot;
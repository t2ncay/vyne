fn unique_arr(arr) {
    result = [];
    i = 0;
    size = arr.size();

    while i < size {
        item = arr[i];
        found = false;
        
        j = 0;
        resSize = result.size();
        while j < resSize {
            if result[j] == item {
                found = true;
                break;
            }
            j = j + 1;
        }

        if found == false {
            result.push(item);
        }
        
        i = i + 1;
    }
    return result;
}

raw = [1, 2, 2, 3, 1, 4, 5, 4];
cleaned = unique_arr(raw);

out("Original:");
out(raw);
out("Unique:");
out(cleaned); # Expected: [1, 2, 3, 4, 5]
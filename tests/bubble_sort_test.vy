numbers = [42, 7, 19, 88, 3, 56, 12, 101, 34, 25, 67, 2, 91, 14, 48];
n = numbers.size();

i = 0;

fn checkSorted(arr, size) {
    sortedStatus = 1;
    row = 0;
    while row < size {
        col = 0;
        while col < (size - row - 1) {
            if arr[col] > arr[col + 1] {
                sortedStatus = 0;
                out("ur shit ain't sorted bro");
                break;
            }
            col = col + 1;
        }
        row = row + 1;
    }
    return sortedStatus;
}

out(checkSorted(numbers, numbers.size()));
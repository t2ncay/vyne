module vstring;

# mappings
lowercase = "abcdefghijklmnopqrstuvwxyz";
uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

fn :: vstring upper(s) {
    result = "";
    i = 0;
    
    while i < s.length() {
        ch = s[i];
        pos = 0;
        found = 0;
        
        # Find position in lowercase
        while pos < lowercase.length() {
            if ch == lowercase[pos] {
                result = result + uppercase[pos];
                found = 1;
                break;
            }
            pos = pos + 1;
        }
        
        # If not found, keep original
        if found == 0 {
            result = result + ch;
        }
        
        i = i + 1;
    }
    return result;
}

fn :: vstring lower(s) {
    result = "";
    i = 0;
    
    while i < s.length() {
        ch = s[i];
        j = 0;
        found = 0;
        
        while j < uppercase.length() {
            if ch == uppercase[j] {
                result = result + lowercase[j];
                found = 1;
                break;
            }
            j = j + 1;
        }
        
        if found == 0 {
            result = result + ch;
        }
        
        i = i + 1;
    }
    return result;
}

fn :: vstring title(s) {
    result = "";
    i = 0;
    newWord = true;  # Start of string is like start of new word
    
    while i < s.length() {
        ch = s[i];
        
        # Check if this character starts a new word
        if ch == " " {
            # It's whitespace - keep as is, next letter starts new word
            result = result + ch;
            newWord = true;
        } else {
            # It's a letter or other character
            if newWord {
                # First character of word - make uppercase
                j = 0;
                found = 0;
                while j < lowercase.length() {
                    if ch == lowercase[j] {
                        result = result + uppercase[j];
                        found = 1;
                        break;
                    }
                    j = j + 1;
                }
                if found == 0 {
                    # Not a lowercase letter, check if it's uppercase
                    k = 0;
                    while k < uppercase.length() {
                        if ch == uppercase[k] {
                            result = result + uppercase[k];  # Keep as uppercase
                            found = 1;
                            break;
                        }
                        k = k + 1;
                    }
                    if found == 0 {
                        # Not a letter at all (number, symbol)
                        result = result + ch;
                    }
                }
                newWord = false;
            } else {
                # Not first character of word - make lowercase
                j = 0;
                found = 0;
                while j < uppercase.length() {
                    if ch == uppercase[j] {
                        result = result + lowercase[j];
                        found = 1;
                        break;
                    }
                    j = j + 1;
                }
                if found == 0 {
                    # Not uppercase, check if it's lowercase
                    k = 0;
                    while k < lowercase.length() {
                        if ch == lowercase[k] {
                            result = result + lowercase[k];  # Keep as lowercase
                            found = 1;
                            break;
                        }
                        k = k + 1;
                    }
                    if found == 0 {
                        # Not a letter (number, symbol)
                        result = result + ch;
                    }
                }
            }
        }
        
        i = i + 1;
    }
    
    return result;
}

fn :: vstring swapCase(s) {
    result = "";
    i = 0;
    
    while i < s.length() {
        ch = s[i];
        j = 0;
        found = 0;
        
        # Check if it's lowercase (convert to uppercase)
        while j < lowercase.length() {
            if ch == lowercase[j] {
                result = result + uppercase[j];
                found = 1;
                break;
            }
            j = j + 1;
        }
        
        if found == 0 {
            # Check if it's uppercase (convert to lowercase)
            j = 0;
            while j < uppercase.length() {
                if ch == uppercase[j] {
                    result = result + lowercase[j];
                    found = 1;
                    break;
                }
                j = j + 1;
            }
            
            if found == 0 {
                # Not a letter
                result = result + ch;
            }
        }
        
        i = i + 1;
    }
    return result;
}

# ===Main string methods===

# Split string into array by delimiter
fn :: vstring split(s, delimiter) {
    result = [];
    
    if s.length() == 0 {
        return result;
    }
    
    current = "";
    i = 0;
    
    while i < s.length() {
        ch = s[i];
        
        # Check if current character matches delimiter
        if ch == delimiter {
            # End of word - add to result if not empty
            if current != "" {
                result = result + [current];
                current = "";
            }
        } else {
            # Add character to current word
            current = current + ch;
        }
        
        i = i + 1;
    }
    
    # Add last word if not empty
    if current != "" {
        result = result + [current];
    }
    
    return result;
}

# Join array of strings with separator
fn :: vstring join(arr, separator) {
    result = "";
    i = 0;
    
    while i < arr.length() {
        if i > 0 {
            result = result + separator;
        }
        result = result + arr[i];
        i = i + 1;
    }
    
    return result;
}

deploy vstring;
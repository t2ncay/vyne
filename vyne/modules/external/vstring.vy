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

# Convert to camelCase (first word lowercase, rest capitalized)
fn :: vstring toCamelCase(s) {
    # First convert to lowercase and split into words
    lowerStr = vstring.lower(s);
    words = [];
    current = "";
    i = 0;
    
    # Split into words
    while i < lowerStr.length() {
        ch = lowerStr[i];
        if (ch >= "a" and ch <= "z") or (ch >= "0" and ch <= "9") {
            current = current + ch;
        } else {
            if current != "" {
                words = words + [current];
                current = "";
            }
        }
        i = i + 1;
    }
    if current != "" {
        words = words + [current];
    }
    
    # Build camelCase
    if words.length() == 0 {
        return "";
    }
    
    result = words[0];  # First word lowercase
    
    i = 1;
    while i < words.length() {
        word = words[i];
        if word.length() > 0 {
            # Capitalize first letter of remaining words
            first = word[0];
            rest = word[1 .. word.length()];
            
            # Capitalize first letter
            j = 0;
            while j < lowercase.length() {
                if first == lowercase[j] {
                    first = uppercase[j];
                    break;
                }
                j = j + 1;
            }
            result = result + first + rest;
        }
        i = i + 1;
    }
    
    return result;
}

# Convert to PascalCase (each word capitalized)
fn :: vstring toPascalCase(s) {
    # First convert to lowercase and split into words
    lowerStr = vstring.lower(s);
    words = [];
    current = "";
    i = 0;
    
    # Split into words
    while i < lowerStr.length() {
        ch = lowerStr[i];
        if (ch >= "a" and ch <= "z") or (ch >= "0" and ch <= "9") {
            current = current + ch;
        } else {
            if current != "" {
                words = words + [current];
                current = "";
            }
        }
        i = i + 1;
    }
    if current != "" {
        words = words + [current];
    }
    
    # Build PascalCase
    result = "";
    i = 0;
    while i < words.length() {
        word = words[i];
        if word.length() > 0 {
            # Capitalize first letter
            first = word[0];
            rest = word[1 .. word.length()];
            
            j = 0;
            while j < lowercase.length() {
                if first == lowercase[j] {
                    first = uppercase[j];
                    break;
                }
                j = j + 1;
            }
            result = result + first + rest;
        }
        i = i + 1;
    }
    
    return result;
}

# Convert to snake_case (lowercase with underscores)
fn :: vstring toSnakeCase(s) {
    lowerStr = vstring.lower(s);
    result = "";
    i = 0;
    
    while i < lowerStr.length() {
        ch = lowerStr[i];
        if (ch >= "a" and ch <= "z") or (ch >= "0" and ch <= "9") {
            result = result + ch;
        } else if ch == " " or ch == "-" or ch == "_" {
            if result.length() > 0 and result[result.length()-1] != "_" {
                result = result + "_";
            }
        }
        i = i + 1;
    }
    
    # Remove trailing underscore
    if result.length() > 0 and result[result.length()-1] == "_" {
        result = result[0 .. result.length()-1];
    }
    
    return result;
}

# Convert to kebab-case (lowercase with hyphens)
fn :: vstring toKebabCase(s) {
    lowerStr = vstring.lower(s);
    result = "";
    i = 0;
    
    while i < lowerStr.length() {
        ch = lowerStr[i];
        if (ch >= "a" and ch <= "z") or (ch >= "0" and ch <= "9") {
            result = result + ch;
        } else if ch == " " or ch == "-" or ch == "_" {
            if result.length() > 0 and result[result.length()-1] != "-" {
                result = result + "-";
            }
        }
        i = i + 1;
    }
    
    # Remove trailing hyphen
    if result.length() > 0 and result[result.length()-1] == "-" {
        result = result[0 .. result.length()-1];
    }
    
    return result;
}

deploy vstring;
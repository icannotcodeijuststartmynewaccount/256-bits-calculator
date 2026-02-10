#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>

// ==================== 256-BIT INTEGER (EXACT) ====================

typedef struct {
    uint64_t part[4];  // part[0] = most significant
                       // part[1]
                       // part[2]
                       // part[3] = least significant
    int sign;          // 0 = positive, 1 = negative
} Int256;

// ==================== CONSTANTS ====================
#define BASE 1000000000ULL  // For decimal conversion (10^9)

// ==================== UTILITY FUNCTIONS ====================

Int256 str_to_int256(const char* str) {
    Int256 result = {{0, 0, 0, 0}, 0};
    
    // Skip leading whitespace
    while (isspace((unsigned char)*str)) str++;
    
    int i = 0;
    if (str[0] == '-') {
        result.sign = 1;
        i = 1;
    } else if (str[0] == '+') {
        i = 1;
    }
    
    int base = 10;
    if (str[i] == '0' && (str[i+1] == 'x' || str[i+1] == 'X')) {
        base = 16;
        i += 2;
    } else if (str[i] == '0' && (str[i+1] == 'b' || str[i+1] == 'B')) {
        base = 2;
        i += 2;
    }
    
    while (str[i]) {
        uint64_t digit;
        char c = str[i];
        
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (base == 16) {
            if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else break;
        } else {
            break;
        }
        
        if (digit >= (uint64_t)base) break;
        
        uint64_t carry = digit;
        for (int j = 3; j >= 0; j--) {
            __uint128_t temp = (__uint128_t)result.part[j] * base + carry;
            result.part[j] = (uint64_t)temp;
            carry = (uint64_t)(temp >> 64);
        }
        
        if (carry != 0) {
            for (int j = 3; j > 0; j--) {
                result.part[j] = result.part[j-1];
            }
            result.part[0] = carry;
        }
        
        i++;
    }
    
    // Skip trailing whitespace
    while (str[i] && isspace((unsigned char)str[i])) i++;
    
    if (str[i] != '\0') {
        printf("Warning: Invalid characters in number '%s'\n", str);
    }
    
    if (result.part[0] == 0 && result.part[1] == 0 && 
        result.part[2] == 0 && result.part[3] == 0) {
        result.sign = 0;
    }
    
    return result;
}

void int256_to_hex(const Int256 n, char* buffer, size_t buffer_size) {
    if (buffer_size < 68) {  // 2 for "0x" + 64 hex digits + 1 for sign + 1 for null
        buffer[0] = '\0';
        return;
    }
    
    char* ptr = buffer;
    
    if (n.sign) *ptr++ = '-';
    *ptr++ = '0';
    *ptr++ = 'x';
    
    int started = 0;
    for (int i = 0; i < 4; i++) {
        if (n.part[i] != 0 || started || i == 3) {
            if (!started) {
                int written = snprintf(ptr, buffer_size - (ptr - buffer), 
                                      "%" PRIx64, n.part[i]);
                if (written < 0) {
                    buffer[0] = '\0';
                    return;
                }
                ptr += written;
                started = 1;
            } else {
                int written = snprintf(ptr, buffer_size - (ptr - buffer), 
                                      "%016" PRIx64, n.part[i]);
                if (written < 0) {
                    buffer[0] = '\0';
                    return;
                }
                ptr += written;
            }
        }
    }
    if (!started) {
        *ptr++ = '0';
        *ptr = '\0';
    } else {
        *ptr = '\0';
    }
}

void int256_to_decimal(const Int256 n, char* buffer, size_t buffer_size) {
    if (buffer_size < 2) {
        buffer[0] = '\0';
        return;
    }
    
    char* buf_ptr = buffer;
    
    if (n.sign && buffer_size > 1) {
        *buf_ptr++ = '-';
        buffer_size--;
    }
    
    if (n.part[0] == 0 && n.part[1] == 0 && n.part[2] == 0 && n.part[3] == 0) {
        *buf_ptr++ = '0';
        *buf_ptr = '\0';
        return;
    }
    
    // Use a temporary buffer large enough for 256-bit decimal (up to 78 digits)
    char temp[100] = {0};
    int pos = sizeof(temp) - 1;
    temp[pos] = '\0';
    
    uint64_t work[8] = {0};
    work[4] = n.part[0];
    work[5] = n.part[1];
    work[6] = n.part[2];
    work[7] = n.part[3];
    
    while (1) {
        uint64_t remainder = 0;
        for (int i = 0; i < 8; i++) {
            __uint128_t value = ((__uint128_t)remainder << 64) | work[i];
            work[i] = (uint64_t)(value / BASE);
            remainder = (uint64_t)(value % BASE);
        }
        
        // Extract 9 decimal digits from remainder
        char digits[10];
        for (int j = 8; j >= 0; j--) {
            digits[j] = '0' + (remainder % 10);
            remainder /= 10;
        }
        
        // Add digits to temp buffer
        int start = 0;
        while (start < 9 && digits[start] == '0') start++;
        for (int j = start; j < 9; j++) {
            temp[--pos] = digits[j];
        }
        
        int all_zero = 1;
        for (int i = 0; i < 8; i++) {
            if (work[i] != 0) {
                all_zero = 0;
                break;
            }
        }
        if (all_zero) break;
    }
    
    // Remove leading zeros
    while (temp[pos] == '0') pos++;
    
    // Copy to output buffer
    size_t len = sizeof(temp) - pos - 1;
    if (len >= buffer_size) {
        buffer[0] = '\0';
        return;
    }
    strcpy(buf_ptr, &temp[pos]);
}

int cmp_abs_int256(const Int256 a, const Int256 b) {
    for (int i = 0; i < 4; i++) {
        if (a.part[i] < b.part[i]) return -1;
        if (a.part[i] > b.part[i]) return 1;
    }
    return 0;
}

int is_zero_int256(const Int256 a) {
    return a.part[0] == 0 && a.part[1] == 0 && a.part[2] == 0 && a.part[3] == 0;
}

Int256 neg_int256(Int256 a) {
    if (!is_zero_int256(a)) {
        a.sign = !a.sign;
    }
    return a;
}

Int256 abs_int256(Int256 a) {
    a.sign = 0;
    return a;
}

// ==================== EXACT ARITHMETIC (FIXED) ====================

static inline uint64_t add_with_carry(uint64_t a, uint64_t b, uint64_t* carry) {
    __uint128_t sum = (__uint128_t)a + b + *carry;
    *carry = (uint64_t)(sum >> 64);
    return (uint64_t)sum;
}

static inline uint64_t sub_with_borrow(uint64_t a, uint64_t b, uint64_t* borrow) {
    __uint128_t diff = (__uint128_t)a - b - *borrow;
    *borrow = (diff >> 127) ? 1 : 0;  // Fix: check if underflow occurred
    return (uint64_t)diff;
}

Int256 add_int256(Int256 a, Int256 b) {
    Int256 result;
    
    if (a.sign == b.sign) {
        uint64_t carry = 0;
        for (int i = 3; i >= 0; i--) {
            result.part[i] = add_with_carry(a.part[i], b.part[i], &carry);
        }
        result.sign = a.sign;
        // If we have carry out of most significant part, we have overflow
        if (carry != 0) {
            printf("Warning: Addition overflow\n");
        }
    } else {
        if (cmp_abs_int256(a, b) >= 0) {
            uint64_t borrow = 0;
            for (int i = 3; i >= 0; i--) {
                result.part[i] = sub_with_borrow(a.part[i], b.part[i], &borrow);
            }
            result.sign = a.sign;
        } else {
            uint64_t borrow = 0;
            for (int i = 3; i >= 0; i--) {
                result.part[i] = sub_with_borrow(b.part[i], a.part[i], &borrow);
            }
            result.sign = b.sign;
        }
    }
    
    if (is_zero_int256(result)) result.sign = 0;
    return result;
}

Int256 sub_int256(Int256 a, Int256 b) {
    b.sign = !b.sign;
    Int256 result = add_int256(a, b);
    b.sign = !b.sign;
    return result;
}

// FIXED MULTIPLICATION - Schoolbook algorithm
Int256 mul_int256(Int256 a, Int256 b) {
    // 4x4 schoolbook multiplication
    __uint128_t temp[8] = {0};
    
    // Multiply each 64-bit part
    for (int i = 3; i >= 0; i--) {
        for (int j = 3; j >= 0; j--) {
            __uint128_t product = (__uint128_t)a.part[i] * b.part[j];
            int pos = (3 - i) + (3 - j);  // Position in temp array
            
            temp[pos] += (uint64_t)product;
            if (temp[pos] < (uint64_t)product) { // Check for overflow
                temp[pos + 1] += 1;
            }
            temp[pos + 1] += (product >> 64);
        }
    }
    
    // Propagate carries
    for (int i = 0; i < 7; i++) {
        uint64_t carry = (uint64_t)(temp[i] >> 64);
        if (carry != 0) {
            temp[i] &= 0xFFFFFFFFFFFFFFFFULL;
            temp[i + 1] += carry;
        }
    }
    
    // Check for overflow beyond 256 bits
    if (temp[7] != 0 || temp[6] != 0 || temp[5] != 0 || temp[4] != 0) {
        printf("Warning: Multiplication overflow (result exceeds 256 bits)\n");
    }
    
    // Build result (256 bits)
    Int256 result;
    result.part[3] = (uint64_t)temp[0];
    result.part[2] = (uint64_t)temp[1];
    result.part[1] = (uint64_t)temp[2];
    result.part[0] = (uint64_t)temp[3];
    
    // Sign handling
    result.sign = a.sign ^ b.sign;
    if (is_zero_int256(result)) result.sign = 0;
    
    return result;
}

// FIXED DIVISION - Binary long division with proper sign handling
Int256 div_int256(Int256 a, Int256 b) {
    if (is_zero_int256(b)) {
        printf("Error: Division by zero!\n");
        return (Int256){{0, 0, 0, 0}, 0};
    }
    
    int result_sign = a.sign ^ b.sign;
    
    // Work with absolute values
    Int256 a_abs = a;
    a_abs.sign = 0;
    Int256 b_abs = b;
    b_abs.sign = 0;
    
    if (cmp_abs_int256(a_abs, b_abs) < 0) {
        return (Int256){{0, 0, 0, 0}, 0};
    }
    
    // Binary long division
    Int256 quotient = {{0, 0, 0, 0}, 0};
    Int256 remainder = {{0, 0, 0, 0}, 0};
    
    for (int i = 0; i < 4; i++) {
        for (int bit = 63; bit >= 0; bit--) {
            // Shift remainder left by 1
            uint64_t carry = remainder.part[0] >> 63;
            remainder.part[0] = (remainder.part[0] << 1) | (remainder.part[1] >> 63);
            remainder.part[1] = (remainder.part[1] << 1) | (remainder.part[2] >> 63);
            remainder.part[2] = (remainder.part[2] << 1) | (remainder.part[3] >> 63);
            remainder.part[3] = (remainder.part[3] << 1);
            
            // Add next bit from a
            remainder.part[3] |= (a_abs.part[i] >> bit) & 1;
            
            // Shift quotient left
            carry = quotient.part[0] >> 63;
            quotient.part[0] = (quotient.part[0] << 1) | (quotient.part[1] >> 63);
            quotient.part[1] = (quotient.part[1] << 1) | (quotient.part[2] >> 63);
            quotient.part[2] = (quotient.part[2] << 1) | (quotient.part[3] >> 63);
            quotient.part[3] = (quotient.part[3] << 1);
            
            if (cmp_abs_int256(remainder, b_abs) >= 0) {
                // Subtract b from remainder
                uint64_t borrow = 0;
                for (int j = 3; j >= 0; j--) {
                    remainder.part[j] = sub_with_borrow(remainder.part[j], b_abs.part[j], &borrow);
                }
                quotient.part[3] |= 1;
            }
        }
    }
    
    quotient.sign = result_sign;
    if (is_zero_int256(quotient)) quotient.sign = 0;
    
    return quotient;
}

Int256 mod_int256(Int256 a, Int256 b) {
    if (is_zero_int256(b)) {
        printf("Error: Division by zero!\n");
        return (Int256){{0, 0, 0, 0}, 0};
    }
    
    int result_sign = a.sign;  // Remainder takes sign of dividend
    
    // Work with absolute values
    Int256 a_abs = a;
    a_abs.sign = 0;
    Int256 b_abs = b;
    b_abs.sign = 0;
    
    if (cmp_abs_int256(a_abs, b_abs) < 0) {
        return a;  // If |a| < |b|, remainder is a
    }
    
    // Binary long division to get remainder
    Int256 remainder = {{0, 0, 0, 0}, 0};
    
    for (int i = 0; i < 4; i++) {
        for (int bit = 63; bit >= 0; bit--) {
            // Shift remainder left by 1
            uint64_t carry = remainder.part[0] >> 63;
            remainder.part[0] = (remainder.part[0] << 1) | (remainder.part[1] >> 63);
            remainder.part[1] = (remainder.part[1] << 1) | (remainder.part[2] >> 63);
            remainder.part[2] = (remainder.part[2] << 1) | (remainder.part[3] >> 63);
            remainder.part[3] = (remainder.part[3] << 1);
            
            // Add next bit from a
            remainder.part[3] |= (a_abs.part[i] >> bit) & 1;
            
            if (cmp_abs_int256(remainder, b_abs) >= 0) {
                // Subtract b from remainder
                uint64_t borrow = 0;
                for (int j = 3; j >= 0; j--) {
                    remainder.part[j] = sub_with_borrow(remainder.part[j], b_abs.part[j], &borrow);
                }
            }
        }
    }
    
    remainder.sign = result_sign;
    if (is_zero_int256(remainder)) remainder.sign = 0;
    
    return remainder;
}

// ==================== BITWISE OPERATIONS ====================

Int256 and_int256(Int256 a, Int256 b) {
    Int256 result;
    for (int i = 0; i < 4; i++) {
        result.part[i] = a.part[i] & b.part[i];
    }
    result.sign = a.sign & b.sign;
    return result;
}

Int256 or_int256(Int256 a, Int256 b) {
    Int256 result;
    for (int i = 0; i < 4; i++) {
        result.part[i] = a.part[i] | b.part[i];
    }
    result.sign = a.sign | b.sign;
    return result;
}

Int256 xor_int256(Int256 a, Int256 b) {
    Int256 result;
    for (int i = 0; i < 4; i++) {
        result.part[i] = a.part[i] ^ b.part[i];
    }
    result.sign = a.sign ^ b.sign;
    return result;
}

Int256 shift_left_int256(Int256 a, int bits) {
    if (bits <= 0) return a;
    if (bits >= 256) return (Int256){{0, 0, 0, 0}, a.sign};
    
    Int256 result = {{0, 0, 0, 0}, a.sign};
    
    int word_shift = bits / 64;
    int bit_shift = bits % 64;
    
    if (bit_shift == 0) {
        for (int i = 3; i >= word_shift; i--) {
            result.part[i] = a.part[i - word_shift];
        }
    } else {
        for (int i = 3; i >= word_shift; i--) {
            int src_idx = i - word_shift;
            int prev_idx = src_idx - 1;
            
            uint64_t low_part = (prev_idx >= 0) ? (a.part[prev_idx] >> (64 - bit_shift)) : 0;
            result.part[i] = (a.part[src_idx] << bit_shift) | low_part;
        }
    }
    
    return result;
}

Int256 shift_right_int256(Int256 a, int bits) {
    if (bits <= 0) return a;
    if (bits >= 256) return (Int256){{0, 0, 0, 0}, a.sign};
    
    Int256 result = {{0, 0, 0, 0}, a.sign};
    
    int word_shift = bits / 64;
    int bit_shift = bits % 64;
    
    if (bit_shift == 0) {
        for (int i = 0; i < 4 - word_shift; i++) {
            result.part[i] = a.part[i + word_shift];
        }
    } else {
        for (int i = 0; i < 4 - word_shift; i++) {
            int src_idx = i + word_shift;
            int next_idx = src_idx + 1;
            
            uint64_t high_part = (next_idx < 4) ? (a.part[next_idx] << (64 - bit_shift)) : 0;
            result.part[i] = (a.part[src_idx] >> bit_shift) | high_part;
        }
    }
    
    return result;
}

// ==================== COMPARISON ====================

int cmp_int256(Int256 a, Int256 b) {
    if (a.sign != b.sign) {
        return (a.sign > b.sign) ? -1 : 1;
    }
    
    int cmp = cmp_abs_int256(a, b);
    return a.sign ? -cmp : cmp;
}

// ==================== POWER AND FACTORIAL (FIXED) ====================

Int256 pow_int256(Int256 base, unsigned int exp) {
    if (exp == 0) {
        return (Int256){{0, 0, 0, 1}, 0};
    }
    
    Int256 result = {{0, 0, 0, 1}, 0};
    Int256 temp = base;
    
    while (exp > 0) {
        if (exp & 1) {
            result = mul_int256(result, temp);
        }
        temp = mul_int256(temp, temp);
        exp >>= 1;
    }
    
    return result;
}

Int256 factorial_int256(unsigned int n) {
    if (n <= 1) return (Int256){{0, 0, 0, 1}, 0};
    
    Int256 result = {{0, 0, 0, 1}, 0};
    for (unsigned int i = 2; i <= n; i++) {
        Int256 multiplier = {{0, 0, 0, i}, 0};
        result = mul_int256(result, multiplier);
        if (is_zero_int256(result)) {
            printf("Warning: Factorial overflow at i=%u\n", i);
            break;
        }
    }
    
    return result;
}

// ==================== MAIN WITH FIXED INPUT HANDLING ====================

void print_menu() {
    printf("\n=== 256-Bit Calculator (Exact) ===\n");
    printf("1. Add (+)          2. Subtract (-)\n");
    printf("3. Multiply (×)     4. Divide (÷)\n");
    printf("5. Modulo (%%)       6. AND (&)\n");
    printf("7. OR (|)           8. XOR (^)\n");
    printf("9. Shift Left (<<) 10. Shift Right (>>)\n");
    printf("11. Power (a^b)    12. Factorial (n!)\n");
    printf("13. Compare        14. Negate (-x)\n");
    printf("15. Absolute       0. Exit\n");
    printf("Choice: ");
}

int main() {
    printf("256-Bit Calculator with Exact Arithmetic\n");
    printf("Supports: Negative numbers, Fixed multiplication\n");
    printf("Max value: ~1.16e77 (2^256 - 1)\n");
    
    char input[100];
    
    while (1) {
        print_menu();
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Remove newline
        input[strcspn(input, "\n")] = '\0';
        
        // Skip empty input
        if (input[0] == '\0') {
            printf("Please enter a choice (0-15)\n");
            continue;
        }
        
        // Convert to number
        char* endptr;
        long choice_long = strtol(input, &endptr, 10);
        
        if (*endptr != '\0') {
            printf("Invalid input: '%s'. Please enter a number 0-15.\n", input);
            continue;
        }
        
        int choice = (int)choice_long;
        
        if (choice < 0 || choice > 15) {
            printf("Invalid choice: %d. Please enter 0-15.\n", choice);
            continue;
        }
        
        if (choice == 0) break;
        
        char buffer1[100], buffer2[100];
        char result_hex[70], result_dec[80];
        Int256 a, b, result;
        unsigned int shift = 0, power = 0, factorial_n = 0;
        int cmp;
        
        switch (choice) {
            case 1: case 2: case 3: case 4: case 5:
            case 6: case 7: case 8:
                printf("Enter first number (decimal/hex/0b binary): ");
                fflush(stdout);
                if (fgets(buffer1, sizeof(buffer1), stdin) == NULL) {
                    printf("Input error\n");
                    break;
                }
                buffer1[strcspn(buffer1, "\n")] = '\0';
                
                printf("Enter second number: ");
                fflush(stdout);
                if (fgets(buffer2, sizeof(buffer2), stdin) == NULL) {
                    printf("Input error\n");
                    break;
                }
                buffer2[strcspn(buffer2, "\n")] = '\0';
                
                a = str_to_int256(buffer1);
                b = str_to_int256(buffer2);
                
                switch (choice) {
                    case 1: result = add_int256(a, b); break;
                    case 2: result = sub_int256(a, b); break;
                    case 3: result = mul_int256(a, b); break;
                    case 4: result = div_int256(a, b); break;
                    case 5: result = mod_int256(a, b); break;
                    case 6: result = and_int256(a, b); break;
                    case 7: result = or_int256(a, b); break;
                    case 8: result = xor_int256(a, b); break;
                    default: result = a; break;
                }
                
                int256_to_hex(result, result_hex, sizeof(result_hex));
                int256_to_decimal(result, result_dec, sizeof(result_dec));
                printf("\nResult: %s\n", result_dec);
                printf("Hex: %s\n", result_hex);
                break;
                
            case 9: case 10:
                printf("Enter number: ");
                fflush(stdout);
                if (fgets(buffer1, sizeof(buffer1), stdin) == NULL) {
                    printf("Input error\n");
                    break;
                }
                buffer1[strcspn(buffer1, "\n")] = '\0';
                
                printf("Enter bits to shift: ");
                fflush(stdout);
                if (scanf("%u", &shift) != 1) {
                    while (getchar() != '\n');  // Clear input
                    printf("Invalid shift amount.\n");
                    break;
                }
                while (getchar() != '\n');  // Clear newline
                
                if (shift > 255) {
                    printf("Warning: Shift amount %u > 255, using 255\n", shift);
                    shift = 255;
                }
                
                a = str_to_int256(buffer1);
                result = (choice == 9) ? shift_left_int256(a, shift) 
                                      : shift_right_int256(a, shift);
                
                int256_to_hex(result, result_hex, sizeof(result_hex));
                int256_to_decimal(result, result_dec, sizeof(result_dec));
                printf("\nResult: %s\n", result_dec);
                printf("Hex: %s\n", result_hex);
                break;
                
            case 11:
                printf("Enter base: ");
                fflush(stdout);
                if (fgets(buffer1, sizeof(buffer1), stdin) == NULL) {
                    printf("Input error\n");
                    break;
                }
                buffer1[strcspn(buffer1, "\n")] = '\0';
                
                printf("Enter exponent: ");
                fflush(stdout);
                if (scanf("%u", &power) != 1) {
                    while (getchar() != '\n');
                    printf("Invalid exponent.\n");
                    break;
                }
                while (getchar() != '\n');
                
                a = str_to_int256(buffer1);
                result = pow_int256(a, power);
                
                int256_to_hex(result, result_hex, sizeof(result_hex));
                int256_to_decimal(result, result_dec, sizeof(result_dec));
                printf("\nResult: %s\n", result_dec);
                printf("Hex: %s\n", result_hex);
                break;
                
            case 12:
                printf("Enter n (0-100): ");
                fflush(stdout);
                if (scanf("%u", &factorial_n) != 1) {
                    while (getchar() != '\n');
                    printf("Invalid n.\n");
                    break;
                }
                while (getchar() != '\n');
                
                if (factorial_n > 100) {
                    printf("Warning: Large factorial will be slow!\n");
                }
                
                result = factorial_int256(factorial_n);
                int256_to_decimal(result, result_dec, sizeof(result_dec));
                int256_to_hex(result, result_hex, sizeof(result_hex));
                printf("\n%d! = %s\n", factorial_n, result_dec);
                printf("Hex: %s\n", result_hex);
                break;
                
            case 13:
                printf("Enter first number: ");
                fflush(stdout);
                if (fgets(buffer1, sizeof(buffer1), stdin) == NULL) {
                    printf("Input error\n");
                    break;
                }
                buffer1[strcspn(buffer1, "\n")] = '\0';
                
                printf("Enter second number: ");
                fflush(stdout);
                if (fgets(buffer2, sizeof(buffer2), stdin) == NULL) {
                    printf("Input error\n");
                    break;
                }
                buffer2[strcspn(buffer2, "\n")] = '\0';
                
                a = str_to_int256(buffer1);
                b = str_to_int256(buffer2);
                
                cmp = cmp_int256(a, b);
                printf("\n%s %c %s\n", buffer1, 
                      cmp < 0 ? '<' : cmp > 0 ? '>' : '=', buffer2);
                break;
                
            case 14:
                printf("Enter number: ");
                fflush(stdout);
                if (fgets(buffer1, sizeof(buffer1), stdin) == NULL) {
                    printf("Input error\n");
                    break;
                }
                buffer1[strcspn(buffer1, "\n")] = '\0';
                
                a = str_to_int256(buffer1);
                result = neg_int256(a);
                int256_to_decimal(result, result_dec, sizeof(result_dec));
                printf("\nResult: %s\n", result_dec);
                break;
                
            case 15:
                printf("Enter number: ");
                fflush(stdout);
                if (fgets(buffer1, sizeof(buffer1), stdin) == NULL) {
                    printf("Input error\n");
                    break;
                }
                buffer1[strcspn(buffer1, "\n")] = '\0';
                
                a = str_to_int256(buffer1);
                result = abs_int256(a);
                int256_to_decimal(result, result_dec, sizeof(result_dec));
                printf("\n|%s| = %s\n", buffer1, result_dec);
                break;
                
            default:
                printf("Invalid choice! Please enter 0-15.\n");
        }
    }
    
    printf("\nGoodbye!\n");
    return 0;
}
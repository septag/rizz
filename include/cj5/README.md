# cj5
Very minimal single header JSON5 parser in C99, derived from [jsmn](https://github.com/zserge/jsmn)

## Features
- No memory allocations. All allocatrions are managed on the user side
- Fast and Minimal. 
- standard C library functions (memory,string functions) can be overriden
- Easy to use API (one function to parse)
- Portable C API
- Helper functions to use parsed data in DOM manner

## Usage
The main function to parse json is `cj5_parse`. like jsmn, you provide all tokens to be filled as an array and provide the maximum count
The result will be return in `cj5_result` struct, and `num_tokens` will represent the actual token count that is parsed.  
In case of errors, cj_result.error will be set to an error code. Here's a quick example of the usage, first define CJ5_IMPLEMENT to include the implementation:

```c  
        #define CJ5_IMPLEMENT
        #include "cj5.h"

        cj5_token tokens[32];
        cj5_result r = cj5_parse(g_json, (int)strlen(g_json), tokens, 32);
        
        if (r.error != CJ5_ERROR_NONE) {
            if (r.error == CJ5_ERROR_OVERFLOW) {
                // you can use r.num_tokens to determine the actual token count and reparse
                printf("Error: line: %d, col: %d\n", r.error_line, r.error_code);    
            }
        } else {
            // use token helper functions (see below) to access the values 
            float my_num = cj5_seekget_float(&r, 0, "my_num", 0 /* default value if not found*/ );
        } 
```

**NOTE**: Unlike _jsmn_, if number of parsed tokens exceeds the provided ones, parser doesn't return immediately with an error.
          But parses the JSON to the end, counts all needed tokens and returns with an CJ5_ERROR_OVERFLOW, so the user can 
          choose to reparse the json with new memory requirements.

## Links
- [jsmn](https://github.com/zserge/jsmn) Jsmn is a world fastest JSON parser/tokenizer. This is the official repo replacing the old one at Bitbucket


[License (MIT)](https://github.com/septag/cj5/blob/master/LICENSE)
--------------------------------------------------------------------------

<a href="http://opensource.org/licenses/MIT" target="_blank">
<img align="right" src="http://opensource.org/trademarks/opensource/OSI-Approved-License-100x137.png">


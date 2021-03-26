#include <stdio.h>

/* Input buffer */
static char input[2048];

int main(int argc, char** argv) {
    /* Print Version and Exit Information */
    puts("Lispy Version 0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    while (1) {        
        /* Output the prompt */
        fputs("lispy> ", stdout);
        /* MSYS2 Win 10 workaround for 'stdout' flush issues */
        fflush(stdout);

        /* Read input < input buffer size */
        fgets(input, 2048, stdin);

        /* Echo input back to user */
        printf("No you're a %s", input);
    }    

    return 0;
}
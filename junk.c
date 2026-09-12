#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

int main(int argc, char *argv[])
{
    int c;
    int digit_optind = 0;
    int aflag = 0, bflag = 0;
    char *file = NULL;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"file", required_argument, 0, 'f'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "abf:h", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 0:
                if (long_options[option_index].flag != 0)
                    break;
                printf("option %s", long_options[option_index].name);
                if (optarg)
                    printf(" with arg %s", optarg);
                printf("\n");
                break;

            case 'a':
                aflag = 1;
                break;

            case 'b':
                bflag = 1;
                break;

            case 'f':
                file = optarg;
                break;

            case 'h':
                printf("Usage: %s [-a] [-b] [-f file] [--help]\n", argv[0]);
                exit(EXIT_SUCCESS);

            case '?':
                break;

            default:
                printf("Unknown option %c\n", c);
                exit(EXIT_FAILURE);
        }
    }

    printf("aflag = %d, bflag = %d, file = %s\n", aflag, bflag, file);

    return 0;
}


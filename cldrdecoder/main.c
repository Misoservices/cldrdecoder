//
//  main.c
//  cldrdecoder
//
//  Created by Misoservices Inc. on 2021-01-19.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// From https://rosettacode.org/wiki/UTF-8_encode_and_decode#C
typedef struct {
    char mask;    /* char data will be bitwise AND with this */
    char lead;    /* start bytes of current char in utf-8 encoded character */
    uint32_t beg; /* beginning of codepoint range */
    uint32_t end; /* end of codepoint range */
    int bits_stored; /* the number of bits from the codepoint that fits in char */
}utf_t;

utf_t * utf[] = {
    /*             mask        lead        beg      end       bits */
    [0] = &(utf_t){0b00111111, 0b10000000, 0,       0,        6    },
    [1] = &(utf_t){0b01111111, 0b00000000, 0000,    0177,     7    },
    [2] = &(utf_t){0b00011111, 0b11000000, 0200,    03777,    5    },
    [3] = &(utf_t){0b00001111, 0b11100000, 04000,   0177777,  4    },
    [4] = &(utf_t){0b00000111, 0b11110000, 0200000, 04177777, 3    },
    &(utf_t){0},
};

/* All lengths are in bytes */
int codepoint_len(const uint32_t cp); /* len of associated utf-8 char */
int utf8_len(const char ch);          /* len of utf-8 encoded char */

char *to_utf8(const uint32_t cp);
uint32_t to_cp(const char chr[4]);

int codepoint_len(const uint32_t cp)
{
    int len = 0;
    for(utf_t **u = utf; *u; ++u) {
        if((cp >= (*u)->beg) && (cp <= (*u)->end)) {
            break;
        }
        ++len;
    }
    if(len > 4) /* Out of bounds */
        exit(1);

    return len;
}

int utf8_len(const char ch)
{
    int len = 0;
    for(utf_t **u = utf; *u; ++u) {
        if((ch & ~(*u)->mask) == (*u)->lead) {
            break;
        }
        ++len;
    }
    if(len > 4) { /* Malformed leading byte */
        exit(1);
    }
    return len;
}

char *to_utf8(const uint32_t cp)
{
    static char ret[5];
    const int bytes = codepoint_len(cp);

    int shift = utf[0]->bits_stored * (bytes - 1);
    ret[0] = (cp >> shift & utf[bytes]->mask) | utf[bytes]->lead;
    shift -= utf[0]->bits_stored;
    for(int i = 1; i < bytes; ++i) {
        ret[i] = (cp >> shift & utf[0]->mask) | utf[0]->lead;
        shift -= utf[0]->bits_stored;
    }
    ret[bytes] = '\0';
    return ret;
}

uint32_t to_cp(const char chr[4])
{
    int bytes = utf8_len(*chr);
    int shift = utf[0]->bits_stored * (bytes - 1);
    uint32_t codep = (*chr++ & utf[bytes]->mask) << shift;

    for(int i = 1; i < bytes; ++i, ++chr) {
        shift -= utf[0]->bits_stored;
        codep |= ((char)*chr & utf[0]->mask) << shift;
    }

    return codep;
}


const char* follows(const char* src, const char* pat) {
    const char* result = strstr(src, pat);
    if (!result) {
        return NULL;
    }
    return result + strlen(pat);
}

int main(int argc, const char * argv[]) {
    while (--argc) {
        const char* const sourcefile = argv[argc];
        char destfile[2048];
        strcpy(destfile, sourcefile);
        {
            char* instance = strstr(destfile, ".xml");
            if (instance == NULL)
            {
                printf("Not an XML file: %s\n", sourcefile);
                continue;
            }
            strcpy(instance, ".strings");
        }
        printf("Processing %s to %s: ", sourcefile, destfile);

        FILE* fs = fopen(sourcefile, "rb");
        FILE* fd = fopen(destfile, "w");
        if (!fs || !fd) {
            printf("Error opening files");
            return -1;
        }
        char line[4096];
        int processed = 0;
        while (fgets(line, sizeof(line), fs)) {
            const char* annotation = follows(line, "<annotation cp=\"");
            if (!annotation) continue;
            const char* annotationEnd = strstr(annotation, "\"");
            if (!annotationEnd) continue;
            if (annotation == annotationEnd) continue;

            int isTTS = strstr(annotationEnd, "type=\"tts\"") != NULL;

            const char* text = follows(annotationEnd, ">");
            if (!text) continue;
            const char* textEnd = strstr(text, "</annotation>");
            if (!textEnd) continue;
            size_t textLen = textEnd - text;

            fprintf(fd, "\"");
            while (annotation < annotationEnd) {
                uint32_t cp = to_cp(annotation);
                annotation += utf8_len(*annotation);
                fprintf(fd, "%X", cp);
                if (annotation < annotationEnd) {
                    fprintf(fd, " ");
                }
            }
            if (!isTTS) {
                fprintf(fd, "-Tags");
            }
            fprintf(fd, "\" = \"");
            fwrite(text, 1, textLen, fd);
            fprintf(fd, "\";\n");

            ++processed;
        }
        printf("%d processed.\n", processed);
        fclose(fs);
        fclose(fd);
    }
    return 0;
}

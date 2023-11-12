#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* line.h not ANSI- */
#ifdef __STDC_VERSION__
# define LINE_IMPLEMENTATION
# include "line.h"
#else
static char _static_line[2048];
# define line_read() gets(_static_line)
# define line_free()
#endif

#define KHOR_IMPLEMENTATION
#include "khor.h"

void dump(khor_object const* self) {
    size_t k;
    switch (self->ty) {
        case KHOR_NUMBER:
            printf("%f", self->num.val);
            break;
        case KHOR_STRING: {
                char* last = self->str.ptr;
                printf("\"");
                for (k = 0; k < self->str.len; k++) {
                    char c;
                    switch (self->str.ptr[k]) {
                        case    7: c = 'a';  break;
                        case    8: c = 'b';  break;
                        case   27: c = 'e';  break;
                        case   12: c = 'f';  break;
                        case   10: c = 'n';  break;
                        case   13: c = 'r';  break;
                        case    9: c = 't';  break;
                        case   11: c = 'v';  break;
                        case  '"': c = '"';  break;
                        case '\\': c = '\\'; break;
                        default: continue;
                    }
                    printf("%.*s\\%c", (int)(self->str.ptr+k-last), last, c);
                    last = self->str.ptr+k+1;
                }
                printf("%.*s\"", (int)(self->str.ptr+k-last), last);
            }
            break;
        case KHOR_LIST:
            if (0 == self->lst.len) printf("()");
            else {
                printf("(list");
                for (k = 0; k < self->lst.len; k++) {
                    printf(" ");
                    dump(self->lst.ptr+k);
                }
                printf(")");
            }
            break;
        case KHOR_LAMBDA:
            printf("(lambda (..%u) <..>)", self->lbd.ary);
            break;
        default:
            printf("%s", self->sym.txt);
            break;
    }
}

void dumpcode(khor_bytecode const* code) {
    size_t k;
    for (k = 0; k < code->len; k++) {
        if (' ' <= code->ptr[k] && code->ptr[k] <= '~')
            printf( "[%5lu] 0x%02X %c\n", k, code->ptr[k] & 255, code->ptr[k]);
        else printf("[%5lu] 0x%02X\n",    k, code->ptr[k] & 255);
    }
}

void dumpenv(khor_environment const* env) {
    size_t k;
    for (k = 0; k < env->entries.len; k++) {
        khor_enventry const* it = env->entries.ptr+k;
        static char const* tynames[] = {"Number", "String", "List", "Lambda", "Symbol"};
        printf("%s :: %s\n", it->key.txt, tynames[it->value.ty < 4 ? it->value.ty : 4]);
    }
}

void dumpmacros(khor_ruleset const* macros) {
    size_t i, j, k;
    for (k = 0; k < macros->len; k++) {
        khor_rules const* it = macros->ptr+k;
        printf("%s :=\n", it->key.txt);
        for (j = 0; j < it->rules.len; j++) {
            printf("\t");
            for (i = 0; i < it->rules.ptr[j].names.len; i++)
                printf(" %s", it->rules.ptr[j].names.ptr[i].txt);
            printf(" -> ");
            dump(&it->rules.ptr[j].subst);
            puts("");
        }
    }
}

bool thandle(khor_bytecode* code, khor_environment* env, khor_stack* stack, unsigned* cp, unsigned* sp, char w) {
    bool so;
    (void)env;
    printf("- interrupted (%d) -\n", w);
    printf("next [%u] 0x%02X\n", *cp+1, code->ptr[*cp+1]);
    printf("top %u/%lu ", *sp, stack->len);
    if (*sp) dump(stack->ptr+*sp-1);
    so = stack->len == *sp;
    if (so) puts("stack overflow!");
    getchar();
    puts("- end -");
    return !so;
}

int main(void) {
    size_t k;
    khor_slice source;
    khor_object ast;
    khor_ruleset macros = {0};
    khor_bytecode code = {0};
    khor_environment env = {0};
    khor_stack stack = {0};

    (void)dyarr_insert(&stack, 0, 1024);

    while ((source.ptr = line_read())) if (*source.ptr) {
#       define cmdeq(__c) (!memcmp(__c, source.ptr, strlen(__c)))
        bool showast = false, showbc = false, norun = false;

        if (cmdeq(".help")) {
            puts("list of command:\n\t.help\n\t.env\n\t.macros\n\t.quit\n\t.ast <source>\n\t.bc <source>\n\t. <source>");
            continue;
        } else if (cmdeq(".env")) {
            puts("{{{ env:");
            dumpenv(&env);
            puts("}}}");
            continue;
        } else if (cmdeq(".macros")) {
            puts("{{{ macros:");
            dumpmacros(&macros);
            puts("}}}");
            continue;
        } else if (cmdeq(".quit")) break;

        if ((showast = cmdeq(".ast"))) source.ptr+= 4;
        if ((showbc = cmdeq(".bc"))) source.ptr+= 3;
        if ((norun = cmdeq(". "))) source.ptr++;

        if (cmdeq(".")) {
            puts("unknown command");
            continue;
        }
#       undef cmdeq

        source.len = strlen(source.ptr);
        ast = khor_parse(&source, &macros);

        if (showast) {
            puts("{{{ ast:");
            dump(&ast);
            puts("\n}}}");
        }

        khor_compile(&ast, &code);
        khor_destroy(&ast);

        if (showbc) {
            puts("{{{ bc:");
            dumpcode(&code);
            puts("}}}");
        }

        if (norun) {
            dyarr_clear(&code);
            continue;
        }

        khor_eval(&code, &env, &stack, thandle);
        dyarr_clear(&code);

        dump(stack.ptr);
        puts("");
        khor_destroy(stack.ptr);
    }
    line_free();

    for (k = 0; k < macros.len; k++) {
        size_t j;
        for (j = 0; j < macros.ptr[k].rules.len; j++) {
            khor_destroy(&macros.ptr[k].rules.ptr[j].subst);
            free(macros.ptr[k].rules.ptr[j].names.ptr);
        }
        free(macros.ptr[k].rules.ptr);
    }
    for (k = 0; k < env.entries.len; k++)
        khor_destroy(&env.entries.ptr[k].value);

    return 0;
}

#include <stdio.h> 
#include <ctype.h> 

int print_1(int argc, char **argv);
int print_2(int argc, char **argv);
int print_3(int argc, char **argv);

int main(int argc, char **argv) {
   return print_3(argc, argv);
}

int print_1(int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        char *arg = argv[i];
        printf("%s\t%p\n", arg, arg);
    }

    return 0;
}

// 从每个参数对应的起始地址开始 都往后输出 15 个字符
int print_2(int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        char *arg = argv[i];
        for (size_t j = 0; j < 15; j++) {
            printf("%c", arg[j]);
        }
        printf("\n");
    }

    return 0;
}

int print_3(int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        char *arg = argv[i];
        for (size_t j = 0; j < 15; j++) {
            if (arg[j] == 0) break;
            printf("%c", arg[j]);
        }
        printf("\n");
    }

    return 0;
}

int len(const char *s) {
    // s[0] = '\0';
    char *s1 = (void *)s;
    s1[0] = '\0';
    return 0;
}

/*
## 编译
* `gcc 1.print.c -o print`
* 运行 `./print 123 "ready" "set" "go"`
* 运行 `./print 123 "ready" "set" "go" | xxd -g 1`




*/

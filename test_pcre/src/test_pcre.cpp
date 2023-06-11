#include <sys/types.h>
#include <regex.h>
#include <stdio.h>
#include <cstring>
#include <regex>
#include <string>
#include <iostream>

int regcomp(regex_t *preg, const char *regex, int cflags);

int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags);

size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size);

void regfree(regex_t *preg);

void test_posix_regex();

void test_cpp_regex();

int main() {
    setbuf(stdout, NULL);
    test_posix_regex();
    test_cpp_regex();
    return 0;
}

void test_cpp_regex() {
    const char * pat1 = "(([a-z]{3})([0-9]+))";
    std::string str = "abc1231abcabc45678";
    std::regex pattern(pat1, std::regex::extended);
    std::regex pattern2(str);
    std::smatch res;
    bool ret = std::regex_search(str, res, pattern);
    if (ret) {
        std::cout << "AAAAAA" << std::endl;
    }
    std::for_each(res.begin(), res.end(), [](auto & x) {
        std::cout << x << std::endl;
    });
    ret = std::regex_match(str, res, pattern2);
    if (ret) {
        std::cout << "BBBBBB" << std::endl;
    }
    auto it = std::sregex_iterator(str.begin(), str.end(), pattern);
    while (it != std::sregex_iterator()) {
        std::cout << it->str() << std::endl;
        for (auto y = it->begin(); y != it->end(); ++y) {
            std::cout << *y << std::endl;
        }
        ++it;
    }
}


void test_posix_regex() {
    regex_t regex;
    int ret = regcomp(&regex, "(([a-z]{3})([0-9]+))", REG_EXTENDED);
    if (ret) {
        return;
    }
    const char *str = "abc1231abcabc45678";
    char buf[128] = {0};
//    测试strncpy memcpy
//    char buf1[] = {'a', 'b', 0, 'c', 'd'};
//    char buf2[128] = {0};
//    strncpy(buf, buf1, 4);
//    memcpy(buf2, buf1, 4);
    regmatch_t pmatch[5];
    const char *pos = str;
    while (pos < str + strlen(str)) {
        ret = regexec(&regex, pos, 5, pmatch, REG_NOTBOL);
        if (ret) {
            perror("regexec");
            break;
        }
        strncpy(buf, pos + pmatch[0].rm_so, pmatch[0].rm_eo - pmatch[0].rm_so);
        printf("%s, %s\n", pos, buf);
        pos += pmatch[0].rm_eo;
    }
    regfree(&regex);
}
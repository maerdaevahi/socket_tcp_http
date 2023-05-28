#include <json/json.h>
#include <fstream>
#include <iostream>

int testJson1(char *const *argv);

/** \brief Parse from stream, collect comments and capture error info.
 * Example Usage:
 * $g++ readFromStream.cpp -ljsoncpp -std=c++11 -o readFromStream
 * $./readFromStream
 * // comment head
 * {
 *    // comment before
 *    "key" : "value"
 * }
 * // comment after
 * // comment tail
 */
int main(int argc, char* argv[]) {
    return testJson1(argv);
}

int testJson1(char *const *argv) {
    Json::Value root;
    std::ifstream ifs;
    ifs.open(argv[1]);

    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    JSONCPP_STRING errs;
    if (!parseFromStream(builder, ifs, &root, &errs)) {
        std::cout << errs << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << root << std::endl;
    std::cout << root["key"].isString() << std::endl;
    std::cout << root["fruits"][1] << std::endl;
    std::cout << root["ints"][1] << std::endl;
    std::cout << root.isObject() << "|" << root.isArray() << std::endl;

    return EXIT_SUCCESS;
}

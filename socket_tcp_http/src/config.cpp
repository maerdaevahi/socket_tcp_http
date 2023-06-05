#include "config.h"
#include <json/json.h>
#include <fstream>
#include <iostream>

server_config * parse_config(const char * pathname) {
    Json::Value root;
    std::ifstream ifs;
    ifs.open(pathname);
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    JSONCPP_STRING errs;
    if (!parseFromStream(builder, ifs, &root, &errs)) {
        std::cout << errs << std::endl;
        return NULL;
    }
    server_config * cfg = (server_config *) malloc(sizeof(server_config));
    strcpy(cfg->ip, root["ip"].asString().c_str());
    cfg->port = root["port"].asInt();
    strcpy(cfg->cwd, root["cwd"].asString().c_str());
    return cfg;
}
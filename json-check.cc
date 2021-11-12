#include "json.h"

#include <iostream>

json::value jv;

bool check(const std::string& filename)
{
    if (!jv.parsefile(filename)) {
        std::cout << "check failed: " << filename.c_str() << ": " << jv.as_string().c_str() << "\n";
        return false;
    }
    std::cout << "check pass " << filename.c_str() << "\n";
    if (filename.rfind("fail1.json") != std::string::npos) {
        std::cout << "(Now it can be any JSON value)\n";
    } else if (filename.rfind("fail18.json") != std::string::npos) {
        std::cout << "(We have no maximum nesting depth limit)\n";
    }
    return true;
}

int main()
{
    char buf[32];
    std::string check_dir = "json-checker/";
    std::cout << "============CHECK fail1.json ~ fail33.json============\n";
    for (int i = 1; i <= 33; i++) {
        sprintf(buf, "fail%d.json", i);
        check(check_dir + buf);
    }
    std::cout << "============CHECK pass1.json ~ pass3.json============\n";
    for (int i = 1; i <= 3; i++) {
        sprintf(buf, "pass%d.json", i);
        check(check_dir + buf);
    }
}

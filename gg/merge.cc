#include <iostream>
#include <fstream>
#include <unistd.h>

using namespace std;

int main (int argc, char *argv[]) {
    string res = "UNSAT";
    for (size_t i = 1; i < argc; ++i) {
        string content;
        ifstream in{argv[i]};
        in >> content;
        if (content != "UNSAT" and content != "SAT") {
            res = content;
        } else if (content != "UNSAT") {
            res = content;
        } else {
            res = content;
        }
    }
    ofstream o{"out"};
    o << res;
}

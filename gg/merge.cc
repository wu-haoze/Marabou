#include <iostream>
#include <fstream>
#include <unistd.h>

using namespace std;

int main (int argc, char *argv[]) {
    ofstream o{"out"};
    for (size_t i = 1; i < argc; ++i) {
        string content;
        ifstream in{argv[i]};
        in >> content;
        if (content == "SAT") {
            o << "SAT\n";
            o << in.rdbuf();
            return EXIT_SUCCESS;
        }
    }
    o << "UNSAT\n";
    return EXIT_SUCCESS;
}

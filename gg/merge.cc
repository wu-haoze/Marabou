#include <cstring>
#include <fstream>
#include <iostream>
#include <unistd.h>

using namespace std;

bool is_thunk(const string& path)
{
    constexpr char magic_number[] = "##GGTHUNK##";
    constexpr size_t magic_number_len = sizeof(magic_number) / sizeof(char) - 1;
    char first_bytes[magic_number_len] = { 0 };

    ifstream fin{ path };
    fin.read(first_bytes, magic_number_len);

    return (strncmp(magic_number, first_bytes, magic_number_len) == 0);
}

int main(int argc, char* argv[])
{
    ofstream o{ "out" };
    bool some_thunk_remains = false;
    for (int i = 1; i < argc; i++) {
        if (is_thunk(argv[i])) {
            some_thunk_remains = true;
        } else {
            ifstream fin{ argv[i] };
            string content;
            fin >> content;
            if (content == "SAT") {
                o << "SAT\n";
                o << fin.rdbuf();
                return EXIT_SUCCESS;
            }
        }
    }
    if (some_thunk_remains) {
        ifstream tin{ getenv("__GG_THUNK_PATH__") };
        o << tin.rdbuf();
    } else {
        o << "UNSAT\n";
    }
    return EXIT_SUCCESS;
}

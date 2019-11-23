#ifndef TOOLCHAIN_HH
#define TOOLCHAIN_HH

#include <sys/types.h>
#include <unordered_map>
#include <string>
#include <vector>

#include "thunk/thunk.hh"
#include "thunk/factory.hh"
#include "util/path.hh"

#define TOOLCHAIN_PATH "/home/aozdemir/repos/gg/toolchain/bin"

const std::string & program_hash( const std::string & name );
extern const std::vector<std::string> c_include_path;
extern const std::vector<std::string> cxx_include_path;
extern const std::vector<std::string> ld_search_path;
extern const std::vector<std::string> gcc_library_path;
extern const std::string gcc_install_path;

extern const std::string GCC;
extern const std::string GXX;
extern const std::string AS ;
extern const std::string CC1;
extern const std::string CC1PLUS;
extern const std::string COLLECT2;
extern const std::string LD;
extern const std::string AR;
extern const std::string RANLIB;
extern const std::string STRIP;
extern const std::string NM;
extern const std::string READELF;
extern const std::string GG_BIN_PREFIX;
extern const roost::path toolchain_path;

extern const std::unordered_map<std::string, ThunkFactory::Data> program_data;

#endif /* TOOLCHAIN_HH */

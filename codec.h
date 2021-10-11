#ifndef SIK2_ENCODER
#define SIK2_ENCODER

#include <cstdint>
#include <vector>
#include <string>
#include <zconf.h>
#include <stdexcept>

class CodecError : public std::runtime_error {
public:
    CodecError(char const *err);
};

class Codec {
private:
    std::vector<char> data;
    size_t pos;
public:    
    Codec(size_t len = 0);

    char* get_data();
    void* get_pos();
    size_t get_len();

    void skip_pos(size_t skipped_pos);

    bool has_data();

    void trim(size_t len);

    void add_uint8_t(uint8_t);
    uint8_t read_uint8_t();

    void add_uint32_t(uint32_t);
    uint32_t read_uint32_t();

    void add_uint64_t(uint64_t);
    uint64_t read_uint64_t();

    void add_string(std::string const &val, bool with_0char);
    std::string read_string(bool until_0char);

    void add(void* data, size_t len);
    void add_first(void* data, size_t len);
    void add_first(uint32_t);

    std::string read_str(size_t len);
    uint32_t read_crc(size_t len);
};

#endif //SIK2_ENCODER
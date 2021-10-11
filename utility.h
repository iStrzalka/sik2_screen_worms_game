#ifndef SK2_UTILITY
#define SK2_UTILITY

#include <cstdint>
#include <sstream>
#include <unistd.h>
#include <exception>
#include <string>
#include <stdexcept>
#include <iostream>
#include <sys/time.h>
#include <signal.h>

void build_crc32_table();

class UtilityError : public std::exception {
public:
    UtilityError(char const *err);
    virtual char const * what() const noexcept { return error; }
private:
    char const *error;
};

uint32_t string_to_uint32_t(std::string);

void validate_playername(std::string);

uint32_t get_crc32(const void* buffer, size_t len);
bool check_crc32(void *buffer, ssize_t len, uint32_t crc);

uint64_t get_timestamp();


#endif //SK2_UTILITY
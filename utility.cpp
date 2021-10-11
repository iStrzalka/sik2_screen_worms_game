#include "utility.h"

static uint32_t crc32_table[256];

UtilityError::UtilityError(char const *err) {
    error = std::move(err);
}

/* Changes string to uint32_t. Throws UtilityError on error.*/
uint32_t string_to_uint32_t(std::string str) {
    uint32_t ret;
    while (str.length() > 0 && str[0] == ' ')
        str = str.substr(1, str.length() - 1);
    std::istringstream istream{str};
    istream >> ret;
    if (istream.fail() || std::to_string(ret) != str) {
        throw UtilityError("Expected integer as an argument");
    } 
    return ret;
}

/* Validates playername, throws UtilityError on error. */
void validate_playername(std::string name) {
    if (name.length() > 20)
        throw UtilityError("Player name is too long\n");
    for(size_t i = 0; i < name.length(); i++) {
        if(name[i] < 33 || name[i] > 126) {
            throw UtilityError("Player name is invalid\n");
        }
    }
}

/* Returns timestamp in microseconds. */
uint64_t get_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * (uint64_t) 1000000 + tv.tv_usec;
}

/* Builds up the crc32 table. */
void build_crc32_table() {
	for(uint32_t i = 0; i < 256; i++) {
		uint32_t ch = i;
		uint32_t crc = 0;
		for(size_t j = 0; j < 8;j++) {
			uint32_t b=(ch^crc)&1;
			crc>>=1;
			if (b)
                crc=crc^0xEDB88320;
			ch>>=1;
		}
		crc32_table[i] = crc;
	}
}

/* Calculates and returns crc32 value. */
uint32_t get_crc32(const void *s,size_t n) {
    auto c = (char *) s;
	uint32_t crc=0xFFFFFFFF;
	
	for(size_t i = 0;i < n;i++) {
		char ch = c[i];
		uint32_t t=(ch^crc)&0xFF;
		crc=(crc>>8)^crc32_table[t];
	}
	
	return ~crc;
}

/* Performs check whether crc32 is valid. */
bool check_crc32(void *buffer, ssize_t len, uint32_t crc) {
    uint32_t crc32 = get_crc32(buffer, len);
    return crc32 == crc;
}
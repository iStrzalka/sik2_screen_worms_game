#include "codec.h"
#include "utility.h"

CodecError::CodecError(char const *err) : std::runtime_error(err) {}

Codec::Codec(size_t len) {
    pos = 0;
    data.resize(len);
}

void Codec::skip_pos(size_t skipped_pos) {
    pos += skipped_pos;
}

void Codec::add(void *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        this->data.push_back(((char *) data)[i]);
    }
}

void Codec::add_uint8_t(uint8_t val) {
    add(&val, sizeof(val));
}

uint8_t Codec::read_uint8_t() {
    uint8_t val = *(uint8_t *)(data.data() + pos);
    pos++;
    return val;
}

void Codec::add_uint32_t(uint32_t val) {
    val = htobe32(val);
    add(&val, sizeof(val));
}

uint32_t Codec::read_uint32_t() {
    if (pos > data.size() - 4)
        throw CodecError("Invalid datagram\n");
    uint32_t val = *(uint32_t *)(data.data() + pos);
    pos += 4;
    return be32toh(val);
}

void Codec::add_uint64_t(uint64_t val) {
    val = htobe64(val);
    add(&val, sizeof(val));
}

uint64_t Codec::read_uint64_t() {
    if (pos > data.size() - 8)
        throw CodecError("Invalid datagram\n");
    uint64_t val = *(uint64_t *)(data.data() + pos);
    pos += sizeof(val);
    return be64toh(val);
}

void Codec::add_string(std::string const &val, bool with_0char) {
    add ((void *) val.c_str(), val.length());
    if (with_0char) {
        char zero_char = 0;
        add(&zero_char, 1);
    }
}

std::string Codec::read_string(bool until_0char) {
    std::string ret;

    while (has_data() && (*(data.data() + pos) != 0 || !until_0char)) {
        ret += *(data.data() + pos);
        pos++;
    }

    if (until_0char)
        pos++;
    return ret;
}

std::string Codec::read_str(size_t len) {
    std::string str;
    size_t i = 0;
    while (has_data() && i < len) {
        str += *(data.data()+pos);
        pos++;
        i++;
    }

    return str;
}

uint32_t Codec::read_crc(size_t len) {
    if (pos + len >= data.size())
        throw CodecError("Invalid datagram\n");
    uint32_t val = *(uint32_t*)(data.data()+pos+len);
    return be32toh(val);
}

void Codec::trim(size_t len) {
    if (data.size() > len)
        data.resize(len);
}

bool Codec::has_data() {
    return pos < data.size();
}

size_t Codec::get_len() {
    return data.size();
}

void* Codec::get_pos() {
    return (void*)(data.data()+pos);
}

char *Codec::get_data() {
    return data.data();
}

void Codec::add_first(void* val, size_t len) {
    for (int64_t i = len - 1; i >= 0; --i)
        this->data.insert(data.begin(), ((char*)val)[i]);
}

void Codec::add_first(uint32_t val) {
    uint32_t v = htobe32(val);
    add_first(&v, sizeof(uint32_t));
}

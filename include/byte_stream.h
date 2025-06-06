#pragma once

#include <Arduino.h>

class ByteStream {
public:
    ByteStream(uint8_t* buffer, size_t max_length) : _buffer(buffer), _max_length(max_length), _read_cursor(0), _write_cursor(0) {}

    // write
    
    void write_uint8(uint8_t value) {
        if (_write_cursor + sizeof(value) > _max_length) return;

        _buffer[_write_cursor++] = value;
    };
    void write_uint16(uint16_t value) {
        if (_write_cursor + sizeof(value) > _max_length) return;

        _buffer[_write_cursor++] = value & 0xFF;
        _buffer[_write_cursor++] = (value >> 8) & 0xFF;
    };
    void write_uint32(uint32_t value) {
        if (_write_cursor + sizeof(value) > _max_length) return;

        for (int i = 0; i < 4; ++i) {
            _buffer[_write_cursor++] = (value >> (8 * i)) & 0xFF;
        }
    };
    void write_uint64(uint64_t value) {
        if (_write_cursor + sizeof(value) > _max_length) return;

        for (int i = 0; i < 8; ++i) {
            _buffer[_write_cursor++] = (value >> (8 * i)) & 0xFF;
        }
    };

    bool can_write_bytes(size_t size) {
        return _write_cursor + size <= _max_length;
    };

    // read

    uint8_t read_uint8() {
        return _buffer[_read_cursor++];
    };
    uint16_t read_uint16() {
        uint16_t value = _buffer[_read_cursor] | (_buffer[_read_cursor + 1] << 8);
        _read_cursor += 2;
        return value;
    };
    uint32_t read_uint32() {
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            value |= ((uint32_t)_buffer[_read_cursor + i] << (8 * i));
        }
        _read_cursor += 4;
        return value;
    };
    uint64_t read_uint64() {
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= ((uint64_t)_buffer[_read_cursor + i] << (8 * i));
        }
        _read_cursor += 8;
        return value;
    };

    bool can_read_bytes(size_t size) {
        return _read_cursor + size <= _max_length;
    };

private:
    uint8_t* _buffer;
    size_t _max_length;
    size_t _read_cursor;
    size_t _write_cursor;
};
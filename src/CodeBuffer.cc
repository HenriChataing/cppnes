
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sys/mman.h>

#include "CodeBuffer.h"

#define PAGE_SIZE       UINTMAX_C(0x1000)

CodeBuffer::CodeBuffer(size_t capacity)
{
    _data = NULL;
    _capacity = ((capacity + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
    _length = 0;
    int r;

#if 0
    _data = (u8 *)mmap(NULL, _capacity,
        PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (_data == MAP_FAILED) {
        std::cerr << "Cannot allocate mmap memory" << std::endl;
        throw "Emitter mmap failure";
    }
#else
    /* Allocate page aligned code buffer. */
    r = posix_memalign((void **)&_data, PAGE_SIZE, _capacity);
    if (r != 0) {
        std::cerr << "Cannot allocate page aligned memory ";
        if (r == EINVAL)
            std::cerr << "(Invalid alignmnt)";
        if (r == ENOMEM)
            std::cerr << "(Out of memory)";
        std::cerr << std::endl;
        throw "CodeBuffer allocation failure";
    }

    /*
     * Change access permissions to code buffer to enable executing
     * code inside.
     */
    r = mprotect(_data, _capacity, PROT_WRITE | PROT_READ | PROT_EXEC);
    if (r < 0) {
        std::cerr << "Cannot change memory permissions for buffer ";
        std::cerr << _data << " (" << strerror(errno) << ")" << std::endl;
        throw "CodeBuffer mprotect failure";
    }
#endif
}

CodeBuffer::~CodeBuffer()
{
    free(_data);
}

CodeBuffer &CodeBuffer::writeb(u8 byte)
{
    if (_length + 1 > _capacity)
        throw "CodeBuffer overflow";
    _data[_length] = byte;
    _length++;
    return *this;
}

CodeBuffer &CodeBuffer::writeh(u16 half)
{
    if (_length + 2 > _capacity)
        throw "CodeBuffer overflow";
    _data[_length]     = half & UINT8_C(0xff);
    _data[_length + 1] = (half >> 8) & UINT8_C(0xff);
    _length += 2;
    return *this;
}

CodeBuffer &CodeBuffer::writew(u32 word)
{
    if (_length + 4 > _capacity)
        throw "CodeBuffer overflow";
    _data[_length]     = word & UINT8_C(0xff);
    _data[_length + 1] = (word >> 8) & UINT8_C(0xff);
    _data[_length + 2] = (word >> 16) & UINT8_C(0xff);
    _data[_length + 3] = (word >> 24) & UINT8_C(0xff);
    _length += 4;
    return *this;
}

void CodeBuffer::dump(const u8 *start) const
{
    if (start == NULL)
        start = _data;
    if ((uintptr_t)start < (uintptr_t)_data ||
        (uintptr_t)start >= (uintptr_t)(_data + _length))
        return;

    std::cout << std::hex << std::setfill('0');
    std::cout << "== " << (uintptr_t)start << std::endl;
    u8 *end = _data + _length;
    for (uint i = 0; start < end; start++, i++) {
        if (i && !(i % 32))
            std::cout << std::endl;
        std::cout << " " << std::setw(2) << (uint)start[0];
    }
    std::cout << std::endl;
}

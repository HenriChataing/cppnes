
#ifndef _CODEBUFFER_H_INCLUDED_
#define _CODEBUFFER_H_INCLUDED_

#include <type.h>

class CodeBuffer
{
public:
    CodeBuffer(size_t capacity = 0x100000);
    ~CodeBuffer();

    bool isEmpty() const { return _length == 0; }
    bool isFull() const { return _length >= _capacity; }
    u8 *getPtr() { return _data + _length; }
    const u8 *getPtr() const { return _data + _length; }
    size_t getLength() const { return _length; }

    CodeBuffer &writeb(u8 byte);
    CodeBuffer &writeh(u16 half);
    CodeBuffer &writew(u32 word);

    void dump(const u8 *start = NULL) const;

private:
    u8 *_data;
    size_t _length;
    size_t _capacity;
};

#endif /* _CODEBUFFER_H_INCLUDED_ */

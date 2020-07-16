#pragma once

typedef struct BUFFER {
	size_t size;
	void* buffer;
}BUFFER, *PBUFFER;

void InitBuffer(PBUFFER pBuffer, size_t size);
void FreeBuffer(PBUFFER pBuffer);


template<typename T>
class CBuffer {
public:
	CBuffer();
	CBuffer(const CBuffer&) = delete;
	CBuffer(const CBuffer&&);
	CBuffer(size_t size);
	~CBuffer();
	operator T*();
	CBuffer& operator=(const CBuffer&) = delete;
	void operator=(CBuffer&&);
	T* reset();

	size_t size;
	T* buffer;
	bool allocated;
};
#include "CBuffer.tpp"
typedef CBuffer<char> CCharBuffer;
typedef CBuffer<unsigned char> CUcharBuffer;

#include "stdafx.h"
#include "Buffer.h"
#include <stdlib.h>

void InitBuffer(PBUFFER pBuffer, size_t size) {
	pBuffer->size = size;
	pBuffer->buffer = malloc(size);
}

void FreeBuffer(PBUFFER pBuffer) {
	free(pBuffer->buffer);
}
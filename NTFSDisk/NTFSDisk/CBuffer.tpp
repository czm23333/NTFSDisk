template<typename T>
CBuffer<T>::CBuffer() {}

template<typename T>
CBuffer<T>::CBuffer(const CBuffer<T>&& rBuf) {
	size = rBuf.size;
	buffer = rBuf.buffer;
	allocated = rBuf.allocated;
}

template<typename T>
CBuffer<T>::CBuffer(size_t size) {
	this->size = size;
	this->buffer = (T*) malloc(size * sizeof(T));
	this->allocated = true;
}

template<typename T>
CBuffer<T>::~CBuffer() {
	if(allocated) {
		allocated = false;
		free((void*) buffer);
	}
}

template<typename T>
CBuffer<T>::operator T*() {
	return buffer;
}

template<typename T>
void CBuffer<T>::operator=(CBuffer<T>&& rBuf) {
	if (allocated)
		free(buffer);

	size = rBuf.size;
	allocated = rBuf.allocated;
	buffer = rBuf.reset();
}

template<typename T>
T* CBuffer<T>::reset() {
	size = 0;
	allocated = false;
	T* temp = buffer;
	buffer = nullptr;
	return temp;
}
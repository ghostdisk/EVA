#pragma once
#include <EVA/Core/Common.hpp>
#include <initializer_list>

#include <vcruntime_new.h>
// #include <new>

#ifndef __NOTHROW_T_DEFINED
#define __NOTHROW_T_DEFINED
    namespace std {
        _VCRT_EXPORT_STD struct nothrow_t { explicit nothrow_t() = default; };
        _VCRT_EXPORT_STD extern nothrow_t const nothrow;
    }
#endif

_VCRT_EXPORT_STD _NODISCARD _Ret_notnull_ _Post_writable_byte_size_(_Size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t _Size);
_VCRT_EXPORT_STD _NODISCARD _Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(_Size) _VCRT_ALLOCATOR
void* __CRTDECL operator new( size_t _Size, ::std::nothrow_t const& ) noexcept;
_VCRT_EXPORT_STD _NODISCARD _Ret_notnull_ _Post_writable_byte_size_(_Size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[]( size_t _Size );
_VCRT_EXPORT_STD _NODISCARD _Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(_Size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[]( size_t _Size, ::std::nothrow_t const& ) noexcept;

template <typename T>
class Vector {
public:
	T*  m_data     = nullptr;
	U32 m_capacity = 0;
	U32 m_size     = 0;

	Vector() {
	}


	Vector(const T* data, U32 size) {
		resize(size);
		for (U32 i = 0; i < size; i++) {
			m_data[i] = data[i];
		}
	}

	Vector(std::initializer_list<T> list) : Vector(list.data(), list.size()) {}
	Vector(const T* begin, const T* end) : Vector(begin, (U32)(end - begin)) {}

	Vector(size_t size) {
		resize(size);
	}

	Vector(const Vector& other) {
		copy_from(other);
	}

	Vector(Vector&& other) {
		move_from(Move(other));
	}

	void operator=(const Vector& other) {
		assert(this != &other);
		destroy();
		copy_from(other);
	}

	void operator=(Vector&& other) {
		assert(this != &other);
		destroy();
		move_from(Move(other));
	}

	~Vector() {
		destroy();
	}

	void push_back(T&& value) {
		if (m_size == m_capacity) {
			set_capacity(m_capacity == 0 ? 8 : m_capacity * 2);
		}
		new (m_data + m_size) T(Move(value));
		m_size++;
	}

	void push_back(const T& value) {
		if (m_size == m_capacity) {
			set_capacity(m_capacity == 0 ? 8 : m_capacity * 2);
		}
		new (m_data + m_size) T(value);
		m_size++;
	}

	T* erase(const T* position) {
		assert(position >= begin() && position < end());
		U32 index = (U32)(position - begin());

		m_data[index].~T();
		for (U32 i = index; i + 1 < m_size; i++) {
			new (m_data + i) T(Move(m_data[i + 1]));
			m_data[i + 1].~T();
		}
		m_size--;
		return m_data + index;
	}

	T* insert(const T* position, const T& value) {
		T copy(value);
		return insert(position, Move(copy));
	}

	T* insert(const T* position, T&& value) {
		assert((m_size == 0 && position == begin()) || (position >= begin() && position <= end()));
		U32 index = m_size == 0 ? 0 : (U32)(position - begin());
		T movedValue(Move(value));

		if (m_size == m_capacity) {
			set_capacity(m_capacity == 0 ? 8 : m_capacity * 2);
		}
		for (U32 i = m_size; i > index; i--) {
			new (m_data + i) T(Move(m_data[i - 1]));
			m_data[i - 1].~T();
		}
		new (m_data + index) T(Move(movedValue));
		m_size++;
		return m_data + index;
	}

	T* insert(const T* position, const T* first, const T* last) {
		assert((m_size == 0 && position == begin()) || (position >= begin() && position <= end()));
		assert(first <= last);
		U32 index = m_size == 0 ? 0 : (U32)(position - begin());

		Vector<T> values;
		for (const T* it = first; it != last; it++) {
			values.push_back(*it);
		}
		if (values.empty()) return m_data + index;

		U32 requiredCapacity = m_size + values.m_size;
		if (requiredCapacity > m_capacity) {
			U32 newCapacity = m_capacity == 0 ? 8 : m_capacity;
			while (newCapacity < requiredCapacity) newCapacity *= 2;
			set_capacity(newCapacity);
		}
		for (U32 i = m_size; i > index; i--) {
			new (m_data + i - 1 + values.m_size) T(Move(m_data[i - 1]));
			m_data[i - 1].~T();
		}
		for (U32 i = 0; i < values.m_size; i++) {
			new (m_data + index + i) T(Move(values[i]));
		}
		m_size += values.m_size;
		return m_data + index;
	}

	T pop_back() {
		assert(m_size > 0);
		T value = Move(m_data[m_size - 1]);
		m_size--;
		return value;
	}

	void set_capacity(U32 newCapacity) {
		U32 oldCapacity = m_capacity;
		U32 oldSize = m_size;
		T* oldData = m_data;

		U32 newSize = oldSize;
		if (newSize > newCapacity) newSize = newCapacity;

		T* newData = (T*)malloc(sizeof(T) * newCapacity);
		for (U32 i = 0; i < newSize; i++) {
			new (newData + i) T(Move(oldData[i]));
		}

		for (U32 i = 0; i < oldSize; i++) {
			oldData[i].~T();
		}

		free(oldData);

		m_data = newData;
		m_size = newSize;
		m_capacity = newCapacity;
	}

	void resize(U32 newSize) {
		U32 oldSize = m_size;
		if (m_capacity < newSize) {
			set_capacity(newSize);
		}

		for (U32 i = oldSize; i < newSize; i++) {
			new (m_data + i) T();
		}
		m_size = newSize;
	}

	void clear() {
		for (U32 i = 0; i < m_size; i++) {
			m_data[i].~T();
		}
		m_size = 0;
	}

	void destroy() {
		clear();
		if (m_data) {
			free(m_data);
			m_data = 0;
			m_capacity = 0;
		}
	}

	T& operator[](size_t idx) {
		return m_data[idx];
	}

	const T& operator[](size_t idx) const {
		return m_data[idx];
	}

	T* begin() {
		return m_data;
	}

	T* end() {
		return m_data + m_size;
	}

	const T* begin() const {
		return m_data;
	}

	const T* end() const {
		return m_data + m_size;
	}

	T* data() {
		return m_data;
	}

	const T* data() const {
		return m_data;
	}

	size_t size() const {
		return m_size;
	}

	size_t capacity() const {
		return m_size;
	}

	bool empty() const {
		return m_size == 0;
	}

	T& back() {
		assert(m_size > 0);
		return m_data[m_size - 1];
	}

	const T& back() const {
		assert(m_size > 0);
		return m_data[m_size - 1];
	}

private:
	void move_from(Vector&& other) {
		assert(!m_data);
		m_data     = other.m_data;
		m_capacity = other.m_capacity;
		m_size     = other.m_size;
		other.m_data     = nullptr;
		other.m_capacity = 0;
		other.m_size     = 0;
	}

	void copy_from(const Vector& other) {
		assert(!m_data);

		set_capacity(other.m_size);
		m_size = other.m_size;

		for (U32 i = 0; i < m_size; i++) {
			new (m_data + i) T(other.m_data[i]);
		}
	}
};

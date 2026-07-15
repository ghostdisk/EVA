#pragma once
#include <EVA/Core/Common.hpp>
#include <new>

template <typename T>
class Vector {
public:
	T*  m_data     = nullptr;
	U32 m_capacity = 0;
	U32 m_size     = 0;

	Vector() {
	}

	Vector(size_t size) {
		resize(size);
	}

	Vector(const Vector& other) {
		copy_from(other);
	}

	Vector(Vector&& other) {
		move_from(static_cast<Vector&&>(other));
	}

	void operator=(const Vector& other) {
		assert(this != &other);
		destroy();
		copy_from(other);
	}

	void operator=(Vector&& other) {
		assert(this != &other);
		destroy();
		move_from(static_cast<Vector&&>(other));
	}

	~Vector() {
		destroy();
	}

	void push_back(T&& value) {
		if (m_size == m_capacity) {
			set_capacity(m_capacity == 0 ? 8 : m_capacity * 2);
		}
		new (m_data + m_size) T(static_cast<T&&>(value));
		m_size++;
	}

	void push_back(const T& value) {
		if (m_size == m_capacity) {
			set_capacity(m_capacity == 0 ? 8 : m_capacity * 2);
		}
		new (m_data + m_size) T(value);
		m_size++;
	}

	T pop_back() {
		assert(m_size > 0);
		T value = std::move(m_data[m_size - 1]);
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
			new (newData + i) T(static_cast<T&&>(oldData[i]));
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

	size_t size() {
		return m_size;
	}

	size_t capacity() {
		return m_size;
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
#pragma once
#include <EVA/Core/Basic.hpp>

class Promise;
class Job;
class PromiseState;
class JobState;

void Ref(PromiseState* state);
void Unref(PromiseState* state);
void Ref(JobState* state);
void Unref(JobState* state);

enum class JobStatus {
	Unscheduled,
	Blocked,
	Scheduled,
	Executing,
	Done,
};

template <typename T>
class JobRefPtr {
public:
	T* m_data = nullptr;

	JobRefPtr() = default;
	explicit JobRefPtr(T* data) : m_data(data) {
		Ref(m_data);
	}
	JobRefPtr(const JobRefPtr& other) : m_data(other.m_data) {
		Ref(m_data);
	}
	JobRefPtr(JobRefPtr&& other) noexcept : m_data(other.m_data) {
		other.m_data = nullptr;
	}
	~JobRefPtr() {
		Reset();
	}

	JobRefPtr& operator=(const JobRefPtr& other) {
		if (this == &other) return *this;
		T* newData = other.m_data;
		Ref(newData);
		Reset();
		m_data = newData;
		return *this;
	}
	JobRefPtr& operator=(JobRefPtr&& other) noexcept {
		if (this == &other) return *this;
		Reset();
		m_data = other.m_data;
		other.m_data = nullptr;
		return *this;
	}

	void Reset() {
		T* data = m_data;
		m_data = nullptr;
		Unref(data);
	}

	T* operator->() const { return m_data; }
	T& operator*() const { return *m_data; }
	explicit operator bool() const { return m_data != nullptr; }
};

class Promise : public JobRefPtr<PromiseState> {
	using JobRefPtr::JobRefPtr;

public:
	void Block();
	void Signal();
	static Promise Create(int signalCount = 1);
	bool Done() const;
};

class Job : public JobRefPtr<JobState> {
	using JobRefPtr::JobRefPtr;

public:
	static Job Create(void (*proc)(void* userdata), void* userdata);
	void Wait(Promise* promise);
	void Schedule();
};

void JobInitialize();
void JobShutdown();

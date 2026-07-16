#include <EVA/Async/JobSystem.hpp>
#include <thread>
#include <semaphore>
#include <atomic>
#include <mutex>

class PromiseState {
public:
	explicit PromiseState(int signalCount) : m_signalCount(signalCount) {
	}

	std::mutex m_mutex;
	Vector<Job> m_blockedJobs;
	std::atomic<int> m_refCount = 0;
	int m_signalCount;
	bool m_done = false;
};

class JobState {
public:
	std::atomic<int> m_refCount = 0;
	std::atomic<int> m_blockCount = 0;
	JobStatus m_status = JobStatus::Unscheduled;
	void (*m_proc)(void* userdata) = nullptr;
	void* m_userdata = nullptr;
};

class Worker {
public:
	std::thread thread;
};

static Vector<Worker*> g_workers = {};
static Vector<JobState*> g_scheduledJobs = {}; // TODO: Use a MPMC queue and get rid of this and the mutex.
static std::mutex g_jobMutex = {};
static std::counting_semaphore<PTRDIFF_MAX> g_workSemaphore(0);
static std::atomic<bool> g_shutdown = false;

void Ref(PromiseState* state) {
	if (state) state->m_refCount.fetch_add(1, std::memory_order_relaxed);
}

void Unref(PromiseState* state) {
	if (state && state->m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
		delete state;
}

void Ref(JobState* state) {
	if (state) state->m_refCount.fetch_add(1, std::memory_order_relaxed);
}

void Unref(JobState* state) {
	if (state && state->m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
		delete state;
}

Job Job::Create(void (*proc)(void* userdata), void* userdata) {
	Job job(new JobState());
	job.m_data->m_proc = proc;
	job.m_data->m_userdata = userdata;
	return job;
}

Promise Promise::Create(int signalCount) {
	return Promise(new PromiseState(signalCount));
}

void Job::Wait(Promise* promise) {
	promise->m_data->m_mutex.lock();
	if (!promise->m_data->m_done) {
		m_data->m_blockCount++;
		promise->m_data->m_blockedJobs.push_back(*this);
	}
	promise->m_data->m_mutex.unlock();
}

void Job::Schedule() {
	assert(m_data->m_status == JobStatus::Unscheduled);

	g_jobMutex.lock();
	if (m_data->m_blockCount > 0) {
		m_data->m_status = JobStatus::Blocked;
		g_jobMutex.unlock();
	} else {
		m_data->m_status = JobStatus::Scheduled;
		Ref(m_data);
		g_scheduledJobs.push_back(m_data);
		g_jobMutex.unlock();
		g_workSemaphore.release();
	}
}

void Promise::Block() {
	m_data->m_mutex.lock();
	assert(!m_data->m_done);
	m_data->m_signalCount++;
	m_data->m_mutex.unlock();
}

void Promise::Signal() {
	m_data->m_mutex.lock();
	assert(!m_data->m_done);
	assert(m_data->m_signalCount > 0);
	m_data->m_signalCount--;
	if (m_data->m_signalCount > 0) {
		m_data->m_mutex.unlock();
		return;
	}
	m_data->m_done = true;

	int unblockCount = 0;
	g_jobMutex.lock();
	for (Job& job : m_data->m_blockedJobs) {
		if (--job.m_data->m_blockCount == 0) {
			job.m_data->m_status = JobStatus::Scheduled;
			Ref(job.m_data);
			g_scheduledJobs.push_back(job.m_data);
			unblockCount++;
		}
	}
	m_data->m_blockedJobs.clear();
	g_jobMutex.unlock();
	m_data->m_mutex.unlock();
	if (unblockCount > 0) g_workSemaphore.release(unblockCount);
}

bool Promise::Done() const {
	return m_data->m_done;
}

void WorkerThreadProc(Worker* worker) {
	for (;;) {
		g_workSemaphore.acquire();

		if (g_shutdown) break;

		g_jobMutex.lock();
		JobState* job = g_scheduledJobs[0];
		g_scheduledJobs.erase(g_scheduledJobs.begin());
		g_jobMutex.unlock();

		job->m_status = JobStatus::Executing;
		job->m_proc(job->m_userdata);
		job->m_status = JobStatus::Done;
		Unref(job);
	}
}

void JobInitialize() {
	int numWorkers = std::thread::hardware_concurrency();
	if (numWorkers < 1) numWorkers = 1;

	for (int i = 0; i < numWorkers; i++) {
		Worker* worker = new Worker();
		worker->thread = std::thread([worker]() { WorkerThreadProc(worker); });
		g_workers.push_back(worker);
	}
}

void JobShutdown() {
	g_shutdown = true;
	g_workSemaphore.release(g_workers.size());
	for (Worker* worker : g_workers) {
		worker->thread.join();
	}
}

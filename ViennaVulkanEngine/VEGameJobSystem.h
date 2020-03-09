#pragma once

/*
The Vienna Game Job System (VGJS)
Designed and implemented by Prof. Helmut Hlavacs, Faculty of Computer Science, University of Vienna
See documentation on how to use it at https://github.com/hlavacs/GameJobSystem
The library is a single include file, and can be used under MIT license.
*/

/**
*
* \file
* \brief
*
* Details
*
*/


#include <iostream>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <thread>
#include <future>
#include <vector>
#include <functional>
#include <condition_variable>
#include <queue>
#include <map>
#include <set>
#include <iterator>
#include <algorithm>
#include <assert.h>



#ifdef VE_ENABLE_MULTITHREADING
	#define JADD( f )	vgjs::JobSystem::getInstance()->addJob( [=](){ f; } );
	#define JDEP( f )	vgjs::JobSystem::getInstance()->onFinishedAddJob( [=](){ f; } );

	#define JADDT( f, t )	vgjs::JobSystem::getInstance()->addJob( [=](){ f; }, t );
	#define JDEPT( f, t )	vgjs::JobSystem::getInstance()->onFinishedAddJob( [=](){ f; }, t );

	#define JREP vgjs::JobSystem::getInstance()->onFinishedRepeatJob();

#else
	#define JADD( f ) {f;}
	#define JDEP( f ) {f;}

	#define JADDT( f, t ) {f;}
	#define JDEPT( f, t ) {f;}

	#define JREP assert(true);

#endif



namespace vgjs {

	class JobMemory;
	class Job;
	class JobSystem;
	class JobQueueFIFO;
	class JobQueueLockFree;

	using Function = std::function<void()>;

	typedef uint32_t VgjsThreadIndex;
	constexpr VgjsThreadIndex VGJS_NULL_THREAD_IDX = std::numeric_limits<VgjsThreadIndex>::max();

	typedef uint64_t VgjsThreadID;
	constexpr VgjsThreadID VGJS_NULL_THREAD_ID = std::numeric_limits<VgjsThreadID>::max();


	//-------------------------------------------------------------------------------
	class Job {
		friend JobMemory;
		friend JobSystem;
		friend JobQueueFIFO;
		friend JobQueueLockFree;

	private:
		Job *					m_nextInQueue;					//next in the current queue
		Job *					m_parentJob;					//parent job, called if this job finishes
		Job *					m_onFinishedJob;				//job to schedule once this job finshes
		VgjsThreadID			m_thread_id;					//the id of the thread this job must be scheduled, or -1 if any thread
		Function				m_function;						//the function to carry out
		std::atomic<uint32_t>	m_numUnfinishedChildren;		//number of unfinished jobs
		std::chrono::high_resolution_clock::time_point t1, t2;	//execution start and end
		VgjsThreadIndex			m_exec_thread;					//thread that this job actually ran at
		std::atomic<bool>		m_available;					//is this job available after a pool reset?
		bool					m_repeatJob;					//if true then the job will be rescheduled
	
		//---------------------------------------------------------------------------
		//set pointer to parent job
		void setParentJob(Job *parentJob ) {
			m_parentJob = parentJob;				//set the pointer
			if (parentJob == nullptr) return;
			parentJob->m_numUnfinishedChildren++;	//tell parent that there is one more child
		};	

		//---------------------------------------------------------------------------
		//set job to execute when this job has finished
		void setOnFinished(Job *pJob) { 	
			m_onFinishedJob = pJob; 
		};

		//---------------------------------------------------------------------------
		//set fixed thread id
		void setThreadId(VgjsThreadID thread_id) {
			m_thread_id = thread_id;
		}

		//---------------------------------------------------------------------------
		//set the Job's function
		void setFunction(Function& func) {
			m_function = func;
		};

		void setFunction(Function&& func) {
			m_function = std::move(func);
		};

		//---------------------------------------------------------------------------
		//notify parent, or schedule the finished job, define later since do not know JobSystem yet
		void onFinished();	

		//---------------------------------------------------------------------------
		void childFinished() {
			uint32_t numLeft = m_numUnfinishedChildren.fetch_sub(1);
			if (numLeft == 1) onFinished();					//this was the last child
		};

		//---------------------------------------------------------------------------
		//run the packaged task
		void operator()();				//does not know JobMemory yet, so define later

	public:

		Job() : m_nextInQueue(nullptr), m_parentJob(nullptr), m_thread_id(VGJS_NULL_THREAD_ID),
			t1(), t2(), m_exec_thread(VGJS_NULL_THREAD_IDX),
			m_numUnfinishedChildren(0), m_onFinishedJob(nullptr),
			m_repeatJob(false), m_available(true) {};
		~Job() {};
	};


	//---------------------------------------------------------------------------
	class JobMemory {
		friend JobSystem;
		friend Job;

		//transient jobs, will be deleted for each new frame
		const static std::uint32_t	m_listLength = 4096;		//length of a segment

		using JobList = std::vector<Job>;
		struct JobPool {
			std::atomic<uint32_t> jobIndex;		//index of next job to allocate, or number of jobs to playback
			std::vector<JobList*> jobLists;		//list of Job structures
			std::mutex lmutex;					//only lock if appending the job list

			JobPool() : jobIndex(0) {
				jobLists.reserve(10);
				jobLists.emplace_back(new JobList(m_listLength));
			};

			~JobPool() {
				for (uint32_t i = 0; i < jobLists.size(); i++) {
					delete jobLists[i];
				}
			};
		};

	private:
		JobPool m_jobPool;		//hols lists of segments, which are job containers
		~JobMemory() {};
	
	public:
		//---------------------------------------------------------------------------
		//get instance of singleton
		static JobMemory *pInstance;			//pointer to singleton
		static JobMemory *getInstance() {						
			if (pInstance == nullptr)
				pInstance = new JobMemory;		//create the singleton
			return pInstance;
		};

		//---------------------------------------------------------------------------
		//get a new empty job from the job memory - if necessary add another job list
		Job * getNextJob() {
			uint32_t index = m_jobPool.jobIndex.fetch_add(1);				//increase counter by 1
			if (index > m_jobPool.jobLists.size() * m_listLength - 1) {		//do we need a new segment?
				std::lock_guard<std::mutex> lock(m_jobPool.lmutex);			//lock the pool

				if (index > m_jobPool.jobLists.size() * m_listLength - 1)		//might be beaten here by other thread so check again
					m_jobPool.jobLists.emplace_back(new JobList(m_listLength));	//create a new segment
			}

			return &(*m_jobPool.jobLists[index / m_listLength])[index % m_listLength];	//get modulus of number of last job list
		};

		//---------------------------------------------------------------------------
		//get the first job that is available
		Job* allocateJob( ) {
			Job *pJob;
			do {
				pJob = getNextJob();						//get the next Job in the pool
			} while ( !pJob->m_available );					//check whether it is available
			pJob->m_nextInQueue = nullptr;

			pJob->m_onFinishedJob = nullptr;				//no successor Job yet
			pJob->m_parentJob = nullptr;					//default is no parent
			pJob->m_repeatJob = false;						//default is no repeat
			pJob->m_thread_id = VGJS_NULL_THREAD_ID;
			pJob->m_exec_thread = VGJS_NULL_THREAD_IDX;
			return pJob;
		};

		//---------------------------------------------------------------------------
		//reset index for new frame, start with 0 again
		void resetPool() { 
			m_jobPool.jobIndex.store(0);
		};

		JobPool* getPoolPointer() {
			return &m_jobPool;
		};
	};


	//---------------------------------------------------------------------------
	//queue class
	//will be changed for lock free queues
	class JobQueue {

	public:
		JobQueue() {};
		virtual void push(Job * pJob) = 0;
		virtual Job * pop() = 0;
		virtual Job *steal() = 0;
	};


	//---------------------------------------------------------------------------
	//queue class
	//will be changed for lock free queues
	class JobQueueFIFO : public JobQueue {
		std::queue<Job*> m_queue;	//conventional queue by now

	public:
		//---------------------------------------------------------------------------
		JobQueueFIFO() {};

		//---------------------------------------------------------------------------
		void push(Job * pJob) {
			m_queue.push(pJob);
		};

		//---------------------------------------------------------------------------
		Job * pop() {
			if (m_queue.size() == 0) {
				return nullptr;
			};
			Job* pJob = m_queue.front();
			m_queue.pop();
			return pJob;
		};

		//---------------------------------------------------------------------------
		Job *steal() {
			return pop();
		};
	};




	//---------------------------------------------------------------------------
	//queue class
	//lock free queue
	class JobQueueLockFree : public JobQueue {

		std::atomic<Job *> m_pHead = nullptr;

	public:
		//---------------------------------------------------------------------------
		JobQueueLockFree() {};

		//---------------------------------------------------------------------------
		void push(Job * pJob) {
			pJob->m_nextInQueue = m_pHead.load(std::memory_order_relaxed);

			while (! std::atomic_compare_exchange_strong_explicit(
				&m_pHead, &pJob->m_nextInQueue, pJob, 
				std::memory_order_release, std::memory_order_relaxed) ) {};
		};

		//---------------------------------------------------------------------------
		Job * pop() {
			Job * head = m_pHead.load(std::memory_order_relaxed);
			if (head == nullptr) return nullptr;
			while (head != nullptr && !std::atomic_compare_exchange_weak(&m_pHead, &head, head->m_nextInQueue)) {};
			return head;
		};

		//---------------------------------------------------------------------------
		Job *steal() {
			return pop();
		};
	};



	//---------------------------------------------------------------------------
	class JobSystem {
		friend Job;

	private:
		std::vector<std::thread>			m_threads;				//array of thread structures
		std::vector<Job*>					m_jobPointers;			//each thread has a current Job that it may run, pointers point to them
		std::unordered_map<std::thread::id, uint32_t> m_threadIndexMap;		//Each thread has an index number 0...Num Threads
		std::atomic<bool>					m_terminate;			//Flag for terminating the pool
		std::vector<JobQueue*>				m_jobQueues;			//Each thread has its own Job queue
		std::vector<JobQueue*>				m_jobQueuesLocal;		//a secondary low priority FIFO queue for each thread, where polling jobs are parked
		std::vector<JobQueue*>				m_jobQueuesLocalFIFO;	//a secondary low priority FIFO queue for each thread, where polling jobs are parked
		std::atomic<uint32_t>				m_numJobs;				//total number of jobs in the system
		std::vector<uint64_t>				m_numLoops;				//number of loops the task has done so far
		std::vector<uint64_t>				m_numMisses;			//number of times the task did not get a job
		std::mutex							m_mainThreadMutex;		//used for syncing with main thread
		std::condition_variable				m_mainThreadCondVar;	//used for waking up main tread

		//---------------------------------------------------------------------------
		// function each thread performs
		void threadTask( uint32_t threadIndex = 0 ) {
			m_threadIndexMap[std::this_thread::get_id()] = threadIndex;	//map thread id to thread index

			static std::atomic<uint32_t> threadIndexCounter = 0;
			threadIndexCounter++;
			while(threadIndexCounter < m_threads.size() )
				std::this_thread::sleep_for(std::chrono::nanoseconds(10));

			while (!m_terminate) {
				m_numLoops[threadIndex]++;

				if (m_terminate) break;

				Job * pJob = m_jobQueuesLocal[threadIndex]->pop();

				if (m_terminate) break;

				if( pJob == nullptr ) 
					pJob = m_jobQueuesLocalFIFO[threadIndex]->pop();

				if (m_terminate) break;

				if (pJob == nullptr)
					pJob = m_jobQueues[threadIndex]->pop();

				if (m_terminate) break;

				uint32_t tsize = (uint32_t)m_threads.size();
				if (pJob == nullptr && tsize > 1) {
					uint32_t idx = std::rand() % tsize;
					uint32_t max = tsize;

					while (pJob == nullptr) {
						if (idx != threadIndex)
							pJob = m_jobQueues[idx]->steal();
						idx = (idx+1) % tsize;
						max--;
						if (max == 0) break;
						if (m_terminate) break;
					}
				}

				if (m_terminate) break;

				if (pJob != nullptr) {
					m_jobPointers[threadIndex] = pJob;						//make pointer to the Job structure accessible!
					pJob->t1 = std::chrono::high_resolution_clock::now();	//time of execution
					(*pJob)();												//run the job
					pJob->t2 = std::chrono::high_resolution_clock::now();	//time of finishing
					pJob->m_exec_thread = threadIndex;						//thread idx this job was executed on
				}
				else {
					m_numMisses[threadIndex]++;
					//std::this_thread::sleep_for(std::chrono::nanoseconds(1));
					//std::this_thread::yield();
				};
			}
			m_numJobs = 0;
			m_mainThreadCondVar.notify_all();		//make sure to wake up a waiting main thread
		};

	public:

		//---------------------------------------------------------------------------
		//class constructor
		//threadCount Number of threads to start. If 0 then the number of hardware threads is used.
		//start_idx is either 0, then the main thread is not part of the pool, or 1, then the main thread must join
		//
		static JobSystem *	pInstance;			//pointer to singleton
		JobSystem(std::size_t threadCount = 0, uint32_t start_idx = 0 ) : m_terminate(false), m_numJobs(0) {
			pInstance = this;
			JobMemory::getInstance();			//create the job memory

			if (threadCount == 0) {
				threadCount = std::thread::hardware_concurrency();		//main thread is also running
			}

			m_jobQueues.resize(threadCount);							//reserve mem for job queue pointers
			m_jobQueuesLocal.resize(threadCount);						//reserve mem for polling job queue pointers
			m_jobQueuesLocalFIFO.resize(threadCount);					//reserve mem for polling job queue pointers
			m_jobPointers.resize(threadCount);							//rerve mem for Job pointers
			m_numMisses.resize(threadCount);
			m_numLoops.resize(threadCount);
			for (uint32_t i = 0; i < threadCount; i++) {
				m_jobQueues[i]			= new JobQueueLockFree();			//job queue with work stealing
				m_jobQueuesLocal[i]		= new JobQueueLockFree();			//job queue for local work
				m_jobQueuesLocalFIFO[i] = new JobQueueFIFO();				//job queue for local polling work
				m_jobPointers[i]		= nullptr;							//pointer to current Job structure
				m_numLoops[i]			= 0;								//for accounting per thread statistics
				m_numMisses[i]			= 0;
			}

			m_threads.reserve(threadCount);										//reserve mem for the threads
			for (uint32_t i = start_idx; i < threadCount; i++) {
				m_threads.push_back(std::thread( &JobSystem::threadTask, this, i ));	//spawn the pool threads
			}

			JobMemory::pInstance->resetPool();	//pre-allocate job pools
		};

		//---------------------------------------------------------------------------
		//singleton access through class
		//returns a pointer to the JobSystem instance
		//
		static JobSystem * getInstance(std::size_t threadCount = 0, uint32_t start_idx = 0) {
			if (pInstance == nullptr) pInstance = new JobSystem(threadCount, start_idx);
			return pInstance;
		};

		JobSystem(const JobSystem&) = delete;				// non-copyable,
		JobSystem& operator=(const JobSystem&) = delete;
		JobSystem(JobSystem&&) = default;					// but movable
		JobSystem& operator=(JobSystem&&) = default;
		~JobSystem() {
			m_threads.clear();
		};

		//---------------------------------------------------------------------------
		//sets a flag to terminate all running threads
		//
		void terminate() {
			m_terminate = true;					//let threads terminate
		};

		//---------------------------------------------------------------------------
		//returns total number of jobs in the system
		//
		uint32_t getNumberJobs() {
			return m_numJobs;
		};

		//---------------------------------------------------------------------------
		//can be called by the main thread to wait for the completion of all jobs in the system
		//returns as soon as there are no more jobs in the job queues
		//
		void wait() {
			std::chrono::seconds sec(1);

			while (getNumberJobs() > 0) {
				std::unique_lock<std::mutex> lock(m_mainThreadMutex);
				m_mainThreadCondVar.wait_for(lock, std::chrono::microseconds(500));		//wait to be awakened
			}
		};

		//---------------------------------------------------------------------------
		//can be called by the main thread to wait for all threads to terminate
		//returns as soon as all threads have exited
		//
		void waitForTermination() {
			for (uint32_t i = 0; i < m_threads.size(); i++ ) {
				m_threads[i].join();
			}
		};

		//---------------------------------------------------------------------------
		// returns number of threads in the thread pool
		//
		std::size_t getThreadCount() const { return m_threads.size(); };

		//---------------------------------------------------------------------------
		//each thread has a unique index between 0 and numThreads - 1
		//returns index of the thread that is calling this function
		//can e.g. be used for allocating command buffers from command buffer pools in Vulkan
		//
		int32_t getThreadIndex() {
			if (m_threadIndexMap.count(std::this_thread::get_id()) == 0) return -1;

			return m_threadIndexMap[std::this_thread::get_id()];
		};

		static VgjsThreadIndex getThreadIndexFromID( VgjsThreadID thread_id ) {
			return (VgjsThreadIndex)(thread_id & VGJS_NULL_THREAD_IDX);
		}

		static VgjsThreadIndex getThreadLabelFromID(VgjsThreadID thread_id) {
			return (VgjsThreadIndex)(thread_id >> 32);
		}

		static VgjsThreadID createThreadID(VgjsThreadIndex thread_idx, VgjsThreadIndex label ) {
			return (VgjsThreadID)label << 32 | (VgjsThreadID)thread_idx;
		}

		//---------------------------------------------------------------------------
		//wrapper for resetting a job pool in the job memory
		//poolNumber Number of the pool to reset
		//
		void resetPool() {
			JobMemory::pInstance->resetPool();
		};
		
		//---------------------------------------------------------------------------
		//returns a pointer to the job of the current task
		//
		Job *getJobPointer() {
			int32_t threadNumber = getThreadIndex();
			if (threadNumber < 0) return nullptr;
			return m_jobPointers[threadNumber];
		};

		//---------------------------------------------------------------------------
		//add a job to a queue
		//pJob Pointer to the job to schedule
		//queue Selects the queue, the lock free queue is LIFO, the polling queue is FIFO
		//
		void addJob( Job *pJob ) {
			m_numJobs++;	//keep track of the number of jobs in the system to sync with main thread

			uint32_t tsize = (int32_t)m_threads.size();
			VgjsThreadIndex thread_idx = getThreadIndexFromID( pJob->m_thread_id );
			if (pJob->m_thread_id != VGJS_NULL_THREAD_ID && thread_idx != VGJS_NULL_THREAD_IDX ) {
				assert(thread_idx < tsize);

				uint32_t threadNumber = getThreadIndex();
				if(thread_idx == threadNumber )
					m_jobQueuesLocalFIFO[thread_idx]->push(pJob);	//put into thread local FIFO queue
				else
					m_jobQueuesLocal[thread_idx]->push(pJob);		//put into thread local LIFO queue
				return;
			}

			m_jobQueues[std::rand() % tsize]->push(pJob);					//put into random LIFO queue
		};

		//---------------------------------------------------------------------------
		//create a new child job in a job pool
		//func The function to schedule, as rvalue reference
		void addJob( Function& func, VgjsThreadID thread_id = VGJS_NULL_THREAD_ID ) {
			Job *pJob = JobMemory::pInstance->allocateJob();
			pJob->setParentJob(getJobPointer());	//set parent Job to notify on finished, or nullptr if main thread
			pJob->setFunction(func);
			pJob->setThreadId(thread_id);
			addJob(pJob);
		};

		//func The function to schedule, as rvalue reference
		//poolNumber Number of the pool
		//id A name for the job for debugging
		void addJob( Function&& func, VgjsThreadID thread_id = VGJS_NULL_THREAD_ID ) {
			Job* pJob = JobMemory::pInstance->allocateJob();
			pJob->setParentJob(getJobPointer());	//set parent Job to notify on finished, or nullptr if main thread
			pJob->setFunction(std::forward<std::function<void()>>(func));
			pJob->setThreadId(thread_id);
			addJob(pJob);
		};

		//---------------------------------------------------------------------------
		//create a successor job for this job, will be added to the queue after 
		//the current job finished (i.e. all children have finished)
		//func The function to schedule
		//id A name for the job for debugging
		//
		void onFinishedAddJob(Function func, VgjsThreadID thread_id = VGJS_NULL_THREAD_ID ) {
			Job *pCurrentJob = getJobPointer();			//should never be called by meain thread
			if (pCurrentJob == nullptr) return;			//is null if called by main thread
			if (pCurrentJob->m_repeatJob) return;		//you cannot do both repeat and add job after finishing
			Job *pNewJob = JobMemory::pInstance->allocateJob();			//new job has the same parent as current job
			pNewJob->setFunction(func);
			pNewJob->setThreadId(thread_id);
			pCurrentJob->setOnFinished(pNewJob);
		};

		//---------------------------------------------------------------------------
		//Once the Job finished, it will be rescheduled  and repeated
		//This also includes all children, which will be recreated and run
		//
		void onFinishedRepeatJob() {
			Job *pCurrentJob = getJobPointer();						//can be nullptr if called from main thread
			if (pCurrentJob == nullptr) return;						//is null if called by main thread
			if (pCurrentJob->m_onFinishedJob != nullptr) return;	//you cannot do both repeat and add job after finishing	
			pCurrentJob->m_repeatJob = true;						//flag that in onFinished() the job will be rescheduled
		}

		//---------------------------------------------------------------------------
		//wait for all children to finish and then terminate the pool
		void onFinishedTerminatePool() {
			onFinishedAddJob( std::bind(&JobSystem::terminate, this) );
		};

		//---------------------------------------------------------------------------
		//Print deubg information, this is synchronized so that text is not confused on the console
		//s The string to print to the console
		void printDebug(std::string s) {
			static std::mutex lmutex;
			std::lock_guard<std::mutex> lock(lmutex);
			std::cout << s;
		};
	};

	VgjsThreadID TID(VgjsThreadIndex label, VgjsThreadIndex index);

}



#ifdef IMPLEMENT_GAMEJOBSYSTEM

namespace vgjs {

	JobMemory* JobMemory::pInstance = nullptr;
	JobSystem* JobSystem::pInstance = nullptr;


	VgjsThreadID TID(VgjsThreadIndex index, VgjsThreadIndex label) {
		return JobSystem::createThreadID(index, label);
	}

	//---------------------------------------------------------------------------
	//This is run if the job is executed
	void Job::operator()() {
		if (m_function == nullptr) return;				//no function bound -> return
		m_numUnfinishedChildren = 1;					//number of children includes itself
		m_function();									//call the function
		uint32_t numLeft = m_numUnfinishedChildren.fetch_sub(1);	//reduce number of running children by 1 (includes itself)
		if (numLeft == 1) onFinished();								//this was the last child
	};


	//---------------------------------------------------------------------------
	//This call back is called once a Job and all its children are finished
	void Job::onFinished() {
		if (m_repeatJob) {							//job is repeated for polling
			m_repeatJob = false;					//only repeat if job executes and so
			JobSystem::pInstance->m_numJobs--;		//addJob() will increase this again
			JobSystem::pInstance->addJob( this);	//rescheduled this to the polling FIFO queue
			return;
		}

		if (m_onFinishedJob != nullptr) {						//is there a successor Job?
			if (m_parentJob != nullptr) 
				m_onFinishedJob->setParentJob(  m_parentJob );
			JobSystem::pInstance->addJob(m_onFinishedJob);	//schedule it for running
		}

		if (m_parentJob != nullptr) {		//if there is parent then inform it	
			m_parentJob->childFinished();	//if this is the last child job then the parent will also finish
		}

		//synchronize with the main thread
		uint32_t numLeft = JobSystem::pInstance->m_numJobs.fetch_sub(1); //one less job in the system
		if (numLeft == 1) {							//if this was the last job in ths system 
			JobSystem::pInstance->m_mainThreadCondVar.notify_all();	//notify main thread that might be waiting
		}
		m_available = true;					//job is available again (after a pool reset)
	};


}

#else



#endif


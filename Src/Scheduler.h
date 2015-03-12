#pragma once

#include "Platform.h"
#include "ConcurrentQueue.h"

#ifdef Yield
#undef Yield
#endif

namespace MT
{

	const uint32 MT_MAX_THREAD_COUNT = 4;
	const uint32 MT_MAX_FIBERS_COUNT = 128;
	const uint32 MT_SCHEDULER_STACK_SIZE = 16384;
	const uint32 MT_FIBER_STACK_SIZE = 16384;

	//
	// Task group
	//
	// Application can wait until whole group was finished.
	namespace TaskGroup
	{
		enum Type
		{
			GROUP_0 = 0,
			GROUP_1 = 1,
			GROUP_2 = 2,

			COUNT,

			GROUP_UNDEFINED
		};
	}


	struct TaskDesc;
	struct ThreadContext;
	class TaskScheduler;

	typedef void (MT_CALL_CONV *TTaskEntryPoint)(MT::ThreadContext & context, void* userData);

	//
	// Fiber task status
	//
	// Task can be completed for several reasons.
	// For example task was done or someone call Yield from the Task body.
	namespace FiberTaskStatus
	{
		enum Type
		{
			UNKNOWN = 0,
			RUNNED = 1,
			FINISHED = 2,
		};
	}

	//
	// Fiber context
	//
	// Context passed to fiber main function
	struct FiberContext
	{
		// pointer to active task attached to this fiber
		TaskDesc * activeTask;

		// active thread context
		ThreadContext * activeContext;

		// active task status
		FiberTaskStatus::Type taskStatus;

		// number of child tasks spawned
		MT::InterlockedInt childTasksCount;

		FiberContext()
			: activeTask(nullptr)
			, activeContext(nullptr)
			, taskStatus(FiberTaskStatus::UNKNOWN)
		{
			childTasksCount.Set(0);
		}
	};

	//
	// Fiber execution context
	//
	// Hold pointer to fiber and pointer to fiber context
	//
	struct FiberExecutionContext
	{
		MT::Fiber fiber;
		MT::FiberContext * fiberContext;

		FiberExecutionContext(MT::Fiber _fiber, MT::FiberContext * _fiberContext)
			: fiber(_fiber)
			, fiberContext(_fiberContext)
		{}

		bool IsValid() const
		{
			return (fiber != nullptr && fiberContext != nullptr);
		}

		static FiberExecutionContext Empty()
		{
			return FiberExecutionContext(nullptr, nullptr);
		}
	};

	//
	// Task description
	//
	struct TaskDesc
	{
		//Execution context. Not valid until scheduler attach fiber to task
		FiberExecutionContext activeFiber;

		TaskGroup::Type taskGroup;

		//Task entry point
		TTaskEntryPoint taskFunc;

		//Task user data (task context)
		void* userData;

		TaskDesc()
			: taskFunc(nullptr)
			, userData(nullptr)
			, taskGroup(TaskGroup::GROUP_UNDEFINED)
			, activeFiber(FiberExecutionContext::Empty())
		{

		}

		TaskDesc(TTaskEntryPoint _taskEntry, void* _userData)
			: taskFunc(_taskEntry)
			, userData(_userData)
			, taskGroup(TaskGroup::GROUP_UNDEFINED)
			, activeFiber(FiberExecutionContext::Empty())
		{
		}
	};


	//
	// Thread (Scheduler fiber) context
	//
	struct ThreadContext
	{
		// pointer to task manager
		MT::TaskScheduler* taskScheduler;

		// thread
		MT::Thread thread;

		// scheduler fiber
		MT::Fiber schedulerFiber;

		// task queue awaiting execution
		MT::ConcurrentQueue<MT::TaskDesc> queue;

		// new task was arrived to queue event
		MT::Event hasNewTasksEvent;

		ThreadContext();

		void Yield();
	};


	//
	// Task scheduler
	//
	class TaskScheduler
	{

		// Thread index for new task
		uint32 roundRobinThreadIndex;

		// Threads created by task manager
		int32 threadsCount;
		ThreadContext threadContext[MT_MAX_THREAD_COUNT];

		// Per group events that is completed
		MT::Event groupIsDoneEvents[TaskGroup::COUNT];

		MT::InterlockedInt groupCurrentlyRunningTaskCount[TaskGroup::COUNT];

		// Fibers pool
		MT::ConcurrentQueue<FiberExecutionContext> availableFibers;

		// Fibers context
		MT::FiberContext fiberContext[MT_MAX_FIBERS_COUNT];

		FiberExecutionContext RequestFiber();
		void ReleaseFiber(FiberExecutionContext fiberExecutionContext);


	public:

		TaskScheduler();
		~TaskScheduler();


		template<typename T, int size>
		void RunTasks(TaskGroup::Type taskGroup, const T(&taskDesc)[size])
		{
			for (int i = 0; i < size; i++)
			{
				ThreadContext & context = threadContext[roundRobinThreadIndex];
				roundRobinThreadIndex = (roundRobinThreadIndex + 1) % (uint32)threadsCount;

				//TODO: can be write more effective implementation here, just split to threads before submitting tasks to queue
				MT::TaskDesc desc = taskDesc[i];
				desc.taskGroup = taskGroup;

				context.queue.Push(desc);
				
				groupIsDoneEvents[taskGroup].Reset();
				groupCurrentlyRunningTaskCount[taskGroup].Inc();

				context.hasNewTasksEvent.Signal();
			}
		}

		bool WaitGroup(MT::TaskGroup::Type group, uint32 milliseconds);
		bool WaitAll(uint32 milliseconds);

		static uint32 MT_CALL_CONV ThreadMain( void* userData );
		static void MT_CALL_CONV FiberMain(void* userData);

		static void ExecuteTask (MT::ThreadContext& context, MT::TaskDesc & taskDesc);

	};
}
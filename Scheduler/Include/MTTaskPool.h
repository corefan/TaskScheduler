// The MIT License (MIT)
// 
// 	Copyright (c) 2015 Sergey Makeev, Vadim Slyusarev
// 
// 	Permission is hereby granted, free of charge, to any person obtaining a copy
// 	of this software and associated documentation files (the "Software"), to deal
// 	in the Software without restriction, including without limitation the rights
// 	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// 	copies of the Software, and to permit persons to whom the Software is
// 	furnished to do so, subject to the following conditions:
// 
//  The above copyright notice and this permission notice shall be included in
// 	all copies or substantial portions of the Software.
// 
// 	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// 	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// 	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// 	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// 	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// 	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// 	THE SOFTWARE.

#pragma once



namespace MT
{

	namespace TaskID
	{
		//unused_id is any odd number, valid_id should be always only even numbers
		static const int UNUSED = 1;
	}


	//forward declaration
	class TaskHandle;


	/// \class TaskPoolBase
	/// \brief 
	//////////////////////////////////////////////////////////////////////////
	struct PoolElementHeader
	{
		AtomicInt id;

	protected:

		virtual void Destroy()
		{
			id.Set(TaskID::UNUSED);
		}

	public:

		PoolElementHeader(int _id)
			: id(_id)
		{
		}

		static bool DestoryByHandle(const MT::TaskHandle & handle);
	};


	/// \class TaskPoolElement
	/// \brief 
	//////////////////////////////////////////////////////////////////////////
	template<typename T>
	class PoolElement : public PoolElementHeader
	{
	protected:

		virtual void Destroy()
		{
			PoolElementHeader::Destroy();
			//call dtor
			task.~T();
		}

	public:

		T task;

		PoolElement(int _id, T && _task)
			: PoolElementHeader(_id)
			, task( std::move(_task) )
		{
		}

	};


	/// \class TaskHandle
	/// \brief 
	//////////////////////////////////////////////////////////////////////////
	class TaskHandle
	{

		int check_id;

	protected:

		friend struct PoolElementHeader;
		PoolElementHeader * task;

	public:

		//default ctor
		TaskHandle()
			: check_id(TaskID::UNUSED)
			, task(nullptr)
		{

		}

		//ctor
		TaskHandle(int _id, PoolElementHeader* _task)
			: check_id(_id)
			, task(_task)
		{
		}

		//copy ctor
		TaskHandle(const TaskHandle & other)
			: check_id(other.check_id)
			, task(other.task)
		{
		}

		//move ctor
		TaskHandle(TaskHandle && other)
			: check_id(other.check_id)
			, task(other.task)
		{
			other.check_id = TaskID::UNUSED;
			other.task = nullptr;
		}

		~TaskHandle()
		{
		}

		bool IsValid() const
		{
			if (task == nullptr)
			{
				return false;
			}

			if (check_id != task->id.Get())
			{
				return false;
			}

			return true;
		}

	};




	//////////////////////////////////////////////////////////////////////////
	inline bool PoolElementHeader::DestoryByHandle(const MT::TaskHandle & handle)
	{
		if (!handle.IsValid())
		{
			return false;
		}

		handle.task->Destroy();
		return true;
	}

	



	/// \class TaskPool
	/// \brief 
	//////////////////////////////////////////////////////////////////////////
	template<typename T, size_t N>
	class TaskPool
	{
		typedef PoolElement<T> PoolItem;

		//
		static const size_t MASK = (N - 1);

		void* data;
		AtomicInt idGenerator;
		AtomicInt index;

		inline PoolItem* Buffer()
		{
			return (PoolItem*)(data);
		}

		inline void MoveCtor(PoolItem* element, int id, T && val)
		{
			new(element) PoolItem(id, std::move(val));
		}

	private:

		TaskPool(const TaskPool &) {}
		void operator=(const TaskPool &) {}

	public:

		TaskPool()
			: idGenerator(0)
			, index(0)
		{
			static_assert( MT::StaticIsPow2<N>::result, "Task pool capacity must be power of 2");

			size_t bytesCount = sizeof(PoolItem) * N;
			data = Memory::Alloc(bytesCount);

			for(size_t idx = 0; idx < N; idx++)
			{
				PoolItem* pElement = Buffer() + idx;
				pElement->id.Set(TaskID::UNUSED);
			}
		}

		~TaskPool()
		{
			if (data != nullptr)
			{

				for(size_t idx = 0; idx < N; idx++)
				{
					PoolItem* pElement = Buffer() + idx;

					int preValue = pElement->id.Set(TaskID::UNUSED);
					if (preValue != TaskID::UNUSED)
					{
						pElement->task.~T();
					}
				}

				Memory::Free(data);
				data = nullptr;
			}
		}


		TaskHandle Alloc(T && task)
		{
			int idx = index.Inc() - 1;

			int clampedIdx = (idx & MASK);

			PoolItem* pElement = Buffer() + clampedIdx;

			bool isUnused = ((pElement->id.Get() & 1 ) != 0);
			if (isUnused == false)
			{
				//Can't allocate more, next element in circular buffer is already used
				return TaskHandle();
			}

			//generate next even number for id
			int id = idGenerator.Add(2);
			MoveCtor( pElement, id, std::move(task) );
			return TaskHandle(id, pElement);
		}

	};

}
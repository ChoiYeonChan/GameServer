#pragma once
#include <Windows.h>
#include <malloc.h>
#include <iostream>

template<typename DATA>
class MemoryPool
{
private:
	struct BlockNode
	{
		BlockNode* next;

		BlockNode() : next(nullptr) { }
	};

	struct StampNode
	{
		BlockNode* ptr;
		__int64 stamp;

		StampNode() : ptr(nullptr), stamp(0) { }
		StampNode(BlockNode* _ptr, __int64 _stamp) : ptr(_ptr), stamp(_stamp) { }
	};

	StampNode* top;

	long countAlloc;	// 할당한 블록의 수
	long countBlock;	// 전체 블록 수

public:
	MemoryPool() : countAlloc(0), countBlock(0)
	{
		top = (StampNode*)_aligned_malloc(sizeof(StampNode), 16);
		top = new (top)StampNode;
	}

	virtual ~MemoryPool()
	{
		BlockNode* node;

		for (int i = 0; i < countBlock; i++)
		{
			node = top->ptr;
			top->ptr = top->ptr->next;
			free(node);
		}

		_aligned_free(top);
	}

	DATA* Alloc(bool placementNew = true)
	{
		BlockNode* node = nullptr;
		StampNode oldTop;

		DATA* result;

		long count = countBlock;
		InterlockedIncrement((long*)&countAlloc);

		// 가용 공간이 없는 경우
		if (count < countAlloc)
		{
			// 메모리를 할당하고 전체 블록 수를 늘린다.
			node = (BlockNode*)malloc(sizeof(BlockNode) + sizeof(DATA));
			InterlockedIncrement((long*)&countBlock);
		}
		else
		{
			while (true)
			{
				oldTop.stamp = top->stamp;
				oldTop.ptr = top->ptr;
				node = oldTop.ptr;
				if (InterlockedCompareExchange128(
					(LONG64*)top,
					top->stamp + 1,
					(LONG64)top->ptr->next,
					(LONG64*)&oldTop
				))
				{
					break;
				}
			}
		}

		result = (DATA*)(node + 1);

		if (placementNew)
		{
			new (result)DATA;
		}

		return result;
	}

	void Free(DATA* data)
	{
		BlockNode* node = ((BlockNode*)data) - 1;
		StampNode oldTop;

		while (true)
		{
			oldTop.stamp = top->stamp;
			oldTop.ptr = top->ptr;

			node->next = top->ptr;
			if (InterlockedCompareExchange128(
				(LONG64*)top,
				top->stamp + 1,
				(LONG64)node,
				(LONG64*)&oldTop
			))
			{
				break;
			}
		}

		InterlockedDecrement((long*)&countAlloc);
	}

	long GetAllocCount() { return countAlloc; }
	long GetBlockCount() { return countBlock; }
};
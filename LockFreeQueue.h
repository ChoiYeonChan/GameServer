#pragma once
#include <Windows.h>
#include <cstdio>
#include <iostream>
#include "MemoryPool.h"

using namespace std;

template <typename DATA>
class LockFreeQueue
{
private:
	struct Node
	{
		DATA value;
		Node* next;

		Node() : next(nullptr) { }
		Node(DATA _value) : value(_value), next(nullptr) { }
	};

	struct StampNode
	{
		Node* ptr;
		__int64 stamp;

		StampNode() : ptr(nullptr), stamp(0) { }
		StampNode(Node* _ptr, __int64 _stamp) : ptr(_ptr), stamp(_stamp) { }
	};

	__declspec(align(16)) StampNode head;
	__declspec(align(16)) StampNode tail;

	unsigned int m_size;

	MemoryPool<Node> m_pool;

public:
	LockFreeQueue() : m_size(0)
	{
		// ���ʳ�� ����
		head.ptr = tail.ptr = m_pool.Alloc();
	}

	~LockFreeQueue()
	{
		Clear();
		m_pool.Free(head.ptr);
	}

	void Clear()
	{
		Node* ptr = nullptr;
		volatile unsigned long long cnt = 0;
		while (head.ptr->next != nullptr)
		{
			InterlockedIncrement(&cnt);
			ptr = head.ptr->next;
			head.ptr->next = head.ptr->next->next;
			m_pool.Free(ptr);
		}

		printf("\nClear cnt = %d\n ", cnt);
		head.stamp = 0;
		tail = head;
		m_size = 0;
	}

	bool Enqueue(DATA value)
	{
		Node* node = m_pool.Alloc();
		if (node == nullptr)
		{
			std::cout << "�ɷȽ��ϴ�" << std::endl;
			return false;
		}
		node->value = value;
		node->next = nullptr;

		StampNode last;
		Node* next;

		while (true)
		{
			last = tail;
			next = last.ptr->next;

			if (last.ptr == tail.ptr)	// last.ptr != tail.ptr�̸� �ٸ� �����忡 ���� Enq�� ����Ǿ���.
			{
				if (next == nullptr)	// next != nullptr�̸� �ٸ� �����忡 ���� Enq�� �������̴�.
				{
					// last.ptr->next = node;
					// CAS�� �������� ������ �ٸ� �����忡 ���� next�� ����δ�.
					// �ٸ� �����忡 ���� last.ptr->next�� ��尡 ���� ���� ����� nullptr�� �ƴϹǷ� �����Ѵ�.
					if (InterlockedCompareExchangePointer((volatile PVOID*)&last.ptr->next, node, next) == nullptr)
					{
						// tail = node;
						// CAS�� �������� ������ �ٸ� �����忡 ���� ������ tail�� ��ġ�� ���ƿ´�.
						// ���������� �ٸ� �����忡 ���� tail�� �Ű��� ���̹Ƿ� true�� �����Ѵ�.
						if (InterlockedCompareExchange128(
							(LONG64*)&tail,
							tail.stamp + 1, (LONG64)node,
							(LONG64*)&last))
						{
							// InterlockedIncrement64((long long*)&m_size);
						}
						return true;
					}
				}
				// next != nullptr �̶�� ���� �ٸ� �����尡 enq�� �õ��ϴ� ��
				// tail.ptr->next = node; ���Ը� �����ϰ� tail�� �ű��� ���� ���¸� �ǹ��ϹǷ�
				// tail�� �ű�� ���� �����ش�.
				else
				{
					InterlockedCompareExchange128(
						(LONG64*)&tail,
						tail.stamp + 1, (LONG64)next,	// ���� ���� ���� node�� �ƴ϶� next�ӿ� ��������.
						(LONG64*)&last);
				}
			}
		}
	}

	bool Dequeue(DATA* data)
	{
		StampNode first;
		StampNode last;
		Node* next;

		DATA ret;

		while (true)
		{
			first = head;
			last = tail;
			next = first.ptr->next;

			if (first.ptr == head.ptr)
			{
				if (first.ptr == last.ptr)
				{
					// first.ptr == head.ptr && next == nullptr�̸� ť�� ����ִ� ����̴�.
					if (next == nullptr)
					{
						return false;
					}
					// first.ptr == last.ptr && next != nullptr�̸� �ٸ� �����尡 enq�� �õ��ϴ� ��
					// tail.ptr->next = next; ���Ը� �����ϰ� tail�� �ű��� ���� ���¸� �ǹ��ϹǷ�
					// tail�� �ű�� ���� �����ش�.
					else
					{
						InterlockedCompareExchange128(
							(LONG64*)&tail,
							tail.stamp + 1, (LONG64)next,
							(LONG64*)&last
						);
					}
				}
				else
				{
					// �� ��츦 ��� ó���ߴ��� �ش� ��ġ���� ����ǰ�
					// �ٸ� �����尡 ť�� ���� �׼��� ������ �߻��Ѵ�.
					if (next != nullptr)	// Ȯ�� ���
					{
						ret = next->value;
						if (InterlockedCompareExchange128(
							(LONG64*)&head,
							head.stamp + 1, (LONG64)next,
							(LONG64*)&first))
						{
							m_pool.Free(first.ptr);
							first.ptr = nullptr;
							*data = ret;
							return true;
						}
					}
				}
			}
		}
	}

	void Display(int count)
	{
		printf("\n==== Display ==== \n");

		Node* p = head.ptr->next;
		while (count > 0 && p != nullptr)
		{
			printf("%d, ", p->value);
			p = p->next;
			--count;
		}
		printf("\n");
	}
};


/*
#pragma once
#include <Windows.h>
#include <cstdio>
#include <iostream>
#include "MemoryPool.h"

using namespace std;

class LockFreeQueue
{
private:
	struct Node
	{
		int value;
		Node* next;

		Node() : value(0), next(nullptr) { }
		Node(int _value) : value(_value), next(nullptr) { }
	};

	struct StampNode
	{
		Node* ptr;
		__int64 stamp;

		StampNode() : ptr(nullptr), stamp(0) { }
		StampNode(Node* _ptr, __int64 _stamp) : ptr(_ptr), stamp(_stamp) { }
	};

	__declspec(align(16)) StampNode head;
	__declspec(align(16)) StampNode tail;

	unsigned int m_size;

	MemoryPool<Node> m_pool;

public:
	LockFreeQueue() : m_size(0)
	{
		// ���ʳ�� ����
		head.ptr = tail.ptr = m_pool.Alloc();
	}

	~LockFreeQueue()
	{
		Clear();
		m_pool.Free(head.ptr);
	}

	void Clear()
	{
		Node* ptr = nullptr;
		volatile unsigned long long cnt = 0;
		while (head.ptr->next != nullptr)
		{
			InterlockedIncrement(&cnt);
			ptr = head.ptr->next;
			head.ptr->next = head.ptr->next->next;
			m_pool.Free(ptr);
		}

		printf("\nClear cnt = %lld\n ", cnt);
		head.stamp = 0;
		tail = head;
		m_size = 0;
	}

	bool Enqueue(int value)
	{
		Node* node = m_pool.Alloc();
		if (node == nullptr)
		{
			std::cout << "�ɷȽ��ϴ�" << std::endl;
			return false;
		}
		node->value = value;
		node->next = nullptr;

		StampNode last;
		Node* next;

		while (true)
		{
			last = tail;
			next = last.ptr->next;

			if (last.ptr == tail.ptr)	// last.ptr != tail.ptr�̸� �ٸ� �����忡 ���� Enq�� ����Ǿ���.
			{
				if (next == nullptr)	// next != nullptr�̸� �ٸ� �����忡 ���� Enq�� �������̴�.
				{
					// last.ptr->next = node;
					// CAS�� �������� ������ �ٸ� �����忡 ���� next�� ����δ�.
					// �ٸ� �����忡 ���� last.ptr->next�� ��尡 ���� ���� ����� nullptr�� �ƴϹǷ� �����Ѵ�.
					if (InterlockedCompareExchangePointer((volatile PVOID*)&last.ptr->next, node, next) == nullptr)
					{
						// tail = node;
						// CAS�� �������� ������ �ٸ� �����忡 ���� ������ tail�� ��ġ�� ���ƿ´�.
						// ���������� �ٸ� �����忡 ���� tail�� �Ű��� ���̹Ƿ� true�� �����Ѵ�.
						if (InterlockedCompareExchange128(
							(LONG64*)&tail,
							tail.stamp + 1, (LONG64)node,
							(LONG64*)&last))
						{
							// InterlockedIncrement64((long long*)&m_size);
						}
						return true;
					}
				}
				// next != nullptr �̶�� ���� �ٸ� �����尡 enq�� �õ��ϴ� ��
				// tail.ptr->next = node; ���Ը� �����ϰ� tail�� �ű��� ���� ���¸� �ǹ��ϹǷ�
				// tail�� �ű�� ���� �����ش�.
				else
				{
					InterlockedCompareExchange128(
						(LONG64*)&tail,
						tail.stamp + 1, (LONG64)next,	// ���� ���� ���� node�� �ƴ϶� next�ӿ� ��������.
						(LONG64*)&last);
				}
			}
		}
	}

	bool Dequeue(int* value)
	{
		StampNode first;
		StampNode last;
		Node* next;

		int ret;

		while (true)
		{
			first = head;
			last = tail;
			next = first.ptr->next;

			if (first.ptr == head.ptr)
			{
				if (first.ptr == last.ptr)
				{
					// first.ptr == head.ptr && next == nullptr�̸� ť�� ����ִ� ����̴�.
					if (next == nullptr)
					{
						return false;
					}
					// first.ptr == last.ptr && next != nullptr�̸� �ٸ� �����尡 enq�� �õ��ϴ� ��
					// tail.ptr->next = next; ���Ը� �����ϰ� tail�� �ű��� ���� ���¸� �ǹ��ϹǷ�
					// tail�� �ű�� ���� �����ش�.
					else
					{
						InterlockedCompareExchange128(
							(LONG64*)&tail,
							tail.stamp + 1, (LONG64)next,
							(LONG64*)&last
						);
					}
				}
				else
				{
					// �� ��츦 ��� ó���ߴ��� �ش� ��ġ���� ����ǰ�
					// �ٸ� �����尡 ť�� ���� �׼��� ������ �߻��Ѵ�.
					if (next != nullptr)	// Ȯ�� ���
					{
						ret = next->value;
						if (InterlockedCompareExchange128(
							(LONG64*)&head,
							head.stamp + 1, (LONG64)next,
							(LONG64*)&first))
						{
							m_pool.Free(first.ptr);
							first.ptr = nullptr;
							*value = ret;
							return true;
						}
					}
				}
			}
		}
	}

	void Display(int count)
	{
		// printf("cnt1 : %lld, cnt2 : %lld, cnt3 : %lld\n", cnt1, cnt2, cnt3);
		Node* p = head.ptr->next;
		while (count > 0 && p != nullptr)
		{
			printf("%d, ", p->value);
			p = p->next;
			--count;
		}
		printf("\n");
	}
};
*/
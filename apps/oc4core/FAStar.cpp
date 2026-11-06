#include "FAStar.h"

#include <algorithm>
#include <utility>

namespace oc4::host
{
	namespace
	{
		int indexOf(const FAStar& f, const FAStarNode* n) { return static_cast<int>(n - f.nodes.data()); }

		bool inRange(const FAStar& f, int x, int y) { return x >= 0 && x < f.columns && y >= 0 && y < f.rows; }

		FAStarNode* nodeAt(FAStar& f, int x, int y)
		{
			if (f.wrapX)
				x = ((x % f.columns) + f.columns) % f.columns;
			if (f.wrapY)
				y = ((y % f.rows) + f.rows) % f.rows;
			return inRange(f, x, y) ? &f.nodes[static_cast<size_t>(y) * f.columns + x] : nullptr;
		}

		int heuristicFor(const FAStar& f, const FAStarNode& n)
		{
			return f.heuristic && inRange(f, f.destX, f.destY) ? f.heuristic(n.m_iX, n.m_iY, f.destX, f.destY) : 0;
		}

		// ------------------------------------------------------------ open list (binary heap)

		// a pops before b
		bool before(const FAStar& f, int a, int b)
		{
			const int fa = f.nodes[a].m_iTotalCost, fb = f.nodes[b].m_iTotalCost;
			return fa != fb ? fa < fb : f.stamp[a] > f.stamp[b];
		}

		void place(FAStar& f, size_t pos, int node)
		{
			f.heap[pos] = node;
			f.heapPos[node] = static_cast<int>(pos);
		}

		void siftUp(FAStar& f, size_t pos)
		{
			const int node = f.heap[pos];
			while (pos > 0)
			{
				const size_t parent = (pos - 1) / 2;
				if (!before(f, node, f.heap[parent]))
					break;
				place(f, pos, f.heap[parent]);
				pos = parent;
			}
			place(f, pos, node);
		}

		void siftDown(FAStar& f, size_t pos)
		{
			const int node = f.heap[pos];
			const size_t n = f.heap.size();
			for (;;)
			{
				size_t child = 2 * pos + 1;
				if (child >= n)
					break;
				if (child + 1 < n && before(f, f.heap[child + 1], f.heap[child]))
					++child;
				if (!before(f, f.heap[child], node))
					break;
				place(f, pos, f.heap[child]);
				pos = child;
			}
			place(f, pos, node);
		}

		void pushOpen(FAStar& f, FAStarNode* n)
		{
			const int i = indexOf(f, n);
			n->m_eFAStarListType = FASTARLIST_OPEN;
			f.stamp[i] = ++f.clock;
			f.heap.push_back(i);
			siftUp(f, f.heap.size() - 1);
			if (f.notifyList)
				f.notifyList(nullptr, n, f.heap.size() == 1 ? ASNL_STARTOPEN : ASNL_ADDOPEN, f.data, &f);
		}

		// An open node's f changed: same as taking it out of the sorted list and inserting it again.
		void reopen(FAStar& f, FAStarNode* n)
		{
			const int i = indexOf(f, n);
			f.stamp[i] = ++f.clock;
			const size_t pos = static_cast<size_t>(f.heapPos[i]);
			siftUp(f, pos);
			siftDown(f, static_cast<size_t>(f.heapPos[i]));
		}

		FAStarNode* popOpen(FAStar& f)
		{
			if (f.heap.empty())
				return nullptr;
			const int top = f.heap.front();
			f.heapPos[top] = -1;
			const int last = f.heap.back();
			f.heap.pop_back();
			if (!f.heap.empty())
			{
				place(f, 0, last);
				siftDown(f, 0);
			}
			return &f.nodes[top];
		}

		// ------------------------------------------------------------ search

		void reset(FAStar& f)
		{
			for (const int i : f.touched)
			{
				f.nodes[i].clear();
				f.heapPos[i] = -1;
			}
			f.touched.clear();
			f.heap.clear();
			f.stack.clear();
			f.lastNode = nullptr;
			f.reusable = false;
		}

		void touch(FAStar& f, FAStarNode* n)
		{
			if (n->m_eFAStarListType == NO_FASTARLIST)
				f.touched.push_back(indexOf(f, n));
		}

		void relink(FAStar& f, FAStarNode* parent, FAStarNode* n, int g, int how)
		{
			n->m_pParent = parent;
			n->m_iKnownCost = g;
			n->m_iTotalCost = g + n->m_iHeuristicCost;
			if (f.notifyChild)
				f.notifyChild(parent, n, how, f.data, &f);
		}

		int stepCost(FAStar& f, FAStarNode* from, FAStarNode* to) { return f.cost ? f.cost(from, to, 0, f.data, &f) : 1; }

		// A cheaper route reached a closed node: pass it on to everything below it.
		void updateParents(FAStar& f, FAStarNode* node)
		{
			f.stack.push_back(node);
			while (!f.stack.empty())
			{
				FAStarNode* parent = f.stack.back();
				f.stack.pop_back();
				parent->m_bOnStack = false;
				for (int c = 0; c < parent->m_iNumChildren; ++c)
				{
					FAStarNode* kid = parent->m_apChildren[c];
					const int g = parent->m_iKnownCost + stepCost(f, parent, kid);
					if (g >= kid->m_iKnownCost)
						continue;
					relink(f, parent, kid, g, ASNC_PARENTADD_UP);
					if (kid->m_eFAStarListType == FASTARLIST_OPEN)
						reopen(f, kid);
					if (!kid->m_bOnStack)
					{
						kid->m_bOnStack = true;
						f.stack.push_back(kid);
					}
				}
			}
		}

		void linkChild(FAStar& f, FAStarNode* node, FAStarNode* child)
		{
			const int g = node->m_iKnownCost + stepCost(f, node, child);
			if (node->m_iNumChildren < 8)
				node->m_apChildren[node->m_iNumChildren++] = child;

			switch (child->m_eFAStarListType)
			{
			case FASTARLIST_OPEN:
				if (g < child->m_iKnownCost)
				{
					relink(f, node, child, g, ASNC_OPENADD_UP);
					reopen(f, child);
				}
				break;
			case FASTARLIST_CLOSED:
				if (g < child->m_iKnownCost)
				{
					relink(f, node, child, g, ASNC_CLOSEDADD_UP);
					updateParents(f, child);
				}
				break;
			default:
				touch(f, child);
				child->m_iHeuristicCost = heuristicFor(f, *child);
				relink(f, node, child, g, ASNC_NEWADD);
				pushOpen(f, child);
				break;
			}
		}

		void createChildren(FAStar& f, FAStarNode* node)
		{
			for (int dx = -1; dx <= 1; ++dx)
			{
				for (int dy = -1; dy <= 1; ++dy)
				{
					if ((dx == 0 && dy == 0) || (f.cardinalOnly && dx != 0 && dy != 0))
						continue;
					FAStarNode* child = nodeAt(f, node->m_iX + dx, node->m_iY + dy);
					if (!child)
						continue;
					if (f.valid && !f.valid(node, child, 0, f.data, &f))
						continue;
					linkChild(f, node, child);
				}
			}
		}

		// Expands the open list until the destination is closed.
		bool run(FAStar& f)
		{
			FAStarNode* dest = inRange(f, f.destX, f.destY) ? nodeAt(f, f.destX, f.destY) : nullptr;
			if (dest && dest->m_eFAStarListType == FASTARLIST_CLOSED)
			{
				f.lastNode = dest;
				return true;
			}
			while (FAStarNode* best = popOpen(f))
			{
				if (f.notifyList)
					f.notifyList(nullptr, best, ASNL_DELETEOPEN, f.data, &f);
				best->m_eFAStarListType = FASTARLIST_CLOSED;
				if (f.notifyList)
					f.notifyList(nullptr, best, ASNL_ADDCLOSED, f.data, &f);
				if (best == dest)
				{
					f.lastNode = best;
					return true;
				}
				createChildren(f, best);
			}
			f.lastNode = nullptr;
			return false;
		}
	}

	void PathFinder::Initialize(FAStar* f, int iColumns, int iRows, bool bWrapX, bool bWrapY, FAPointFunc DestValidFunc, FAHeuristic HeuristicFunc,
		FAStarFunc CostFunc, FAStarFunc ValidFunc, FAStarFunc NotifyChildFunc, FAStarFunc NotifyListFunc, void* pData)
	{
		reset(*f);
		f->columns = std::max(iColumns, 0);
		f->rows = std::max(iRows, 0);
		f->wrapX = bWrapX;
		f->wrapY = bWrapY;
		f->destValid = DestValidFunc;
		f->heuristic = HeuristicFunc;
		f->cost = CostFunc;
		f->valid = ValidFunc;
		f->notifyChild = NotifyChildFunc;
		f->notifyList = NotifyListFunc;
		f->data = pData;
		const size_t count = static_cast<size_t>(f->columns) * f->rows;
		f->nodes.assign(count, FAStarNode());
		for (int y = 0; y < f->rows; ++y)
			for (int x = 0; x < f->columns; ++x)
			{
				FAStarNode& n = f->nodes[static_cast<size_t>(y) * f->columns + x];
				n.m_iX = x;
				n.m_iY = y;
			}
		f->heapPos.assign(count, -1);
		f->stamp.assign(count, 0);
		f->heap.reserve(count);
		f->touched.reserve(count);
	}

	bool PathFinder::GeneratePath(FAStar* pf, int iXstart, int iYstart, int iXdest, int iYdest, bool bCardinalOnly, int iInfo, bool bReuse)
	{
		FAStar& f = *pf;
		const bool sameSearch = f.reusable && bReuse && iXstart == f.startX && iYstart == f.startY && iInfo == f.info &&
			bCardinalOnly == f.cardinalOnly && f.data == f.searchData;
		const bool newDest = iXdest != f.destX || iYdest != f.destY;
		f.destX = iXdest;
		f.destY = iYdest;

		if (sameSearch)
		{
			if (f.destValid && inRange(f, iXdest, iYdest) && !f.destValid(iXdest, iYdest, f.data, &f))
			{
				f.lastNode = nullptr;
				return false;
			}
			if (newDest)
			{
				// Closed nodes keep their cost; the open list is re-ranked for the new goal.
				for (const int i : f.heap)
				{
					FAStarNode& n = f.nodes[i];
					n.m_iHeuristicCost = heuristicFor(f, n);
					n.m_iTotalCost = n.m_iKnownCost + n.m_iHeuristicCost;
				}
				for (size_t i = f.heap.size() / 2; i-- > 0;)
					siftDown(f, i);
			}
			return run(f);
		}

		reset(f);
		f.startX = iXstart;
		f.startY = iYstart;
		f.info = iInfo;
		f.cardinalOnly = bCardinalOnly;
		f.searchData = f.data;

		FAStarNode* start = inRange(f, iXstart, iYstart) ? nodeAt(f, iXstart, iYstart) : nullptr;
		if (!start)
			return false;
		if (f.destValid && inRange(f, iXdest, iYdest) && !f.destValid(iXdest, iYdest, f.data, &f))
			return false;
		if (f.valid && !f.valid(nullptr, start, 0, f.data, &f))
			return false;

		f.reusable = true;
		touch(f, start);
		start->m_iKnownCost = 0;
		start->m_iHeuristicCost = heuristicFor(f, *start);
		start->m_iTotalCost = start->m_iHeuristicCost;
		pushOpen(f, start);
		if (f.notifyChild)
			f.notifyChild(nullptr, start, ASNC_INITIALADD, f.data, &f);
		return run(f);
	}
}

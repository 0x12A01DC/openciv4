#pragma once

// The engine's A* pathfinder (CvDLLFAStarIFaceBase). The DLL supplies the rules as
// callbacks and reads the result through FAStarNode (m_pParent chain, m_iData1/2).
//
// Semantics the DLL relies on:
//  - one node per plot; neighbours are the 8 surrounding plots (4 with bCardinalOnly),
//    wrapped on wrapping maps;
//  - ValidFunc(NULL, start) is asked first, then ValidFunc(parent, child) per neighbour;
//  - NotifyChildFunc runs whenever a node gets a (new) parent, so it can derive the
//    node's data from the parent's: ASNC_INITIALADD for the start, ASNC_NEWADD,
//    ASNC_OPENADD_UP / ASNC_CLOSEDADD_UP for a cheaper route, ASNC_PARENTADD_UP when
//    a cheaper route propagates to the children of a closed node;
//  - NotifyListFunc(ASNL_ADDCLOSED) for every node taken off the open list (the area,
//    plot group and irrigation flood fills count and mark plots there);
//  - a destination of (-1, -1) explores everything reachable and returns false;
//  - bReuse keeps the previous search when start, info and data are unchanged, until
//    ForceReset (the DLL resets whenever the map or the moving group changes).
//
// The open list pops the lowest f, and among equal f the most recently added node,
// i.e. the order of a list sorted by insertion before the first node with f >= new f.

#include "CvGameCoreDLL.h"
#include "CvDLLFAStarIFaceBase.h"
#include "FAStarNode.h"

#include <cstdint>
#include <vector>

class FAStar
{
public:
	// Configuration (Initialize / SetData)
	int columns = 0;
	int rows = 0;
	bool wrapX = false;
	bool wrapY = false;
	FAPointFunc destValid = nullptr;
	FAHeuristic heuristic = nullptr;
	FAStarFunc cost = nullptr;
	FAStarFunc valid = nullptr;
	FAStarFunc notifyChild = nullptr;
	FAStarFunc notifyList = nullptr;
	const void* data = nullptr;

	// Last search
	int startX = -1, startY = -1;
	int destX = -1, destY = -1;
	int info = 0;
	bool cardinalOnly = false;
	const void* searchData = nullptr;
	bool reusable = false;
	FAStarNode* lastNode = nullptr;

	std::vector<FAStarNode> nodes;  // columns * rows, indexed y * columns + x like CvMap plots
	std::vector<int> touched;       // nodes to clear before the next search
	std::vector<int> heap;          // open list
	std::vector<int> heapPos;       // node -> index in heap, -1 when not open
	std::vector<uint32_t> stamp;    // node -> time it was (re)added to the open list
	std::vector<FAStarNode*> stack; // UpdateParents work list
	uint32_t clock = 0;
};

namespace oc4::host
{
	class PathFinder : public CvDLLFAStarIFaceBase
	{
	public:
		FAStar* create() override { return new FAStar; }
		void destroy(FAStar*& ptr, bool) override
		{
			delete ptr;
			ptr = nullptr;
		}
		bool GeneratePath(FAStar*, int iXstart, int iYstart, int iXdest, int iYdest, bool bCardinalOnly, int iInfo, bool bReuse) override;
		void Initialize(FAStar*, int iColumns, int iRows, bool bWrapX, bool bWrapY, FAPointFunc DestValidFunc, FAHeuristic HeuristicFunc,
			FAStarFunc CostFunc, FAStarFunc ValidFunc, FAStarFunc NotifyChildFunc, FAStarFunc NotifyListFunc, void* pData) override;
		void SetData(FAStar* f, const void* pData) override { f->data = pData; }
		FAStarNode* GetLastNode(FAStar* f) override { return f->lastNode; }
		bool IsPathStart(FAStar* f, int iX, int iY) override { return iX == f->startX && iY == f->startY; }
		bool IsPathDest(FAStar* f, int iX, int iY) override { return iX == f->destX && iY == f->destY; }
		int GetStartX(FAStar* f) override { return f->startX; }
		int GetStartY(FAStar* f) override { return f->startY; }
		int GetDestX(FAStar* f) override { return f->destX; }
		int GetDestY(FAStar* f) override { return f->destY; }
		int GetInfo(FAStar* f) override { return f->info; }
		void ForceReset(FAStar* f) override { f->reusable = false; }
	};
}

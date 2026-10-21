#pragma once

#include "Box.h"

enum class EBVHCullResult : uint8 { Outside, Intersect, Inside };

template <typename T>
class TBVH
{
public:
	using FBoundsGetter = std::function<FBox(const T&)>;

	explicit TBVH(FBoundsGetter BoundsGetter) : BoundsGetter(std::move(BoundsGetter))
	{
		QueryStack.Reserve(MaxDepth + 1);
		TraceStack.Reserve(MaxDepth + 1);
		CullStack.Reserve(MaxDepth + 1);
	}

	void Build(std::span<const T> InElements);

	// 루트노드부터 Bounding Box 재계산
	void Refit();

	template <typename TBoundsPredicate, typename TVisitor>
	void Query(TBoundsPredicate&& BoundsTest, TVisitor&& Visitor) const;

	// Classify(const FBox&, uint32& Mask) -> EBVHCullResult.
	// Inside인 노드는 하위 원소를 검사 없이 모두 방문하고, Mask는 부모에서 통과한 평면을 자식에게 넘긴다.
	template <typename TClassify, typename TVisitor>
	void QueryCull(uint32 InitialMask, TClassify&& Classify, TVisitor&& Visitor) const;

	template <typename TBoundsPredicate, typename TRayHit>
	bool TraceClosest(TBoundsPredicate&& BoundsTrace, TRayHit&& LeafTrace, float& OutNearestT) const;

	void Clear();

private:
	static constexpr uint32 InvalidIndex = std::numeric_limits<uint32>::max();
	static constexpr uint32 MaxDepth = 32;
	static constexpr uint32 MinSplitSize = 2;
	static constexpr uint32 BinCount = 16;
	static constexpr uint32 PrefetchLevels = 4;

	struct FSlot
	{
		uint32 Child = InvalidIndex;
		uint32 Count = 0;

		bool IsLeaf() const
		{
			return Child == InvalidIndex;
		}
	};

	struct alignas(64) FNode
	{
		FBox LeftBounds;
		FBox RightBounds;
		FSlot Left;
		FSlot Right;
	};

	struct FElement
	{
		T Value;
		FBox Bounds;
	};

	struct FSplit
	{
		int32 Axis = -1;
		float Position = 0.0f;
		float Cost = std::numeric_limits<float>::max();
	};

	struct FQueryEntry
	{
		uint32 Node;
		uint32 First;
		uint32 Count;
	};

	struct FStackEntry
	{
		uint32 Node;
		uint32 First;
		uint32 Count;
		float EnterT;
	};

	struct FCullStackEntry
	{
		uint32 Node;
		uint32 First;
		uint32 Mask;
	};

	FSlot BuildNode(uint32 First, uint32 Count, uint32 Depth, FBox& OutBounds);
	FBox ComputeBounds(uint32 First, uint32 Count) const;
	FSplit FindBestSplit(uint32 First, uint32 Count, const FBox& Bounds) const;
	uint32 Partition(uint32 First, uint32 Count, int32 Axis, float Position);

	FBox RefitSlot(const FSlot& Slot, uint32 First);

	static float SurfaceArea(const FBox& Box);

	void CollectPrefetchNodes();

	FBoundsGetter BoundsGetter;

	TArray<FNode> Nodes;
	TArray<FElement> Elements;

	FSlot Root;
	FBox RootBounds;

	mutable TArray<FQueryEntry> QueryStack;
	mutable TArray<FStackEntry> TraceStack;
	mutable TArray<FCullStackEntry> CullStack;

	TArray<uint32> PrefetchNodes;
};

template <typename T>
void TBVH<T>::Build(std::span<T const> InElements)
{
	Clear();
	if (InElements.empty())
	{
		return;
	}

	Elements.Reserve(static_cast<uint32>(InElements.size()));
	for (const T& Element : InElements)
	{
		Elements.Emplace(Element, BoundsGetter(Element));
	}

	Root = BuildNode(0, static_cast<uint32>(Elements.size()), 0, RootBounds);

	CollectPrefetchNodes();
}

template <typename T>
void TBVH<T>::Refit()
{
	if (Elements.IsEmpty())
	{
		return;
	}
	RootBounds = RefitSlot(Root, 0);
}

template <typename T>
template <typename TBoundsPredicate, typename TVisitor>
void TBVH<T>::Query(TBoundsPredicate&& BoundsTest, TVisitor&& Visitor) const
{
	if (Elements.IsEmpty() || !BoundsTest(RootBounds))
	{
		return;
	}

	QueryStack.Reset();
	QueryStack.Add({ Root.Child, 0, Root.Count });
	while (!QueryStack.IsEmpty())
	{
		const FQueryEntry Entry = QueryStack.Last();
		QueryStack.RemoveLast();

		if (Entry.Node == InvalidIndex)
		{
			for (uint32 i = Entry.First; i < Entry.First + Entry.Count; ++i)
			{
				const FElement& Element = Elements[i];
				if (BoundsTest(Element.Bounds))
				{
					Visitor(Element.Value);
				}
			}
			continue;
		}

		const FNode& Node = Nodes[Entry.Node];
		if (BoundsTest(Node.LeftBounds))
		{
			QueryStack.Add({ Node.Left.Child, Entry.First, Node.Left.Count });
		}
		if (BoundsTest(Node.RightBounds))
		{
			QueryStack.Add({ Node.Right.Child, Entry.First + Node.Left.Count, Node.Right.Count });
		}
	}
}

template <typename T>
template <typename TClassify, typename TVisitor>
void TBVH<T>::QueryCull(uint32 InitialMask, TClassify&& Classify, TVisitor&& Visitor) const
{
	if (Elements.IsEmpty())
	{
		return;
	}

	const auto VisitChild = [&](const FBox& Bounds, const FSlot& Slot, uint32 First, uint32 ParentMask)
	{
		uint32 Mask = ParentMask;
		const EBVHCullResult Result = Mask ? Classify(Bounds, Mask) : EBVHCullResult::Inside;
		if (Result == EBVHCullResult::Outside)
		{
			return;
		}

		if (Result == EBVHCullResult::Inside)
		{
			const uint32 End = First + Slot.Count;
			for (uint32 i = First; i < End; ++i)
			{
				Visitor(Elements[i].Value);
			}
			return;
		}

		if (Slot.IsLeaf())
		{
			const uint32 End = First + Slot.Count;
			for (uint32 i = First; i < End; ++i)
			{
				const FElement& Element = Elements[i];
				uint32 ElementMask = Mask;
				if (Classify(Element.Bounds, ElementMask) != EBVHCullResult::Outside)
				{
					Visitor(Element.Value);
				}
			}
			return;
		}

		CullStack.Add({ Slot.Child, First, Mask });
	};

	CullStack.Reset();
	VisitChild(RootBounds, Root, 0, InitialMask);
	while (!CullStack.IsEmpty())
	{
		const FCullStackEntry Entry = CullStack.Last();
		CullStack.RemoveLast();

		const FNode& Node = Nodes[Entry.Node];
		VisitChild(Node.LeftBounds, Node.Left, Entry.First, Entry.Mask);
		VisitChild(Node.RightBounds, Node.Right, Entry.First + Node.Left.Count, Entry.Mask);
	}
}

template <typename T>
template <typename TBoundsTrace, typename TLeafTrace>
bool TBVH<T>::TraceClosest(TBoundsTrace&& BoundsTrace, TLeafTrace&& LeafTrace, float& OutNearestT) const
{
	if (Elements.IsEmpty())
	{
		return false;
	}

	float RootEnterT;
	if (!BoundsTrace(RootBounds, RootEnterT) || RootEnterT >= OutNearestT)
	{
		return false;
	}

	const FNode* NodeBase = Nodes.GetData();
	for (uint32 Index : PrefetchNodes)
	{
		_mm_prefetch(reinterpret_cast<const char*>(NodeBase + Index), _MM_HINT_T0);
	}

	TraceStack.Reset();
	TraceStack.Add({ Root.Child, 0, Root.Count, RootEnterT });

	bool bHit = false;
	while (!TraceStack.IsEmpty())
	{
		const FStackEntry Entry = TraceStack.Last();
		TraceStack.RemoveLast();

		if (Entry.EnterT >= OutNearestT)
		{
			continue;
		}

		if (Entry.Node == InvalidIndex)
		{
			const uint32 End = Entry.First + Entry.Count;
			for (uint32 i = Entry.First; i < End; ++i)
			{
				const FElement& Element = Elements[i];

				float ElementEnterT;
				if (!BoundsTrace(Element.Bounds, ElementEnterT) || ElementEnterT >= OutNearestT)
				{
					continue;
				}

				bHit |= LeafTrace(Element.Value, OutNearestT);
			}
			continue;
		}

		const FNode& Node = Nodes[Entry.Node];
		const uint32 RightFirst = Entry.First + Node.Left.Count;

		float LeftEnterT;
		const bool bLeftHit = BoundsTrace(Node.LeftBounds, LeftEnterT) && LeftEnterT < OutNearestT;

		float RightEnterT;
		const bool bRightHit = BoundsTrace(Node.RightBounds, RightEnterT) && RightEnterT < OutNearestT;

		const FStackEntry LeftEntry{ Node.Left.Child, Entry.First, Node.Left.Count, LeftEnterT };
		const FStackEntry RightEntry{ Node.Right.Child, RightFirst, Node.Right.Count, RightEnterT };

		if (bLeftHit && bRightHit)
		{
			if (LeftEnterT < RightEnterT)
			{
				TraceStack.Add(RightEntry);
				TraceStack.Add(LeftEntry);
			}
			else
			{
				TraceStack.Add(LeftEntry);
				TraceStack.Add(RightEntry);
			}
		}
		else if (bLeftHit)
		{
			TraceStack.Add(LeftEntry);
		}
		else if (bRightHit)
		{
			TraceStack.Add(RightEntry);
		}
	}

	return bHit;
}

template <typename T>
void TBVH<T>::Clear()
{
	Nodes.Reset();
	Elements.Reset();
	Root = FSlot{};
	RootBounds = FBox{};
}

template <typename T>
typename TBVH<T>::FSlot TBVH<T>::BuildNode(uint32 First, uint32 Count, uint32 Depth, FBox& OutBounds)
{
	OutBounds = ComputeBounds(First, Count);

	if (Count <= MinSplitSize || Depth >= MaxDepth)
	{
		return FSlot{ InvalidIndex, Count };
	}

	const FSplit Split = FindBestSplit(First, Count, OutBounds);
	if (Split.Axis == -1 || Split.Cost >= static_cast<float>(Count)) // 분할 후의 비용이 더 크다고 보이는 경우
	{
		return FSlot{ InvalidIndex, Count };
	}

	const uint32 Middle = Partition(First, Count, Split.Axis, Split.Position);
	if (Middle == First || Middle == First + Count)
	{
		return FSlot{ InvalidIndex, Count };
	}

	const uint32 Index = static_cast<uint32>(Nodes.Num());
	Nodes.Emplace();

	FBox LeftBounds;
	FBox RightBounds;
	const FSlot Left = BuildNode(First, Middle - First, Depth + 1, LeftBounds);
	const FSlot Right = BuildNode(Middle, First + Count - Middle, Depth + 1, RightBounds);

	FNode& Node = Nodes[Index]; // 재귀 중 Nodes가 재할당되어 참조가 무효화되는 경우 방지
	Node.LeftBounds = LeftBounds;
	Node.RightBounds = RightBounds;
	Node.Left = Left;
	Node.Right = Right;

	return FSlot{ Index, Count };
}

template <typename T>
FBox TBVH<T>::ComputeBounds(uint32 First, uint32 Count) const
{
	FBox Result{ FVector{ std::numeric_limits<float>::max() }, FVector{ std::numeric_limits<float>::lowest() } };
	for (uint32 i = First; i < First + Count; ++i)
	{
		const FElement& Element = Elements[i];
		
		Result.Min.X = std::min(Result.Min.X, Element.Bounds.Min.X);
		Result.Min.Y = std::min(Result.Min.Y, Element.Bounds.Min.Y);
		Result.Min.Z = std::min(Result.Min.Z, Element.Bounds.Min.Z);

		Result.Max.X = std::max(Result.Max.X, Element.Bounds.Max.X);
		Result.Max.Y = std::max(Result.Max.Y, Element.Bounds.Max.Y);
		Result.Max.Z = std::max(Result.Max.Z, Element.Bounds.Max.Z);
	}
	return Result;
}

// SAH(Surface Area Heuristic)
template <typename T>
typename TBVH<T>::FSplit TBVH<T>::FindBestSplit(uint32 First, uint32 Count, const FBox& Bounds) const
{
	const float ParentArea = SurfaceArea(Bounds);
	if (ParentArea <= std::numeric_limits<float>::epsilon())
	{
		return FSplit{};
	}

	FSplit BestSplit{};
	for (uint32 Axis = 0; Axis < 3; ++Axis)
	{
		float MinCenter = std::numeric_limits<float>::max();
		float MaxCenter = std::numeric_limits<float>::lowest();
		for (uint32 i = First; i < First + Count; ++i)
		{
			const FElement& Element = Elements[i];
			float Center = (Element.Bounds.Min[Axis] + Element.Bounds.Max[Axis]) * 0.5f;

			MinCenter = std::min(MinCenter, Center);
			MaxCenter = std::max(MaxCenter, Center);
		}

		float BinWidth = (MaxCenter - MinCenter) / static_cast<float>(BinCount);
		if (BinWidth <= std::numeric_limits<float>::epsilon())
		{
			continue;
		}

		std::array<uint32, BinCount - 1> LeftCounts{};
		std::array<FBox, BinCount - 1> LeftBounds{};
		std::array<uint32, BinCount - 1> RightCounts{};
		std::array<FBox, BinCount - 1> RightBounds{};
		for (uint32 i = 0; i < BinCount - 1; ++i)
		{
			LeftBounds[i] = FBox{ FVector{ std::numeric_limits<float>::max() }, FVector{ std::numeric_limits<float>::lowest() } };
			RightBounds[i] = FBox{ FVector{ std::numeric_limits<float>::max() }, FVector{ std::numeric_limits<float>::lowest() } };
		}

		for (uint32 i = First; i < First + Count; ++i)
		{
			const FElement& Element = Elements[i];
			float Center = (Element.Bounds.Min[Axis] + Element.Bounds.Max[Axis]) * 0.5f;
			uint32 BinIndex = static_cast<uint32>((Center - MinCenter) / BinWidth);
			BinIndex = std::clamp(BinIndex, 0u, BinCount - 1);
			if (BinIndex < BinCount - 1)
			{
				LeftBounds[BinIndex].Expand(Element.Bounds);
				++LeftCounts[BinIndex];
			}
			if (BinIndex > 0)
			{
				RightBounds[BinIndex - 1].Expand(Element.Bounds);
				++RightCounts[BinIndex - 1];
			}
		}

		for (uint32 i = 1; i < BinCount - 1; ++i)
		{
			LeftCounts[i] += LeftCounts[i - 1];
			LeftBounds[i].Expand(LeftBounds[i - 1]);
			RightCounts[BinCount - 2 - i] += RightCounts[BinCount - 1 - i];
			RightBounds[BinCount - 2 - i].Expand(RightBounds[BinCount - 1 - i]);
		}

		for (uint32 i = 1; i < BinCount; ++i)
		{
			if (LeftCounts[i - 1] == 0 || RightCounts[i - 1] == 0)
			{
				continue;
			}

			float LeftArea = SurfaceArea(LeftBounds[i - 1]);
			float RightArea = SurfaceArea(RightBounds[i - 1]);
			float Cost = 1.0f + (LeftArea * static_cast<float>(LeftCounts[i - 1]) + RightArea * static_cast<float>(RightCounts[i - 1])) / ParentArea;
			if (Cost < BestSplit.Cost)
			{
				BestSplit.Axis = static_cast<int32>(Axis);
				BestSplit.Position = MinCenter + BinWidth * static_cast<float>(i);
				BestSplit.Cost = Cost;
			}
		}
	}

	return BestSplit;
}

template <typename T>
uint32 TBVH<T>::Partition(uint32 First, uint32 Count, int32 Axis, float Position)
{
	auto Begin = Elements.begin() + First;
	auto End = Begin + Count;

	auto Middle = std::partition(Begin, End, [Axis, Position](const FElement& Element) {
		float Center = (Element.Bounds.Min[Axis] + Element.Bounds.Max[Axis]) * 0.5f;
		return Center < Position;
	});

	return static_cast<uint32>(Middle - Elements.begin());
}

template <typename T>
FBox TBVH<T>::RefitSlot(const FSlot& Slot, uint32 First)
{
	if (Slot.IsLeaf())
	{
		FBox Bounds = FBox{ FVector{ std::numeric_limits<float>::max() }, FVector{ std::numeric_limits<float>::lowest() } };

		for (uint32 i = First; i < First + Slot.Count; ++i)
		{
			FElement& Element = Elements[i];
			Element.Bounds = BoundsGetter(Element.Value);
			Bounds.Expand(Element.Bounds);
		}
		return Bounds;
	}

	FNode& Node = Nodes[Slot.Child];
	Node.LeftBounds = RefitSlot(Node.Left, First);
	Node.RightBounds = RefitSlot(Node.Right, First + Node.Left.Count);

	FBox Bounds = Node.LeftBounds;
	Bounds.Expand(Node.RightBounds);
	return Bounds;
}

template <typename T>
float TBVH<T>::SurfaceArea(const FBox& Box)
{
	const FVector Size = Box.Max - Box.Min;
	return 2.0f * (Size.X * Size.Y + Size.Y * Size.Z + Size.Z * Size.X);
}

template <typename T>
void TBVH<T>::CollectPrefetchNodes()
{
	PrefetchNodes.Reset();
	if (Root.IsLeaf())
		return;

	TArray<uint32> Level;
	Level.Add(Root.Child);
	for (uint32 Depth = 0; Depth < PrefetchLevels && !Level.IsEmpty(); ++Depth)
	{
		TArray<uint32> NextLevel;
		for (uint32 NodeIndex : Level)
		{
			const FNode& Node = Nodes[NodeIndex];
			if (!Node.Left.IsLeaf())
				NextLevel.Add(Node.Left.Child);
			if (!Node.Right.IsLeaf())
				NextLevel.Add(Node.Right.Child);
		}
		PrefetchNodes.Append(NextLevel);
		Level = std::move(NextLevel);
	}
}

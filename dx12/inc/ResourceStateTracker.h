#pragma once

constexpr u32 SUBRESOURCE_ALL = 0xFFFFFFFF;

struct Context;
struct CommandList;


// SubresourceKey — identifies a resource + subresource for tracking
struct SubresourceKey {
	ID3D12Resource* resource;
	u32 subresource;

	bool operator==(const SubresourceKey& other) const {
		return resource == other.resource && subresource == other.subresource;
	}

	struct Hash {
		std::size_t operator()(const SubresourceKey& key) const {
			std::size_t h1 = std::hash<void*>{}(key.resource);
			std::size_t h2 = std::hash<u32>{}(key.subresource);
			return h1 ^ (h2 << 1);
		}
	};
};


// Tracks the known state of a texture within a single command list recording.
struct LocalTextureState {
	D3D12_BARRIER_LAYOUT layout;
	D3D12_BARRIER_ACCESS lastAccess;
	D3D12_BARRIER_SYNC lastSync;
};

// Tracks the known state of a buffer within a single command list recording.
struct LocalBufferState {
	D3D12_BARRIER_ACCESS lastAccess;
	D3D12_BARRIER_SYNC lastSync;
};

using TextureStateMap = std::unordered_map<SubresourceKey, LocalTextureState, SubresourceKey::Hash>;
using BufferStateMap = std::unordered_map<SubresourceKey, LocalBufferState, SubresourceKey::Hash>;


// Per-command-list object that records barrier intents during command list recording.
// Not thread-safe (each command list has its own recorder, used from one thread).
class ResourceStateTracker {
public:
	void Init(D3D12_COMMAND_LIST_TYPE commandListType);

	// Record a texture state transition.
	void TransitionTexture(
		ID3D12Resource* resource,
		u32 subresource,
		D3D12_BARRIER_SYNC syncAfter,
		D3D12_BARRIER_ACCESS accessAfter,
		D3D12_BARRIER_LAYOUT layoutAfter,
		bool discard = false
	);

	// Record a buffer state transition.
	void TransitionBuffer(
		ID3D12Resource* resource,
		D3D12_BARRIER_SYNC syncAfter,
		D3D12_BARRIER_ACCESS accessAfter
	);

	void UAVBufferBarrier(ID3D12Resource* resource);
	void UAVTextureBarrier(ID3D12Resource* resource, 
		u32 firstSubresource, u32 numSubresources);

	void FlushImmediateBarriers(CommandList* commandList);
	void Reset();

private:
	friend class GlobalLayoutTracker;

	D3D12_COMMAND_LIST_TYPE m_type;

	// Local states for resources touched in this command list
	TextureStateMap m_localTextureStates;
	BufferStateMap m_localBufferStates;

	// Immediate barriers resolved during recording
	std::vector<D3D12_TEXTURE_BARRIER> m_immediateTextureBarriers;
	std::vector<D3D12_BUFFER_BARRIER> m_immediateBufferBarriers;

	// Pending barriers whose LayoutBefore is unknown at record time
	std::vector<D3D12_TEXTURE_BARRIER> m_pendingTextureBarriers;
};

// Tracks the persistent layout of all registered texture subresources.
class GlobalLayoutTracker {
public:

	void Init(Context* context);

	void Register(ID3D12Resource* resource, D3D12_BARRIER_LAYOUT initialLayout);
	void Unregister(ID3D12Resource* resource);

	// Commit final texture layouts. This must be called when the command list submitted
	void CommitFinalLayoutStates(std::vector<ResourceStateTracker*> trackers);

	// Gather and resolve pending barriers on all provided trackers
	ID3D12GraphicsCommandList7* ResolvePendingBarriers(std::vector<ResourceStateTracker*> trackers);

private:
	ComPtr<ID3D12CommandAllocator> m_pendingAllocator;
	ComPtr<ID3D12GraphicsCommandList7> m_pendingCommandList;
	std::shared_mutex m_mutex;
	std::unordered_map<SubresourceKey, D3D12_BARRIER_LAYOUT, SubresourceKey::Hash> m_layouts;
};

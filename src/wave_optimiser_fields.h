#pragma once

#include "plugin_interface.h"
#include "Engine_classes.hpp"
#include "Chimera_structs.hpp"
#include "ChimeraMassCommon_structs.hpp"
#include "MassActors_structs.hpp"
#include "plugin_map.h"

#include <cstdint>
#include <cstring>
#include <iterator>

// Every member of a game class the plugin reads, taken from the loader's map
// for the running game build (see plugin_map.h). Resolved once in
// OnPluginLoadHooks; read on the game thread with In(object), which is null
// when the object is null or the member did not resolve, and the feature
// using it is skipped. Engine members (UObject::Outer, UWorld) are not game
// classes and stay compiled in.

#define WAVE_OPTIMISER_FIELD(Class, Member) SR_FIELD(decltype(SDK::Class::Member), Class, Member, 0)

namespace WaveOptimiserFields
{
	// FCrMassTemperatureFragment, reached through FMassEntityManager's
	// fragment lookup.
	inline auto CurrentHeat    = WAVE_OPTIMISER_FIELD(FCrMassTemperatureFragment, CurrentHeat);
	inline auto LastUpdateTime = WAVE_OPTIMISER_FIELD(FCrMassTemperatureFragment, LastUpdateTime);

	// FMassActorFragment, reached the same way.
	inline auto FragmentActor = WAVE_OPTIMISER_FIELD(FMassActorFragment, Actor);

	// The game's own constructor fills a stack buffer of this struct and the
	// actor's identity is read from it, so its size is checked on the running
	// build.
	inline auto ReplicationHelperActor = WAVE_OPTIMISER_FIELD(FCrMassActorReplicationHelper, Actor);
	inline auto ReplicationHelperSize  = SR_SIZE_CHECK(FCrMassActorReplicationHelper);

	// UMassEntitySubsystem::EntityManager and UMassActorSubsystem::ActorManager
	// are TSharedPtrs the Game-SDK folds into Pad_38 (not reflected), so there
	// is no SDK name to take an offset from. A stale offset would read whatever
	// now sits there, so on a build the loader has no row for, the plugin is
	// refused instead.
	// CL-127004: 0x38 for both; the layout diff lists no move on CL-128786. The
	// rows below are this plugin's own until the loader's map carries them.
	inline auto EntityManager = PluginField<void*>{ { "UMassEntitySubsystem::EntityManager",
		"UMassEntitySubsystem", "EntityManager", PLUGIN_FIELD_NATIVE, sizeof(void*), 0x38 } };
	inline auto ActorManager = PluginField<void*>{ { "UMassActorSubsystem::ActorManager",
		"UMassActorSubsystem", "ActorManager", PLUGIN_FIELD_NATIVE, sizeof(void*), 0x38 } };

	inline const PluginFieldCandidate kEntityManagerRows[] = {
		{ 127004, 0x38, static_cast<uint32_t>(sizeof(void*)), 0, nullptr },
		{ 128786, 0x38, static_cast<uint32_t>(sizeof(void*)), 0, nullptr },
	};
	inline const PluginFieldCandidate kActorManagerRows[] = {
		{ 127004, 0x38, static_cast<uint32_t>(sizeof(void*)), 0, nullptr },
		{ 128786, 0x38, static_cast<uint32_t>(sizeof(void*)), 0, nullptr },
	};

	// Asks the loader for every row above. False when any did not resolve; the
	// loader has then recorded the failure and refuses the plugin.
	inline bool Resolve(IPluginSelf* self, IPluginHookScanner* scanner)
	{
		bool nativeOk = PluginMap::ResolveField(scanner, self, EntityManager,
			kEntityManagerRows, static_cast<uint32_t>(std::size(kEntityManagerRows)));
		nativeOk = PluginMap::ResolveField(scanner, self, ActorManager,
			kActorManagerRows, static_cast<uint32_t>(std::size(kActorManagerRows))) && nativeOk;
		return PluginMap::ResolveFields(scanner, self,
			CurrentHeat, LastUpdateTime, FragmentActor,
			ReplicationHelperActor, ReplicationHelperSize) && nativeOk;
	}

	// The FMassEntityManager* a UMassEntitySubsystem holds, or null when the
	// subsystem is null or the member did not resolve.
	inline void* EntityManagerOf(const void* subsystem)
	{
		void* const* at = EntityManager.In(subsystem);
		return at ? *at : nullptr;
	}

	// The FMassActorManager* a UMassActorSubsystem holds, or null.
	inline void* ActorManagerOf(const void* subsystem)
	{
		void* const* at = ActorManager.In(subsystem);
		return at ? *at : nullptr;
	}

	// The live actor an FMassActorFragment points at, or null.
	inline SDK::AActor* ActorOf(const void* fragment)
	{
		const auto* weak = FragmentActor.In(fragment);
		return weak ? weak->Get() : nullptr;
	}

	// The {object index, serial number} identity of the actor in a
	// FCrMassActorReplicationHelper, which is the key the actor manager looks
	// entities up by. Zero when the member did not resolve.
	inline uint64_t ActorKeyOf(const void* helper)
	{
		uint64_t key = 0;
		if (const auto* weak = ReplicationHelperActor.In(helper))
			std::memcpy(&key, weak, sizeof(key));
		return key;
	}
}

#pragma once

// Windows defines this macro with the same name as the domain time method.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <NavalCommand/Classification/Api/Alignment.h>
#include <NavalCommand/Environment/Api/ITimeProvider.h>
#include <NavalCommand/Equipment/Api/IProvider.h>
#include <NavalCommand/Factions/Api/IProvider.h>
#include <NavalCommand/Observations/Api/IInstanceProvider.h>
#include <NavalCommand/Operations/Api/IProvider.h>
#include <NavalCommand/Resources/Api/IProvider.h>
#include <NavalCommand/Simulation/Api/IPauseController.h>
#include <NavalCommand/Simulation/Api/IPauseProvider.h>
#include <NavalCommand/Simulation/Api/ITimeCompressionController.h>
#include <NavalCommand/Simulation/Api/ITimeCompressionProvider.h>
#include <NavalCommand/Units/Api/IInstanceProvider.h>
#include <NavalCommand/Units/Api/ITypeProvider.h>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>

namespace NavalCommand::World
{
	struct Providers
	{
		Units::Api::IInstanceProvider& Units;
		Observations::Api::IInstanceProvider& Observations;
		Operations::Api::IProvider& Operations;
		Factions::Api::IProvider& Factions;
		Units::Api::ITypeProvider& Types;
		Equipment::Api::IProvider& Equipment;
		Resources::Api::IProvider& Resources;
		Environment::Api::ITimeProvider& Time;
		Simulation::Api::IPauseProvider& Pause;
		Simulation::Api::IPauseController& PauseControl;
		Simulation::Api::ITimeCompressionProvider& Compression;
		Simulation::Api::ITimeCompressionController& CompressionControl;
	};

	// Metadata and retained lifetime describe acquisition, not domain semantics.
	// The source must bind all reads to this same session and logical revision.
	struct UpstreamView
	{
		std::shared_ptr<void> Lifetime;
		Providers Interfaces;
		std::string Session;
		std::uint64_t Revision{};
	};

	using AcquireSource = std::function<Error::Api::Result<UpstreamView>(std::optional<Factions::Api::Id>)>;

	class Snapshot;

	class View final : public Units::Api::IInstanceProvider,
					   public Observations::Api::IInstanceProvider,
					   public Operations::Api::IProvider,
					   public Factions::Api::IProvider,
					   public Units::Api::ITypeProvider,
					   public Equipment::Api::IProvider,
					   public Resources::Api::IProvider,
					   public Environment::Api::ITimeProvider,
					   public Simulation::Api::IPauseProvider,
					   public Simulation::Api::IPauseController,
					   public Simulation::Api::ITimeCompressionProvider,
					   public Simulation::Api::ITimeCompressionController
	{
		friend class Engine;
		std::shared_ptr<const Snapshot> m_Snapshot;
		explicit View(std::shared_ptr<const Snapshot> snapshot);

	public:
		~View();
		const std::string& Session() const;
		std::uint64_t Revision() const;
		Error::Api::Result<std::vector<Units::Api::Id>> GetUnitInstanceIds() const override;
		Error::Api::Result<Units::Api::Instance> GetUnitInstance(const Units::Api::Id& id) const override;
		Error::Api::Result<std::vector<Observations::Api::Id>> GetObservationIds() const override;
		Error::Api::Result<Observations::Api::Instance> GetObservationInstance(const Observations::Api::Id& id) const override;
		Error::Api::Result<Operations::Api::UnitState> GetUnitOperations(const Units::Api::Id& id) const override;
		Error::Api::Result<std::vector<Factions::Api::Id>> GetFactionIds() override;
		Error::Api::DomainResult<Classification::Api::Alignment, Factions::Api::Error> GetFactionAlignment(const Factions::Api::Id& id, const Factions::Api::Id& second) override;
		Error::Api::Result<std::vector<Units::Api::TypeId>> GetUnitTypeIds() override;
		Error::Api::Result<Units::Api::Type> GetUnitType(const Units::Api::TypeId& id) override;
		Error::Api::Result<std::vector<Equipment::Api::TypeId>> GetEquipmentTypes() override;
		Error::Api::DomainResult<Equipment::Api::Type, Equipment::Api::Error> GetEquipmentType(const Equipment::Api::TypeId& id) const override;
		Error::Api::Result<std::vector<Resources::Api::Id>> GetResourceIds() const override;
		Error::Api::Result<std::vector<Resources::Api::Definition>> GetResourceDefinition(const Resources::Api::Id& id) const override;
		Error::Api::Result<Physics::Api::Seconds> GetCurrentTime() override;
		Error::Api::Result<bool> IsPaused() const override;
		Error::Api::Result<bool> CanRequestPause() const override;
		Error::Api::Result<bool> CanRequestUnpause() const override;
		Error::Api::Result<Simulation::Api::TimeCompressionFactor> GetTimeCompression() override;
		Error::Api::DomainResult<void, Simulation::Api::Error> RequestPause(bool paused) override;
		Error::Api::DomainResult<void, Simulation::Api::Error> SetTimeCompression(Simulation::Api::TimeCompressionFactor factor) override;
	};

	struct Status
	{
		bool Loading{true};
		bool Stale{};
		std::optional<Error::Api::Error> Failure;
	};

	// All source calls run without publication locks. AcquireView() returns a
	// retained immutable revision; consumers explicitly rebind to refresh.
	class Engine final
	{
		class State;
		std::unique_ptr<State> m_State;

	public:
		explicit Engine(AcquireSource source, std::chrono::milliseconds interval = std::chrono::milliseconds{250});
		~Engine();
		Engine(const Engine&) = delete;
		Engine& operator=(const Engine&) = delete;
		void Start();
		void Stop();
		void Select(std::optional<Factions::Api::Id> faction);
		std::shared_ptr<View> AcquireView() const;
		Status GetStatus() const;
		// Explicit synchronous refresh supports deterministic local-source tests.
		// Call on a worker, never on the rendering thread.
		Error::Api::Result<void> Refresh();
	};
}

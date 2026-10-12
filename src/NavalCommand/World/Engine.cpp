#include <NavalCommand/Error/Api/MakeError.h>
#include <NavalCommand/World/Engine.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace NavalCommand::World
{
	namespace
	{
		Error::Api::Error Unavailable()
		{
			return Error::Api::MakeError(Error::Api::ErrorCondition::NotReady);
		}

		template <class Result> Result Missing()
		{
			return std::unexpected(typename Result::error_type{Error::Api::MakeError(Error::Api::ErrorCondition::NotFound)});
		}

		template <class Result> Result Lookup(const std::unordered_map<std::string, Result>& values, const std::string& id)
		{
			auto found = values.find(id);
			return found == values.end() ? Missing<Result>() : found->second;
		}

		std::string Key(const std::optional<Factions::Api::Id>& faction)
		{
			return faction ? "faction:" + faction->Value : "omniscient";
		}
	}

	class Snapshot
	{
	public:
		UpstreamView Upstream;
		Error::Api::Result<std::vector<Units::Api::Id>> Units;
		Error::Api::Result<std::vector<Observations::Api::Id>> Observations;
		Error::Api::Result<std::vector<Factions::Api::Id>> Factions;
		Error::Api::Result<std::vector<Units::Api::TypeId>> Types;
		Error::Api::Result<std::vector<Equipment::Api::TypeId>> Equipment;
		Error::Api::Result<std::vector<Resources::Api::Id>> Resources;
		Error::Api::Result<Physics::Api::Seconds> Time;
		Error::Api::Result<bool> Paused;
		Error::Api::Result<bool> CanPause;
		Error::Api::Result<bool> CanUnpause;
		Error::Api::Result<Simulation::Api::TimeCompressionFactor> Compression;
		std::unordered_map<std::string, Error::Api::Result<Units::Api::Instance>> UnitValues;
		std::unordered_map<std::string, Error::Api::Result<Observations::Api::Instance>> ObservationValues;
		std::unordered_map<std::string, Error::Api::Result<Operations::Api::UnitState>> OperationValues;
		std::unordered_map<std::string, Error::Api::Result<Units::Api::Type>> TypeValues;
		std::unordered_map<std::string, Error::Api::DomainResult<Equipment::Api::Type, Equipment::Api::Error>> EquipmentValues;
		std::unordered_map<std::string, Error::Api::Result<std::vector<Resources::Api::Definition>>> ResourceValues;
		std::unordered_map<std::string, std::unordered_map<std::string, Error::Api::DomainResult<Factions::Api::Alignment, Factions::Api::Error>>> Alignments;

		explicit Snapshot(UpstreamView upstream) : Upstream{std::move(upstream)}
		{
		}
	};

	View::View(std::shared_ptr<const Snapshot> snapshot) : m_Snapshot{std::move(snapshot)}
	{
	}

	View::~View() = default;
	const std::string& View::Session() const
	{
		return m_Snapshot->Upstream.Session;
	}
	std::uint64_t View::Revision() const
	{
		return m_Snapshot->Upstream.Revision;
	}

	Error::Api::Result<std::vector<Units::Api::Id>> View::GetUnitInstanceIds() const
	{
		using Result = Error::Api::Result<std::vector<Units::Api::Id>>;
		return m_Snapshot->Units;
	}

	Error::Api::Result<Units::Api::Instance> View::GetUnitInstance(const Units::Api::Id& id) const
	{
		using Result = Error::Api::Result<Units::Api::Instance>;
		if (!m_Snapshot->Units)
		{
			return std::unexpected(m_Snapshot->Units.error());
		}
		return Lookup(m_Snapshot->UnitValues, id.Value);
	}

	Error::Api::Result<std::vector<Observations::Api::Id>> View::GetObservationIds() const
	{
		using Result = Error::Api::Result<std::vector<Observations::Api::Id>>;
		return m_Snapshot->Observations;
	}

	Error::Api::Result<Observations::Api::Instance> View::GetObservationInstance(const Observations::Api::Id& id) const
	{
		using Result = Error::Api::Result<Observations::Api::Instance>;
		if (!m_Snapshot->Observations)
		{
			return std::unexpected(m_Snapshot->Observations.error());
		}
		return Lookup(m_Snapshot->ObservationValues, id.Value);
	}

	Error::Api::Result<Operations::Api::UnitState> View::GetUnitOperations(const Units::Api::Id& id) const
	{
		using Result = Error::Api::Result<Operations::Api::UnitState>;
		if (!m_Snapshot->Units)
		{
			return std::unexpected(m_Snapshot->Units.error());
		}
		return Lookup(m_Snapshot->OperationValues, id.Value);
	}

	Error::Api::Result<std::vector<Factions::Api::Id>> View::GetFactionIds()
	{
		using Result = Error::Api::Result<std::vector<Factions::Api::Id>>;
		return m_Snapshot->Factions;
	}

	Error::Api::DomainResult<Factions::Api::Alignment, Factions::Api::Error> View::GetFactionAlignment(const Factions::Api::Id& id, const Factions::Api::Id& second)
	{
		using Result = Error::Api::DomainResult<Factions::Api::Alignment, Factions::Api::Error>;
		if (!m_Snapshot->Factions)
		{
			return std::unexpected(Result::error_type{m_Snapshot->Factions.error()});
		}
		auto first = m_Snapshot->Alignments.find(id.Value);
		if (first == m_Snapshot->Alignments.end())
		{
			return Missing<Result>();
		}
		return Lookup(first->second, second.Value);
	}

	Error::Api::Result<std::vector<Units::Api::TypeId>> View::GetUnitTypeIds()
	{
		using Result = Error::Api::Result<std::vector<Units::Api::TypeId>>;
		return m_Snapshot->Types;
	}

	Error::Api::Result<Units::Api::Type> View::GetUnitType(const Units::Api::TypeId& id)
	{
		using Result = Error::Api::Result<Units::Api::Type>;
		if (!m_Snapshot->Types)
		{
			return std::unexpected(m_Snapshot->Types.error());
		}
		return Lookup(m_Snapshot->TypeValues, id.Value);
	}

	Error::Api::Result<std::vector<Equipment::Api::TypeId>> View::GetEquipmentTypes()
	{
		using Result = Error::Api::Result<std::vector<Equipment::Api::TypeId>>;
		return m_Snapshot->Equipment;
	}

	Error::Api::DomainResult<Equipment::Api::Type, Equipment::Api::Error> View::GetEquipmentType(const Equipment::Api::TypeId& id) const
	{
		using Result = Error::Api::DomainResult<Equipment::Api::Type, Equipment::Api::Error>;
		if (!m_Snapshot->Equipment)
		{
			return std::unexpected(Result::error_type{m_Snapshot->Equipment.error()});
		}
		return Lookup(m_Snapshot->EquipmentValues, id.Value);
	}

	Error::Api::Result<std::vector<Resources::Api::Id>> View::GetResourceIds() const
	{
		using Result = Error::Api::Result<std::vector<Resources::Api::Id>>;
		return m_Snapshot->Resources;
	}

	Error::Api::Result<std::vector<Resources::Api::Definition>> View::GetResourceDefinition(const Resources::Api::Id& id) const
	{
		using Result = Error::Api::Result<std::vector<Resources::Api::Definition>>;
		if (!m_Snapshot->Resources)
		{
			return std::unexpected(m_Snapshot->Resources.error());
		}
		return Lookup(m_Snapshot->ResourceValues, id.Value);
	}

	Error::Api::Result<Physics::Api::Seconds> View::GetCurrentTime()
	{
		using Result = Error::Api::Result<Physics::Api::Seconds>;
		return m_Snapshot->Time;
	}

	Error::Api::Result<bool> View::IsPaused() const
	{
		using Result = Error::Api::Result<bool>;
		return m_Snapshot->Paused;
	}

	Error::Api::Result<bool> View::CanRequestPause() const
	{
		using Result = Error::Api::Result<bool>;
		return m_Snapshot->CanPause;
	}

	Error::Api::Result<bool> View::CanRequestUnpause() const
	{
		using Result = Error::Api::Result<bool>;
		return m_Snapshot->CanUnpause;
	}

	Error::Api::Result<Simulation::Api::TimeCompressionFactor> View::GetTimeCompression()
	{
		using Result = Error::Api::Result<Simulation::Api::TimeCompressionFactor>;
		return m_Snapshot->Compression;
	}

	Error::Api::DomainResult<void, Simulation::Api::Error> View::RequestPause(bool paused)
	{
		return m_Snapshot->Upstream.Interfaces.PauseControl.RequestPause(paused);
	}

	Error::Api::DomainResult<void, Simulation::Api::Error> View::SetTimeCompression(Simulation::Api::TimeCompressionFactor factor)
	{
		return m_Snapshot->Upstream.Interfaces.CompressionControl.SetTimeCompression(factor);
	}

	class Engine::State
	{
	public:
		AcquireSource Source;
		std::chrono::milliseconds Interval;
		mutable std::mutex Mutex;
		std::mutex RefreshMutex;
		std::condition_variable Changed;
		std::optional<Factions::Api::Id> Selected;
		std::uint64_t SelectionGeneration{};
		std::string Session;
		Status CurrentStatus;
		std::atomic<std::shared_ptr<View>> Current;
		std::unordered_map<std::string, std::shared_ptr<View>> Perspectives;
		std::jthread Worker;

		State(AcquireSource source, std::chrono::milliseconds interval) : Source{std::move(source)}, Interval{interval}
		{
			if (!Source || interval.count() <= 0)
			{
				throw std::invalid_argument("source_and_positive_interval_required");
			}
		}
	};

	Engine::Engine(AcquireSource source, std::chrono::milliseconds interval) : m_State{std::make_unique<State>(std::move(source), interval)}
	{
	}

	Engine::~Engine()
	{
		Stop();
	}

	void Engine::Start()
	{
		if (m_State->Worker.joinable())
		{
			return;
		}
		m_State->Worker = std::jthread(
			[this](std::stop_token stop)
			{
				while (!stop.stop_requested())
				{
					std::uint64_t generation;
					{
						std::lock_guard lock{m_State->Mutex};
						generation = m_State->SelectionGeneration;
					}
					const auto refreshResult = Refresh();
					std::unique_lock lock{m_State->Mutex};
					m_State->Changed.wait_for(lock,
											  m_State->Interval,
											  [&]
											  {
												  return stop.stop_requested() || generation != m_State->SelectionGeneration;
											  });
				}
			});
	}

	void Engine::Stop()
	{
		m_State->Worker.request_stop();
		m_State->Changed.notify_all();
		if (m_State->Worker.joinable())
		{
			m_State->Worker.join();
		}
	}

	void Engine::Select(std::optional<Factions::Api::Id> faction)
	{
		std::lock_guard lock{m_State->Mutex};
		if (m_State->Selected == faction)
		{
			return;
		}
		m_State->Selected = std::move(faction);
		++m_State->SelectionGeneration;
		m_State->Current.store(nullptr);
		m_State->CurrentStatus = {};
		m_State->Changed.notify_all();
	}

	std::shared_ptr<View> Engine::AcquireView() const
	{
		return m_State->Current.load();
	}

	Status Engine::GetStatus() const
	{
		std::lock_guard lock{m_State->Mutex};
		return m_State->CurrentStatus;
	}

	Error::Api::Result<void> Engine::Refresh()
	{
		// Serialize refresh operations, but never block readers or selection on I/O.
		std::lock_guard refreshLock{m_State->RefreshMutex};
		std::optional<Factions::Api::Id> faction;
		std::uint64_t generation;
		{
			std::lock_guard lock{m_State->Mutex};
			faction = m_State->Selected;
			generation = m_State->SelectionGeneration;
		}
		auto fail = [&](Error::Api::Error error) -> Error::Api::Result<void>
		{
			std::lock_guard lock{m_State->Mutex};
			if (generation == m_State->SelectionGeneration)
			{
				m_State->CurrentStatus = {false, m_State->Current.load() != nullptr, error};
				if (error.Condition == Error::Api::ErrorCondition::NotReady)
				{
					m_State->Current.store(nullptr);
					m_State->Perspectives.clear();
				}
			}
			return std::unexpected(error);
		};
		try
		{
			auto acquired = m_State->Source(faction);
			if (!acquired)
			{
				return fail(acquired.error());
			}
			if (acquired->Session.empty() || !acquired->Lifetime)
			{
				return fail(Unavailable());
			}
			{
				std::lock_guard lock{m_State->Mutex};
				if (generation != m_State->SelectionGeneration)
				{
					return {};
				}
				if (m_State->Session != acquired->Session)
				{
					m_State->Session = acquired->Session;
					m_State->Current.store(nullptr);
					m_State->Perspectives.clear();
				}
				auto retained = m_State->Perspectives.find(Key(faction));
				if (retained != m_State->Perspectives.end() && retained->second->Revision() == acquired->Revision)
				{
					m_State->Current.store(retained->second);
					m_State->CurrentStatus = {false, false, {}};
					return {};
				}
			}
			auto snapshot = std::make_shared<Snapshot>(std::move(*acquired));
			auto& providers = snapshot->Upstream.Interfaces;
			snapshot->Units = providers.Units.GetUnitInstanceIds();
			snapshot->Observations = providers.Observations.GetObservationIds();
			snapshot->Factions = providers.Factions.GetFactionIds();
			snapshot->Types = providers.Types.GetUnitTypeIds();
			snapshot->Equipment = providers.Equipment.GetEquipmentTypes();
			snapshot->Resources = providers.Resources.GetResourceIds();
			snapshot->Time = providers.Time.GetCurrentTime();
			snapshot->Paused = providers.Pause.IsPaused();
			snapshot->CanPause = providers.PauseControl.CanRequestPause();
			snapshot->CanUnpause = providers.PauseControl.CanRequestUnpause();
			snapshot->Compression = providers.Compression.GetTimeCompression();
			// Core tactical data failure cannot publish a partly refreshed picture.
			if (!snapshot->Units)
			{
				return fail(snapshot->Units.error());
			}
			if (!snapshot->Observations)
			{
				return fail(snapshot->Observations.error());
			}
			if (!snapshot->Factions)
			{
				return fail(snapshot->Factions.error());
			}
			if (!snapshot->Time)
			{
				return fail(snapshot->Time.error());
			}
			if (!snapshot->Paused)
			{
				return fail(snapshot->Paused.error());
			}
			if (!snapshot->Compression)
			{
				return fail(snapshot->Compression.error());
			}

			for (const auto& id : *snapshot->Units)
			{
				auto unit = providers.Units.GetUnitInstance(id);
				if (!unit)
				{
					return fail(unit.error());
				}
				if (unit->Identity != id || (faction && unit->Faction != *faction))
				{
					return fail(Error::Api::MakeError(Error::Api::ErrorCondition::PermissionDenied));
				}
				if (!snapshot->UnitValues.emplace(id.Value, std::move(unit)).second)
				{
					return fail(Error::Api::MakeError(Error::Api::ErrorCondition::ContractError));
				}
				snapshot->OperationValues.emplace(id.Value, providers.Operations.GetUnitOperations(id));
			}
			for (const auto& id : *snapshot->Observations)
			{
				auto observation = providers.Observations.GetObservationInstance(id);
				if (!observation)
				{
					return fail(observation.error());
				}
				if (observation->Identity != id || (faction && observation->ObserverFaction != *faction))
				{
					return fail(Error::Api::MakeError(Error::Api::ErrorCondition::PermissionDenied));
				}
				if (!snapshot->ObservationValues.emplace(id.Value, std::move(observation)).second)
				{
					return fail(Error::Api::MakeError(Error::Api::ErrorCondition::ContractError));
				}
			}
			for (const auto& first : *snapshot->Factions)
			{
				for (const auto& second : *snapshot->Factions)
				{
					snapshot->Alignments[first.Value].emplace(second.Value, providers.Factions.GetFactionAlignment(first, second));
				}
			}
			if (snapshot->Types)
			{
				for (const auto& id : *snapshot->Types)
				{
					snapshot->TypeValues.emplace(id.Value, providers.Types.GetUnitType(id));
				}
			}
			if (snapshot->Equipment)
			{
				for (const auto& id : *snapshot->Equipment)
				{
					snapshot->EquipmentValues.emplace(id.Value, providers.Equipment.GetEquipmentType(id));
				}
			}
			if (snapshot->Resources)
			{
				for (const auto& id : *snapshot->Resources)
				{
					snapshot->ResourceValues.emplace(id.Value, providers.Resources.GetResourceDefinition(id));
				}
			}
			auto view = std::shared_ptr<View>{new View(snapshot)};
			{
				std::lock_guard lock{m_State->Mutex};
				if (generation != m_State->SelectionGeneration)
				{
					return std::unexpected(Unavailable());
				}
				m_State->Perspectives[Key(faction)] = view;
				m_State->Current.store(view);
				m_State->CurrentStatus = {false, false, {}};
			}
			return {};
		}
		catch (const std::exception&)
		{
			return fail(Error::Api::MakeError(Error::Api::ErrorCondition::UnknownError));
		}
		catch (...)
		{
			return fail(Error::Api::MakeError(Error::Api::ErrorCondition::UnknownError));
		}
	}
}

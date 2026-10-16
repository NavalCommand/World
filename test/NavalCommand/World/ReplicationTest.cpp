#include <NavalCommand/Environment/Api/ITimeProviderMock.h>
#include <NavalCommand/Equipment/Api/IProviderMock.h>
#include <NavalCommand/Error/Api/MakeError.h>
#include <NavalCommand/Factions/Api/IProviderMock.h>
#include <NavalCommand/Observations/Api/IInstanceProviderMock.h>
#include <NavalCommand/Operations/Api/IProviderMock.h>
#include <NavalCommand/Resources/Api/IProviderMock.h>
#include <NavalCommand/Simulation/Api/IPauseControllerMock.h>
#include <NavalCommand/Simulation/Api/IPauseProviderMock.h>
#include <NavalCommand/Simulation/Api/ITimeCompressionControllerMock.h>
#include <NavalCommand/Simulation/Api/ITimeCompressionProviderMock.h>
#include <NavalCommand/Units/Api/IInstanceProviderMock.h>
#include <NavalCommand/Units/Api/ITypeProviderMock.h>
#include <NavalCommand/World/Engine.h>
#include <gtest/gtest.h>
#include <thread>
#include <utility>

namespace NavalCommand::World
{
	using testing::_;
	using testing::Invoke;
	using testing::Return;

	struct LocalSource
	{
		testing::NiceMock<Units::Api::IInstanceProviderMock> Units;
		testing::NiceMock<Observations::Api::IInstanceProviderMock> Observations;
		testing::NiceMock<Operations::Api::IProviderMock> Operations;
		testing::NiceMock<Factions::Api::IProviderMock> Factions;
		testing::NiceMock<Units::Api::ITypeProviderMock> Types;
		testing::NiceMock<Equipment::Api::IProviderMock> Equipment;
		testing::NiceMock<Resources::Api::IProviderMock> Resources;
		testing::NiceMock<Environment::Api::ITimeProviderMock> Time;
		testing::NiceMock<Simulation::Api::IPauseProviderMock> Pause;
		testing::NiceMock<Simulation::Api::IPauseControllerMock> PauseControl;
		testing::NiceMock<Simulation::Api::ITimeCompressionProviderMock> Compression;
		testing::NiceMock<Simulation::Api::ITimeCompressionControllerMock> CompressionControl;
		std::string Session{"local-session"};
		std::uint64_t Revision{1};
		std::string Faction{"blue"};
		std::string UnitName{"Initial"};
		std::string UnitTypeId{"frigate"};
		std::string SelectedLoadout{"frigate::loadout::empty"};
		bool UnitDestroyed{};
		Classification::Api::SubCategory UnitSubCategory{Classification::Api::SubCategory::Frigate};
		Classification::Api::Role PlatformRole{Classification::Api::Role::SurfaceWarfare};
		Classification::Api::Role LoadoutRole{Classification::Api::Role::AntiSubmarineWarfare};
		Classification::Api::Category ContactCategory{Classification::Api::Category::SurfaceVessel};
		std::string ObservedFaction{"red"};
		std::string ObservationLoadout{"frigate::loadout::empty"};
		bool ObservationDestroyed{};
		bool Available{true};
		std::atomic<int> Acquisitions{};

		Providers Interfaces()
		{
			return {Units, Observations, Operations, Factions, Types, Equipment, Resources, Time, Pause, PauseControl, Compression, CompressionControl};
		}

		LocalSource()
		{
			ON_CALL(Units, GetUnitInstanceIds()).WillByDefault(Return(std::vector<Units::Api::Id>{{"unit"}}));
			ON_CALL(Units, GetUnitInstance(_))
				.WillByDefault(Invoke(
					[this](const Units::Api::Id& id) -> Error::Api::Result<Units::Api::Instance>
					{
						Units::Api::Instance unit{};
						unit.Identity = id;
						unit.Name = UnitName;
						unit.Type.Value = UnitTypeId;
						unit.CurrentLoadout.Value = SelectedLoadout;
						unit.IsDestroyed = UnitDestroyed;
						unit.Faction.Value = Faction;
						return unit;
					}));
			ON_CALL(Observations, GetObservationIds())
				.WillByDefault(Return(std::vector<Observations::Api::Id>{{"type-only"}, {"faction-only"}}));
			ON_CALL(Observations, GetObservationInstance(_))
				.WillByDefault(Invoke(
					[this](const Observations::Api::Id& id) -> Error::Api::Result<Observations::Api::Instance>
					{
						Observations::Api::Instance observation{};
						observation.Identity = id;
						observation.ObserverFaction.Value = Faction;
						if (id.Value == "type-only")
						{
							observation.Type = decltype(observation.Type)::Known(ContactCategory);
							observation.CurrentLoadout = decltype(observation.CurrentLoadout)::Known({ObservationLoadout});
							observation.IsDestroyed = decltype(observation.IsDestroyed)::Known(ObservationDestroyed);
						}
						else
						{
							observation.ObservedFaction = decltype(observation.ObservedFaction)::Known({ObservedFaction});
						}
						observation.Kinematics.Position = Observations::Api::PartiallyLocalizedPosition{};
						return observation;
					}));
			ON_CALL(Operations, GetUnitOperations(_)).WillByDefault(Return(Operations::Api::UnitState{}));
			ON_CALL(Factions, GetFactionIds()).WillByDefault(Return(std::vector<Factions::Api::Id>{{"blue"}, {"red"}}));
			ON_CALL(Factions, GetFactionAlignment(_, _)).WillByDefault(Return(Classification::Api::Alignment::Neutral));
			ON_CALL(Types, GetUnitTypeIds()).WillByDefault(Invoke([this] { return std::vector<Units::Api::TypeId>{{UnitTypeId}}; }));
			ON_CALL(Types, GetUnitType(_)).WillByDefault(Invoke(
				[this](const Units::Api::TypeId& id) -> Error::Api::Result<Units::Api::Type>
				{
					Units::Api::Type type{};
					type.Id = id;
					type.UnitSubCategory = UnitSubCategory;
					type.PlatformRoles.push_back(PlatformRole);
					Equipment::Api::Loadout loadout{};
					loadout.Id = {SelectedLoadout};
					loadout.Roles.push_back(LoadoutRole);
					type.Loadouts.push_back(std::move(loadout));
					return type;
				}));
			ON_CALL(Equipment, GetEquipmentTypes()).WillByDefault(Return(std::vector<Equipment::Api::TypeId>{}));
			ON_CALL(Resources, GetResourceIds()).WillByDefault(Return(std::vector<Resources::Api::Id>{}));
			ON_CALL(Time, GetCurrentTime()).WillByDefault(Return(Physics::Api::Seconds{42}));
			ON_CALL(Pause, IsPaused()).WillByDefault(Return(true));
			ON_CALL(PauseControl, CanRequestPause()).WillByDefault(Return(true));
			ON_CALL(PauseControl, CanRequestUnpause()).WillByDefault(Return(true));
			ON_CALL(Compression, GetTimeCompression()).WillByDefault(Return(Simulation::Api::TimeCompressionFactor{1}));
		}
	};

	class WorldTest : public testing::Test
	{
	protected:
		std::shared_ptr<LocalSource> Source = std::make_shared<LocalSource>();

		AcquireSource Acquisition()
		{
			return [source = Source](std::optional<Factions::Api::Id> faction) -> Error::Api::Result<UpstreamView>
			{
				++source->Acquisitions;
				if (!source->Available)
				{
					return std::unexpected(Error::Api::MakeError(Error::Api::ErrorCondition::CommunicationError));
				}
				source->Faction = faction ? faction->Value : "blue";
				return UpstreamView{source, source->Interfaces(), source->Session, source->Revision};
			};
		}
	};

	TEST_F(WorldTest, RetainedViewsNeverMixRevisions)
	{
		Engine engine{Acquisition()};
		ASSERT_TRUE(engine.Refresh());
		auto first = engine.AcquireView();
		auto firstType = first->GetUnitType({"frigate"});
		ASSERT_TRUE(firstType);
		EXPECT_EQ(firstType->UnitSubCategory, Classification::Api::SubCategory::Frigate);
		ASSERT_EQ(firstType->PlatformRoles.size(), 1);
		EXPECT_EQ(firstType->PlatformRoles[0], Classification::Api::Role::SurfaceWarfare);
		ASSERT_EQ(firstType->Loadouts.size(), 1);
		EXPECT_EQ(firstType->Loadouts[0].Id.Value, "frigate::loadout::empty");
		ASSERT_EQ(firstType->Loadouts[0].Roles.size(), 1);
		EXPECT_EQ(firstType->Loadouts[0].Roles[0], Classification::Api::Role::AntiSubmarineWarfare);
		auto firstContact = first->GetObservationInstance({"type-only"});
		ASSERT_TRUE(firstContact);
		ASSERT_NE(firstContact->Type.GetIf<Classification::Api::Category>(), nullptr);
		EXPECT_EQ(*firstContact->Type.GetIf<Classification::Api::Category>(), Classification::Api::Category::SurfaceVessel);
		EXPECT_FALSE(firstContact->ObservedFaction.IsKnown());
		EXPECT_EQ(firstContact->CurrentLoadout.Value().Value, "frigate::loadout::empty");
		ASSERT_TRUE(firstContact->IsDestroyed.IsKnown());
		EXPECT_FALSE(firstContact->IsDestroyed.Value());
		auto firstFactionOnly = first->GetObservationInstance({"faction-only"});
		ASSERT_TRUE(firstFactionOnly);
		EXPECT_TRUE(firstFactionOnly->Type.IsUnknown());
		EXPECT_EQ(firstFactionOnly->ObservedFaction.Value().Value, "red");
		auto firstUnit = first->GetUnitInstance({"unit"});
		ASSERT_TRUE(firstUnit);
		EXPECT_EQ(firstUnit->CurrentLoadout.Value, "frigate::loadout::empty");
		EXPECT_FALSE(firstUnit->IsDestroyed);

		Source->UnitName = "Changed";
		Source->UnitTypeId = "destroyer";
		Source->SelectedLoadout = "destroyer::loadout::standard";
		Source->UnitDestroyed = true;
		Source->UnitSubCategory = Classification::Api::SubCategory::Destroyer;
		Source->PlatformRole = Classification::Api::Role::Strike;
		Source->LoadoutRole = Classification::Api::Role::LandAttack;
		Source->ContactCategory = Classification::Api::Category::Other;
		Source->ObservedFaction = "blue";
		Source->ObservationLoadout = "destroyer::loadout::standard";
		Source->ObservationDestroyed = true;
		++Source->Revision;
		ASSERT_TRUE(engine.Refresh());
		auto second = engine.AcquireView();
		auto secondType = second->GetUnitType({"destroyer"});
		ASSERT_TRUE(secondType);
		EXPECT_EQ(secondType->UnitSubCategory, Classification::Api::SubCategory::Destroyer);
		ASSERT_EQ(secondType->PlatformRoles.size(), 1);
		EXPECT_EQ(secondType->PlatformRoles[0], Classification::Api::Role::Strike);
		ASSERT_EQ(secondType->Loadouts.size(), 1);
		ASSERT_EQ(secondType->Loadouts[0].Roles.size(), 1);
		EXPECT_EQ(secondType->Loadouts[0].Roles[0], Classification::Api::Role::LandAttack);
		auto secondContact = second->GetObservationInstance({"type-only"});
		ASSERT_TRUE(secondContact);
		ASSERT_NE(secondContact->Type.GetIf<Classification::Api::Category>(), nullptr);
		EXPECT_EQ(*secondContact->Type.GetIf<Classification::Api::Category>(), Classification::Api::Category::Other);
		EXPECT_FALSE(secondContact->ObservedFaction.IsKnown());
		EXPECT_EQ(secondContact->CurrentLoadout.Value().Value, "destroyer::loadout::standard");
		EXPECT_TRUE(secondContact->IsDestroyed.Value());
		auto secondFactionOnly = second->GetObservationInstance({"faction-only"});
		ASSERT_TRUE(secondFactionOnly);
		EXPECT_TRUE(secondFactionOnly->Type.IsUnknown());
		EXPECT_EQ(secondFactionOnly->ObservedFaction.Value().Value, "blue");
		auto secondUnit = second->GetUnitInstance({"unit"});
		ASSERT_TRUE(secondUnit);
		EXPECT_EQ(secondUnit->CurrentLoadout.Value, "destroyer::loadout::standard");
		EXPECT_TRUE(secondUnit->IsDestroyed);

		EXPECT_EQ(first->GetUnitInstance({"unit"})->Name, "Initial");
		EXPECT_EQ(second->GetUnitInstance({"unit"})->Name, "Changed");
		EXPECT_EQ(firstType->UnitSubCategory, Classification::Api::SubCategory::Frigate);
		EXPECT_FALSE(firstContact->ObservedFaction.IsKnown());
		EXPECT_EQ(firstFactionOnly->ObservedFaction.Value().Value, "red");
		EXPECT_FALSE(firstContact->IsDestroyed.Value());
		EXPECT_FALSE(firstUnit->IsDestroyed);
		EXPECT_NE(first->Revision(), second->Revision());
		EXPECT_FALSE(first->GetUnitInstance({"hidden"}));
	}

	TEST_F(WorldTest, CachedReadsDoNotCallUpstream)
	{
		Engine engine{Acquisition()};
		ASSERT_TRUE(engine.Refresh());
		auto view = engine.AcquireView();
		EXPECT_CALL(Source->Units, GetUnitInstanceIds()).Times(0);
		EXPECT_CALL(Source->Units, GetUnitInstance(_)).Times(0);
		for (int reader = 0; reader < 20; ++reader)
		{
			EXPECT_EQ(view->GetUnitInstanceIds()->size(), 1);
			EXPECT_EQ(view->GetUnitInstance({"unit"})->Name, "Initial");
		}
	}

	TEST_F(WorldTest, FailureRetainsLastPictureAndMarksStale)
	{
		Engine engine{Acquisition()};
		ASSERT_TRUE(engine.Refresh());
		auto previous = engine.AcquireView();
		Source->Available = false;
		EXPECT_FALSE(engine.Refresh());
		EXPECT_EQ(engine.AcquireView(), previous);
		EXPECT_TRUE(engine.GetStatus().Stale);
		EXPECT_EQ(engine.GetStatus().Failure->Condition, Error::Api::ErrorCondition::CommunicationError);
		Source->Available = true;
		ASSERT_TRUE(engine.Refresh());
		EXPECT_FALSE(engine.GetStatus().Stale);
	}

	TEST_F(WorldTest, PerspectiveSwitchClearsOldPictureAndSeparatesIdentities)
	{
		Engine engine{Acquisition()};
		engine.Select(Factions::Api::Id{"blue"});
		ASSERT_TRUE(engine.Refresh());
		auto blue = engine.AcquireView();
		engine.Select(Factions::Api::Id{"red"});
		EXPECT_FALSE(engine.AcquireView());
		ASSERT_TRUE(engine.Refresh());
		auto red = engine.AcquireView();
		EXPECT_EQ(blue->GetObservationInstance({"type-only"})->ObserverFaction.Value, "blue");
		EXPECT_EQ(red->GetObservationInstance({"type-only"})->ObserverFaction.Value, "red");
		EXPECT_EQ(blue->GetUnitInstance({"unit"})->Faction.Value, "blue");
		EXPECT_EQ(red->GetUnitInstance({"unit"})->Faction.Value, "red");
	}

	TEST_F(WorldTest, MissionReplacementAndControlForwarding)
	{
		Engine engine{Acquisition()};
		ASSERT_TRUE(engine.Refresh());
		Source->Session = "replacement";
		ASSERT_TRUE(engine.Refresh());
		auto view = engine.AcquireView();
		EXPECT_EQ(view->Session(), "replacement");
		EXPECT_CALL(Source->PauseControl, RequestPause(false)).WillOnce(Return(Error::Api::DomainResult<void, Simulation::Api::Error>{}));
		EXPECT_CALL(Source->CompressionControl, SetTimeCompression(Simulation::Api::TimeCompressionFactor{4}))
			.WillOnce(Return(Error::Api::DomainResult<void, Simulation::Api::Error>{}));
		EXPECT_TRUE(view->RequestPause(false));
		EXPECT_TRUE(view->SetTimeCompression({4}));
	}

	TEST_F(WorldTest, UnchangedRevisionReusesCacheAndAvoidsProviderReads)
	{
		Engine engine{Acquisition()};
		ASSERT_TRUE(engine.Refresh());
		auto retained = engine.AcquireView();
		EXPECT_CALL(Source->Units, GetUnitInstanceIds()).Times(0);
		EXPECT_CALL(Source->Observations, GetObservationIds()).Times(0);
		ASSERT_TRUE(engine.Refresh());
		EXPECT_EQ(engine.AcquireView(), retained);
	}

	TEST_F(WorldTest, ConcurrentReadersRetainTheirRevisionDuringPublication)
	{
		Engine engine{Acquisition()};
		ASSERT_TRUE(engine.Refresh());
		std::vector<std::jthread> readers;
		for (int index = 0; index < 4; ++index)
		{
			readers.emplace_back(
				[&]
				{
					for (int read = 0; read < 500; ++read)
					{
						auto view = engine.AcquireView();
						ASSERT_TRUE(view);
						const auto revision = view->Revision();
						const auto name = view->GetUnitInstance({"unit"})->Name;
						std::this_thread::yield();
						EXPECT_EQ(view->Revision(), revision);
						EXPECT_EQ(view->GetUnitInstance({"unit"})->Name, name);
					}
				});
		}
		for (int publication = 0; publication < 20; ++publication)
		{
			Source->UnitName = "Publication " + std::to_string(publication);
			++Source->Revision;
			ASSERT_TRUE(engine.Refresh());
		}
	}

	TEST_F(WorldTest, ProviderExceptionMarksStaleAndCanRecover)
	{
		bool throwing{};
		auto acquire = Acquisition();
		Engine engine{[&](auto faction) -> Error::Api::Result<UpstreamView>
					  {
						  if (throwing)
						  {
							  throw std::runtime_error("local_provider_failed");
						  }
						  return acquire(faction);
					  }};
		ASSERT_TRUE(engine.Refresh());
		auto retained = engine.AcquireView();
		throwing = true;
		EXPECT_FALSE(engine.Refresh());
		EXPECT_TRUE(engine.GetStatus().Stale);
		EXPECT_EQ(engine.AcquireView(), retained);
		throwing = false;
		ASSERT_TRUE(engine.Refresh());
		EXPECT_FALSE(engine.GetStatus().Stale);
	}

	TEST_F(WorldTest, PollsByWallClockWhileSimulationIsPaused)
	{
		Engine engine{Acquisition(), std::chrono::milliseconds{10}};
		engine.Start();
		for (int attempt = 0; attempt < 100 && Source->Acquisitions < 3; ++attempt)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{5});
		}
		engine.Stop();
		EXPECT_GE(Source->Acquisitions.load(), 3);
		ASSERT_TRUE(engine.AcquireView());
		EXPECT_TRUE(*engine.AcquireView()->IsPaused());
	}
}

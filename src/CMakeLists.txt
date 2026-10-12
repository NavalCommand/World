add_library(NavalCommand.World STATIC NavalCommand/World/Engine.cpp)
target_include_directories(NavalCommand.World PUBLIC "${PROJECT_SOURCE_DIR}/public")
target_link_libraries(NavalCommand.World PUBLIC
    NavalCommand.Units.Api NavalCommand.Observations.Api NavalCommand.Operations.Api
    NavalCommand.Environment.Api NavalCommand.Simulation.Api)
NavalCommandConfigureTarget(NavalCommand.World)

cmake_minimum_required(VERSION 4.2)
cmake_policy(SET CMP0168 NEW)
project(NavalCommand.World VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(ncbuild GIT_REPOSITORY https://github.com/NavalCommand/Build.CMake.git GIT_TAG main)
FetchContent_MakeAvailable(ncbuild)
include("${ncbuild_SOURCE_DIR}/NavalCommand.Build.CMake.cmake")

NavalCommandAddDependency(https://github.com/NavalCommand/Domain.Api.git fbf6367bd9408fdd2f0324ecaf71bddf5e896803)

NavalCommandConfigureModule()

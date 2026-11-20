set(root "${BUILD_DIR}/package-consumer")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}/source")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${root}/prefix" RESULT_VARIABLE result OUTPUT_QUIET)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Core installation failed")
endif()
file(RENAME "${root}/prefix" "${root}/relocated prefix")
file(WRITE "${root}/source/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.24)
project(consumer LANGUAGES CXX)
find_package(ArbiterX 1.1.0...<1.2.0 CONFIG REQUIRED)
get_target_property(api ArbiterX::core ARBITER_X_API_VERSION)
if(NOT api STREQUAL "1")
  message(FATAL_ERROR "Unexpected API generation")
endif()
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE ArbiterX::core)
]=])
file(WRITE "${root}/source/main.cpp" [=[
#include <arbiter/world.hpp>
#include <arbiter/world_overrides.hpp>
int main() {
  arbiter::WorldConfig cfg; cfg.journal_events=true; cfg.journal_path="parent.journal";
  cfg.horizon_ns=50'000'000;
  std::string error;
  if(!arbiter::apply_world_override(cfg,"fee_per_lot","0",error)) return 2;
  arbiter::World world(cfg,42); world.run(500);
  const auto snapshot = world.capture_snapshot();
  if (!arbiter::write_world_snapshot("parent.wsnap", snapshot)) return 3;
  arbiter::World same(cfg,42);
  if (same.restore_snapshot(snapshot,cfg,true).code != arbiter::Errc::IoError) return 4;
  auto fresh=cfg; fresh.journal_path="continuation.journal";
  arbiter::World branch(fresh,42);
  if (!branch.restore_snapshot(snapshot,fresh,true)) return 5;
  branch.run(1);
  world.run();
  return world.state_hash()==1236319655606612024ull ? 0 : 1;
}
]=])
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${root}/source" -B "${root}/build" "-DCMAKE_PREFIX_PATH=${root}/relocated prefix" "-DCMAKE_CXX_COMPILER=${COMPILER}" -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Installed consumer configuration failed")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${root}/build" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Installed consumer build failed")
endif()
execute_process(COMMAND "${root}/build/consumer" WORKING_DIRECTORY "${root}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Installed consumer deterministic check failed")
endif()
set(cli "${root}/relocated prefix/bin/arbiter")
if(EXISTS "${cli}")
  file(SHA256 "${root}/parent.journal" original)
  foreach(args IN ITEMS "worldjump" "worldfork;--variable;half_spread=4")
    execute_process(COMMAND "${cli}" ${args} --snap parent.wsnap --events 50 WORKING_DIRECTORY "${root}" COMMAND_ERROR_IS_FATAL ANY)
    file(SHA256 "${root}/parent.journal" actual)
    if(NOT actual STREQUAL original)
      message(FATAL_ERROR "Snapshot continuation modified its parent journal")
    endif()
  endforeach()
endif()

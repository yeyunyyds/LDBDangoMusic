#pragma once
#include "world_camera.hpp"
#include <optional>
#include <string>
#include <vector>
namespace dango::inventory {
struct Record {
 uint32_t handle=0,index=0,serial=0;std::string className,modelName;int classId=-1;
 std::optional<bool> dormant,isCarryable;std::optional<Vec3> networkOrigin,networkAngles;
 std::optional<int> team,modelIndex,worldModelIndex,effects,character,health;std::optional<uint8_t> lifeState;std::optional<uint32_t> owner,weaponOwner,activeWeapon,viewModel,parent;
 std::optional<uint8_t> parentAttachment;
};
struct Frame {uint32_t epoch=0;uint64_t tick=0;unsigned occupied=0,unknownClass=0,unknownBase=0,unknownTables=0;std::vector<Record> records;uint32_t requestedFollowHandle=0xffffffffu,followedHandle=0xffffffffu;std::string followRoute;std::vector<std::string> getterHints,bindingChecks;bool modelLookup=false;std::string status="Entity diagnostics off";};
struct FollowRequest {bool enabled=false;uint32_t epoch=0,handle=0;};
}

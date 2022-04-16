#ifndef DYNOS_C_H
#define DYNOS_C_H
#ifndef __cplusplus

#include "dynos.h"
#include "types.h"
#include "src/game/moving_texture.h"

void *dynos_swap_cmd(void *cmd);

// -- built in -- //
void *dynos_update_cmd         (void *cmd);
void  dynos_update_gfx         ();
void  dynos_update_opt         (void *pad);
s32   dynos_gfx_import_texture (void **output, void *ptr, s32 tile, void *grapi, void **hashmap, void *pool, s32 *poolpos, s32 poolsize);
void  dynos_gfx_swap_animations(void *ptr);

// -- warps -- //
bool dynos_warp_to_level(s32 aLevel, s32 aArea, s32 aAct);
bool dynos_warp_restart_level(void);
bool dynos_warp_exit_level(s32 aDelay);
bool dynos_warp_to_castle(s32 aLevel);

// -- dynos packs -- //
int dynos_packs_get_count(void);
const char* dynos_packs_get(s32 index);
bool dynos_packs_get_enabled(s32 index);
void dynos_packs_set_enabled(s32 index, bool value);
void dynos_generate_packs(const char* directory);

// -- geos -- //
void dynos_add_actor_custom(const char *filePath, const char* geoName);
const void* dynos_geolayout_get(const char *name);

// -- collisions -- //
void dynos_add_collision(const char *filePath, const char* collisionName);
Collision* dynos_collision_get(const char* collisionName);

// -- movtexqcs -- //
void dynos_movtexqc_register(const char* name, s16 level, s16 area, s16 type);
struct MovtexQuadCollection* dynos_movtexqc_get_from_id(u32 id);
struct MovtexQuadCollection* dynos_movtexqc_get_from_index(s32 index);

// -- levels -- //
void dynos_add_level(s32 modIndex, const char *filePath, const char* levelName);
const char* dynos_level_get_token(u32 index);
Trajectory* dynos_level_get_trajectory(const char* name);
void dynos_level_load_background(void *ptr);

#endif
#endif

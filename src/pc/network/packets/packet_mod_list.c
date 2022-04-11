#include <stdio.h>
#include "../network.h"
#include "pc/mods/mods.h"
#include "pc/mods/mods_utils.h"
#include "pc/djui/djui.h"
#include "pc/debuglog.h"

void network_send_mod_list_request(void) {
    SOFT_ASSERT(gNetworkType == NT_CLIENT);
    mods_clear(&gRemoteMods);
    mods_clear(&gActiveMods);

    if (!mods_generate_remote_base_path()) {
        LOG_ERROR("Failed to generate remote base path!");
        return;
    }

    struct Packet p = { 0 };
    packet_init(&p, PACKET_MOD_LIST_REQUEST, true, PLMT_NONE);

    network_send_to((gNetworkPlayerServer != NULL) ? gNetworkPlayerServer->localIndex : 0, &p);
    LOG_INFO("sending mod list request");
}

void network_receive_mod_list_request(UNUSED struct Packet* p) {
    SOFT_ASSERT(gNetworkType == NT_SERVER);
    LOG_INFO("received mod list request");

    network_send_mod_list();
}

void network_send_mod_list(void) {
    SOFT_ASSERT(gNetworkType == NT_SERVER);

    struct Packet p = { 0 };
    packet_init(&p, PACKET_MOD_LIST, true, PLMT_NONE);

    char version[MAX_VERSION_LENGTH] = { 0 };
    snprintf(version, MAX_VERSION_LENGTH, "%s", get_version());
    LOG_INFO("sending version: %s", version);
    packet_write(&p, &version, sizeof(u8) * MAX_VERSION_LENGTH);

    packet_write(&p, &gActiveMods.entryCount, sizeof(u16));
    LOG_INFO("sent mod list (%u):", gActiveMods.entryCount);
    for (u16 i = 0; i < gActiveMods.entryCount; i++) {
        struct Mod* mod = gActiveMods.entries[i];

        u16 nameLength = strlen(mod->name);
        if (nameLength > 31) { nameLength = 31; }

        u16 relativePathLength = strlen(mod->relativePath);
        u64 modSize = mod->size;

        packet_write(&p, &nameLength, sizeof(u16));
        packet_write(&p, mod->name, sizeof(u8) * nameLength);
        packet_write(&p, &relativePathLength, sizeof(u16));
        packet_write(&p, mod->relativePath, sizeof(u8) * relativePathLength);
        packet_write(&p, &modSize, sizeof(u64));
        packet_write(&p, &mod->isDirectory, sizeof(u8));
        LOG_INFO("    '%s': %llu", mod->name, (u64)mod->size);

        packet_write(&p, &mod->fileCount, sizeof(u16));
        for (u16 j = 0; j < mod->fileCount; j++) {
            struct ModFile* file = &mod->files[j];
            u16 relativePathLength = strlen(file->relativePath);
            u64 fileSize = file->size;
            packet_write(&p, &relativePathLength, sizeof(u16));
            packet_write(&p, file->relativePath, sizeof(u8) * relativePathLength);
            packet_write(&p, &fileSize, sizeof(u64));
            LOG_INFO("      '%s': %llu", file->relativePath, (u64)file->size);
        }
    }
    network_send_to(0, &p);
}

void network_receive_mod_list(struct Packet* p) {
    SOFT_ASSERT(gNetworkType == NT_CLIENT);

    if (p->localIndex != UNKNOWN_LOCAL_INDEX) {
        if (gNetworkPlayerServer == NULL || gNetworkPlayerServer->localIndex != p->localIndex) {
            LOG_ERROR("Received download from known local index '%d'", p->localIndex);
            return;
        }
    }

    if (gRemoteMods.entries != NULL) {
        LOG_INFO("received mod list after allocating");
        return;
    }

    if (gNetworkServerAddr == NULL) {
        gNetworkServerAddr = network_duplicate_address(0);
    }

    char version[MAX_VERSION_LENGTH] = { 0 };
    snprintf(version, MAX_VERSION_LENGTH, "%s", get_version());
    LOG_INFO("client has version: %s", version);

    // verify version
    char remoteVersion[MAX_VERSION_LENGTH] = { 0 };
    packet_read(p, &remoteVersion, sizeof(u8) * MAX_VERSION_LENGTH);
    LOG_INFO("server has version: %s", version);
    if (memcmp(version, remoteVersion, MAX_VERSION_LENGTH) != 0) {
        network_shutdown(true);
        LOG_ERROR("version mismatch");
        char mismatchMessage[256] = { 0 };
        snprintf(mismatchMessage, 256, "\\#ffa0a0\\Error:\\#c8c8c8\\ Version mismatch.\n\nYour version: \\#a0a0ff\\%s\\#c8c8c8\\\nTheir version: \\#a0a0ff\\%s\\#c8c8c8\\\n\nSomeone is out of date!\n", version, remoteVersion);
        djui_panel_join_message_error(mismatchMessage);
        return;
    }

    packet_read(p, &gRemoteMods.entryCount, sizeof(u16));
    gRemoteMods.entries = calloc(gRemoteMods.entryCount, sizeof(struct Mod*));
    if (gRemoteMods.entries == NULL) {
        LOG_ERROR("Failed to allocate remote mod entries");
        return;
    }

    LOG_INFO("received mod list (%u):", gRemoteMods.entryCount);
    size_t totalSize = 0;
    for (u16 i = 0; i < gRemoteMods.entryCount; i++) {
        gRemoteMods.entries[i] = calloc(1, sizeof(struct Mod));
        struct Mod* mod = gRemoteMods.entries[i];
        if (mod == NULL) {
            LOG_ERROR("Failed to allocate remote mod!");
            return;
        }

        char name[32] = { 0 };
        u16 nameLength = 0;

        packet_read(p, &nameLength, sizeof(u16));
        if (nameLength > 31) {
            LOG_ERROR("Received name with invalid length!");
            return;
        }
        packet_read(p, name, nameLength * sizeof(u8));
        mod->name = strdup(name);

        u16 relativePathLength = 0;
        packet_read(p, &relativePathLength, sizeof(u16));
        packet_read(p, mod->relativePath, relativePathLength * sizeof(u8));
        packet_read(p, &mod->size, sizeof(u64));
        packet_read(p, &mod->isDirectory, sizeof(u8));
        normalize_path(mod->relativePath);
        totalSize += mod->size;
        LOG_INFO("    '%s': %llu", mod->name, (u64)mod->size);

        if (snprintf(mod->basePath, SYS_MAX_PATH - 1, "%s", gRemoteModsBasePath) < 0) {
            LOG_ERROR("Failed save remote base path!");
            return;
        }

        if (mod->size >= MAX_MOD_SIZE) {
            djui_popup_create("Server had too large of a mod.\nQuitting.", 4);
            network_shutdown(false);
            return;
        }

        packet_read(p, &mod->fileCount, sizeof(u16));
        mod->files = calloc(mod->fileCount, sizeof(struct ModFile));
        if (mod->files == NULL) {
            LOG_ERROR("Failed to allocate mod files!");
            return;
        }

        for (u16 j = 0; j < mod->fileCount; j++) {
            struct ModFile* file = &mod->files[j];
            u16 relativePathLength = 0;
            packet_read(p, &relativePathLength, sizeof(u16));
            packet_read(p, file->relativePath, relativePathLength * sizeof(u8));
            packet_read(p, &file->size, sizeof(u64));
            file->fp = NULL;
            if (mod->isDirectory && !strstr(file->relativePath, "actors") && !strstr(file->relativePath, "levels") && !strstr(file->relativePath, "sound")) {
                char tmp[SYS_MAX_PATH];
                if (snprintf(tmp, SYS_MAX_PATH, "%s-%s", mod->relativePath, file->relativePath) >= 0) {
                    memcpy(file->relativePath, tmp, strlen(tmp) + 1);
                }
            }
            normalize_path(file->relativePath);
            LOG_INFO("      '%s': %llu", file->relativePath, (u64)file->size);
        }
    }
    gRemoteMods.size = totalSize;

    network_start_download_requests();
}

#pragma once
#include <random>
#define WIN32_LEAN_AND_MEAN
#include <rpc.h>
#include <WinSock2.h>

using Address = UUID;

UUID UUIDHtoN(UUID uuid);
UUID UUIDNtoH(UUID uuid);

static std::random_device rd;
static std::mt19937 gen(rd());

namespace std {
	template<>
	struct hash<UUID> {
		size_t operator()(const UUID& o) const {
			size_t h = o.Data1 ^ o.Data2 ^ o.Data3;
			for (int i = 0; i < 8; ++i)
				h ^= o.Data4[i];
			return h;
		}
	};
}
#pragma once
#include <wchar.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	// Creates the context.
	bool lru_create();

	// Destroys the context.
	void lru_destroy();

	// Inserts an item / char
	bool lru_insert(uint16_t type, uint64_t ptr, unsigned char* val);

	// Finds a user by name.
	const unsigned char * lru_find(uint16_t type, uint64_t ptr, unsigned char* outVal, size_t outLen);

	// Delete a cachced item
	void lru_delete(uint16_t type, uint64_t ptr);

	// Resolve IDs
	void lru_resolve();

#ifdef __cplusplus
}
#endif

#include <stddef.h>
#include <stdint.h>

typedef void (*br_prng_seeder)(const void *ctx, const void *seed, size_t len);

br_prng_seeder br_prng_seeder_system(const char **name) {
    if (name) *name = "none";
    return 0;
}

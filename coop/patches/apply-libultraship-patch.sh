#!/bin/sh
# Applies the co-op headless-host patch to the libultraship submodule (idempotent).
# Run once after `git submodule update --init`, from anywhere.
set -e
root="$(cd "$(dirname "$0")/../.." && pwd)"
patch="$root/coop/patches/libultraship-headless.patch"
cd "$root/libultraship"
if git apply --check -R "$patch" 2>/dev/null; then
    echo "libultraship: co-op patch already applied."
elif git apply --check "$patch"; then
    git apply "$patch"
    echo "libultraship: co-op patch applied."
else
    echo "libultraship: the patch does not apply; the submodule is not at the expected commit (7f9b86a5)." >&2
    exit 1
fi

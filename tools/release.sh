#!/usr/bin/env bash
# Cuts a firmware release.
#
#   tools/release.sh            # build, publish the release, update the manifest
#   tools/release.sh --dry-run  # build and report sizes, publish nothing
#
# The version comes from esphome.project.version in muse-luxe.yaml, so bump that
# first. The script then:
#
#   1. builds the firmware,
#   2. checks the image still fits the app partition,
#   3. creates the GitHub release and uploads the factory and OTA images,
#   4. writes the new version and checksum into manifest_update.json.
#
# The manifest is what devices poll for updates, and it points at
# releases/latest/download/, so it only ever needs the checksum refreshed. Keeping
# that write in the same script as the build is what stops the two from drifting
# apart, which is easy to do when the checksum is pasted in by hand.

set -euo pipefail

cd "$(dirname "$0")/.."

YAML=muse-luxe.yaml
MANIFEST=manifest_update.json
OTA_ASSET=muse-luxe.ota.bin
FACTORY_ASSET=muse-luxe.factory.bin

# The app partition in partitions.csv. An image larger than this cannot be written
# to the inactive slot, so a device already in the field could not update to it.
APP_PARTITION_SIZE=$((0x1F0000))

dry_run=false
[ "${1:-}" = "--dry-run" ] && dry_run=true

command -v esphome >/dev/null || {
  echo "esphome is not on PATH. Install it with: pip install esphome" >&2
  exit 1
}
if ! $dry_run; then
  command -v gh >/dev/null || {
    echo "the GitHub CLI (gh) is not on PATH; install it or pass --dry-run" >&2
    exit 1
  }
fi

version=$(python3 - "$YAML" <<'PY'
import re, sys
text = open(sys.argv[1]).read()
match = re.search(r'^  project:\n(?:.*\n)*?    version: "([^"]+)"', text, re.M)
if not match:
    sys.exit("Could not find esphome.project.version in " + sys.argv[1])
print(match.group(1))
PY
)
tag="v${version}"
echo "Building ${tag}"

esphome compile "$YAML"

build_dir=$(dirname "$(find .esphome/build -name firmware.ota.bin -print -quit)")
[ -d "$build_dir" ] || { echo "No firmware.ota.bin produced by the build" >&2; exit 1; }

cp "${build_dir}/firmware.ota.bin" "$OTA_ASSET"
cp "${build_dir}/firmware.factory.bin" "$FACTORY_ASSET"

size=$(wc -c < "$OTA_ASSET")
printf 'OTA image: %s bytes (%d%% of the %s byte app partition)\n' \
  "$size" "$((size * 100 / APP_PARTITION_SIZE))" "$APP_PARTITION_SIZE"
if [ "$size" -gt "$APP_PARTITION_SIZE" ]; then
  echo "Image does not fit the app partition; devices in the field could not update to it." >&2
  exit 1
fi

checksum=$(python3 -c "import hashlib,sys;print(hashlib.md5(open(sys.argv[1],'rb').read()).hexdigest())" "$OTA_ASSET")
echo "md5: ${checksum}"

if $dry_run; then
  echo "Dry run: not publishing ${tag}."
  exit 0
fi

# Resolve the repository from the origin remote. Without this, gh picks whichever
# remote it likes and will happily aim at the upstream you forked from.
repo=$(git remote get-url origin | sed -E 's#(git@github\.com:|https://github\.com/)##; s#\.git$##')
echo "Publishing to ${repo}"

# Create and push the tag with git rather than letting gh do it: gh's tag creation
# goes through an API path that asks for the 'workflow' OAuth scope.
if ! git rev-parse -q --verify "refs/tags/${tag}" >/dev/null; then
  git tag "$tag"
fi
git push --quiet origin "$tag"

if gh release view "$tag" -R "$repo" >/dev/null 2>&1; then
  echo "Release ${tag} already exists; uploading over its assets."
  gh release upload "$tag" "$OTA_ASSET" "$FACTORY_ASSET" -R "$repo" --clobber
else
  gh release create "$tag" "$OTA_ASSET" "$FACTORY_ASSET" \
    -R "$repo" --verify-tag \
    --title "$tag" \
    --notes "Firmware ${version} for the Raspiaudio Muse Luxe, built with $(esphome version | head -1).

- \`${FACTORY_ASSET}\` — full flash image, for a device being set up for the first time.
- \`${OTA_ASSET}\` — over-the-air image, served to devices through \`manifest_update.json\`."
fi

python3 - "$MANIFEST" "$version" "$checksum" <<'PY'
import json, sys

path, version, checksum = sys.argv[1], sys.argv[2], sys.argv[3]
with open(path) as handle:
    manifest = json.load(handle)

manifest["version"] = version
for build in manifest["builds"]:
    build["ota"]["md5"] = checksum

with open(path, "w") as handle:
    json.dump(manifest, handle, indent=2)
    handle.write("\n")
PY

echo
echo "Published ${tag}. Commit and push ${MANIFEST} so devices are offered the update:"
echo "  git commit -am 'Release ${version}' && git push"

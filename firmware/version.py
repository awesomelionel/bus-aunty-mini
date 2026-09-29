# PlatformIO extra script: stamps the firmware with a version and adds a
# `mergebin` target that writes one flashable image per board.
#
# The version is BUS_AUNTY_VERSION when set (the release workflow sets it to
# the tag), otherwise `git describe`, so a local build reads like
# v0.2.0-3-gabc1234-dirty and can always be traced back to a commit.
import os
import subprocess

Import("env")


def resolve_version():
    explicit = os.environ.get("BUS_AUNTY_VERSION", "").strip()
    if explicit:
        return explicit
    try:
        return subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=env.subst("$PROJECT_DIR"),
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return "dev"


VERSION = resolve_version()

# A generated header rather than a -D flag: a changed define rebuilds every
# file, whereas this only rebuilds the ones that include the header, and the
# file is left untouched when the version has not changed.
gen_dir = os.path.join(env.subst("$BUILD_DIR"), "generated")
os.makedirs(gen_dir, exist_ok=True)
header = os.path.join(gen_dir, "bus_aunty_version.h")
content = "#pragma once\n#define BUS_AUNTY_VERSION \"%s\"\n" % VERSION.replace(
    "\\", "").replace("\"", "")
try:
    with open(header) as f:
        current = f.read()
except OSError:
    current = None
if current != content:
    with open(header, "w") as f:
        f.write(content)
env.Append(CPPPATH=[gen_dir])
print("Bus Aunty firmware version: %s" % VERSION)


def merge_bin(source, target, env):
    # Bootloader, partition table and app at their flash offsets, in one file
    # that esptool or ESP Web Tools can write at 0x0 to a blank board.
    images = []
    for offset, image in env.get("FLASH_EXTRA_IMAGES", []):
        images += [env.subst(offset), env.subst(image)]
    images += [env.subst("$ESP32_APP_OFFSET"),
               env.subst("$BUILD_DIR/${PROGNAME}.bin")]
    out = os.path.join(env.subst("$BUILD_DIR"), "bus-aunty-%s-%s.bin" % (
        env.subst("$PIOENV"), VERSION))
    cmd = [env.subst("$PYTHONEXE"), env.subst("$OBJCOPY"),
           "--chip", env.BoardConfig().get("build.mcu"),
           "merge_bin", "-o", out] + images
    print(" ".join('"%s"' % c for c in cmd))
    return subprocess.call(cmd)


env.AddCustomTarget(
    name="mergebin",
    dependencies="$BUILD_DIR/${PROGNAME}.bin",
    actions=merge_bin,
    title="Merge binary",
    description="Write a single image to flash at 0x0",
)

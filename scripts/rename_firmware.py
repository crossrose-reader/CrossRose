"""
PlatformIO post-build script: copy firmware.bin to convenient artifact names
in the same build directory.

Default outputs:
  .pio/build/default/firmware-x3-x4.bin
  .pio/build/x4-pro/firmware-x4-pro.bin
  .pio/build/x4-classic/firmware-x4-classic.bin
  .pio/build/sticky/firmware-sticky.bin
  .pio/build/x4-pro/firmware-x4-pro.bin
  .pio/build/picco/firmware-picco.bin

Release outputs when CROSSINK_RELEASE_VERSION is set:
  .pio/build/default/firmware-x3-x4-v<version>.bin
  .pio/build/x4-pro/firmware-x4-pro-v<version>.bin
  .pio/build/x4-classic/firmware-x4-classic-v<version>.bin
  .pio/build/sticky/firmware-sticky-v<version>.bin
  .pio/build/x4-pro/firmware-x4-pro-v<version>.bin
  .pio/build/picco/firmware-picco-v<version>.bin
"""

import os
import re
import shutil
import sys


def _copy_artifact(src, dst):
    shutil.copy(src, dst)
    print(f'Firmware copied to: {dst}')


def _get_project_option(env, name):
    try:
        value = env.GetProjectOption(name)
    except Exception:
        return None
    if isinstance(value, str):
        value = value.strip()
    return value or None


def _get_firmware_device_type(env):
    return _get_project_option(env, 'custom_firmware_device_type') or env['PIOENV']


def _get_release_version(env):
    version = _get_project_option(env, 'custom_release_version') or os.environ.get('CROSSINK_RELEASE_VERSION')
    if not version:
        return None
    version = version.strip()
    if not version:
        return None
    if not version.startswith('v'):
        version = f'v{version}'
    return re.sub(r'[^A-Za-z0-9._-]+', '-', version)


def rename_firmware(source, target, env):
    src = str(target[0])
    build_dir = os.path.dirname(src)

    device_type = _get_firmware_device_type(env)
    default_dst = os.path.join(build_dir, f'firmware-{device_type}.bin')
    _copy_artifact(src, default_dst)

    release_version = _get_release_version(env)
    if release_version:
        release_dst = os.path.join(build_dir, f'firmware-{device_type}-{release_version}.bin')
        _copy_artifact(src, release_dst)


try:
    Import('env')                                           # noqa: F821  # type: ignore[name-defined]
    env.AddPostAction(                                      # noqa: F821  # type: ignore[name-defined]
        '$BUILD_DIR/${PROGNAME}.bin',
        rename_firmware,
    )
except NameError:
    print('rename_firmware.py: must be run via PlatformIO', file=sys.stderr)

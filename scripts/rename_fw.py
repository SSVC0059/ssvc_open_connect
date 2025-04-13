"""
    EMS-ESP - https://github.com/emsesp/EMS-ESP
    Copyright 2020-2023  Paul Derbyshire

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. 
"""

import shutil
import re
import os
Import("env")
import hashlib
from cryptography import x509


OUTPUT_DIR = "build{}".format(os.path.sep)

def readFlag(flag):
    buildFlags = env.ParseFlags(env["BUILD_FLAGS"])
    for define in buildFlags.get("CPPDEFINES"):
        if (define == flag or (isinstance(define, list) and define[0] == flag)):
            cleanedFlag = re.sub(r'^"|"$', '', define[1])
            return cleanedFlag
    return None


def calculate_md5(file_path):
    """Calculate MD5 hash for the given file."""
    with open(file_path, "rb") as f:
        return hashlib.md5(f.read()).hexdigest()


def should_copy_file(source, target_md5_path):
    """Check if a file should be copied based on its MD5 hash."""
    if not os.path.isfile(source):
        return False
    new_md5 = calculate_md5(source)
    if os.path.isfile(target_md5_path):
        with open(target_md5_path, "r") as f:
            existing_md5 = f.read().strip()
        return new_md5 != existing_md5
    return True


def bin_copy(source, target, env):
    # get the build info
    app_version = readFlag("APP_VERSION")
    app_name = readFlag("APP_NAME")
    build_target = env.get('PIOENV')

    print("App Version: " + app_version)
    print("App Name: " + app_name)
    print("Build Target: " + build_target)

    # Create target-specific directory
    target_dir = os.path.join(OUTPUT_DIR, build_target)
    if not os.path.isdir(target_dir):
        os.makedirs(target_dir)

    print("target_dir: " + target_dir)

    # Define file paths
    bin_file = os.path.join(target_dir, f"{app_name}_{build_target}_{app_version.replace('.', '-')}.bin")
    md5_file = os.path.join(target_dir, f"{app_name}_{build_target}_{app_version.replace('.', '-')}.md5")
    bootloader_file = os.path.join(target_dir, "bootloader.bin")
    partition_file = os.path.join(target_dir, "partitions.bin")
    merged_frm_file = os.path.join(target_dir, "firmware_merged.bin")
    bootloader_md5 = os.path.join(target_dir, "bootloader.md5")
    partition_md5 = os.path.join(target_dir, "partitions.md5")
    merged_frm_md5 = os.path.join(target_dir, "firmware_merged.md5")
    
    # Remove existing firmware files
    if os.path.isfile(bin_file):
        os.remove(bin_file)

    print("Renaming file to " + bin_file)
    shutil.copy(str(target[0]), bin_file)

    # Calculate MD5 for main firmware.bin
    firmware_hash = calculate_md5(bin_file)
    print("Firmware MD5: " + firmware_hash)
    with open(md5_file, "w") as f:
        f.write(firmware_hash)

    # Copy and update bootloader.bin if MD5 differs
    bootloader_src = os.path.join(env.subst("$BUILD_DIR"), "bootloader.bin")
    if should_copy_file(bootloader_src, bootloader_md5):
        print("Copying bootloader to " + bootloader_file)
        shutil.copy(bootloader_src, bootloader_file)
        bootloader_hash = calculate_md5(bootloader_file)
        print("Bootloader MD5: " + bootloader_hash)
        with open(bootloader_md5, "w") as f:
            f.write(bootloader_hash)
    else:
        print("Bootloader unchanged. Skipping copy.")

    # Copy and update partitions.bin if MD5 differs
    partition_src = os.path.join(env.subst("$BUILD_DIR"), "partitions.bin")
    if should_copy_file(partition_src, partition_md5):
        print("Copying partition table to " + partition_file)
        shutil.copy(partition_src, partition_file)
        partition_hash = calculate_md5(partition_file)
        print("Partition Table MD5: " + partition_hash)
        with open(partition_md5, "w") as f:
            f.write(partition_hash)
    else:
        print("Partition table unchanged. Skipping copy.")

    merged_frm_src = os.path.join(env.subst("$BUILD_DIR"), "firmware_merged.bin")
    if should_copy_file(merged_frm_src, merged_frm_md5):
        print("Copying partition table to " + merged_frm_file)
        shutil.copy(merged_frm_src, merged_frm_file)
        merged_frm_hash = calculate_md5(merged_frm_file)
        print("Partition Table MD5: " + merged_frm_hash)
        with open(merged_frm_md5, "w") as f:
            f.write(merged_frm_hash)
    else:
        print("Partition table unchanged. Skipping copy.")

# Attach actions to build process
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", [bin_copy])
env.AddPostAction("$BUILD_DIR/${PROGNAME}.md5", [bin_copy])

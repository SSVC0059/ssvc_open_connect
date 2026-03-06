import shutil
import os
import hashlib
Import("env")

BASE_OUTPUT_DIR = os.path.join("build", "elf")

def elf_copy(source, target, env):
    # Get the build environment name (e.g., 'esp32dev', 'esp32-s3-devkitc-1')
    # This will be used as the controller type subdirectory.
    controller_type = env['PIOENV']
    
    # Construct the final output directory path: build/elf/<controller_type>
    output_dir = os.path.join(BASE_OUTPUT_DIR, controller_type)

    # check if output directories exist and create if necessary
    # os.makedirs will create all parent directories if they don't exist.
    os.makedirs(output_dir, exist_ok=True)

    with open(str(target[0]),"rb") as f:
        elf_content = f.read()
        sha256_hash = hashlib.sha256(elf_content).hexdigest()
        
    # Create the full path for the new ELF file, named by its hash.
    elf_file_path = os.path.join(output_dir, f"{sha256_hash}.elf")

    print(f"Saving ELF file to {elf_file_path}")

    # Copy the source ELF file to the new destination.
    shutil.copy(str(target[0]), elf_file_path)
        
env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", elf_copy)
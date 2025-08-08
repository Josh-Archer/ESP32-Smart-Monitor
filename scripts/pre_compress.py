try:
    from SCons.Script import Import
    Import("env")  # provided by PlatformIO
except ImportError:
    env = None

import os, gzip, shutil

# Compress static assets before creating filesystem image
def gzip_assets(source, target, env):
    # Build the SPIFFS data folder from web/, gzipping JS/CSS/HTML and copying other files
    print("[INFO] Pre-gzip: Generating data/ from web/...")
    project_dir = env.get("PROJECT_DIR")
    web_dir = os.path.join(project_dir, "web")
    data_dir = os.path.join(project_dir, "data")
    # Clear out old data directory
    if os.path.exists(data_dir):
        shutil.rmtree(data_dir)
    os.makedirs(data_dir, exist_ok=True)
    # Walk web folder and process files
    for root, dirs, files in os.walk(web_dir):
        for fname in files:
            src_path = os.path.join(root, fname)
            rel_path = os.path.relpath(src_path, web_dir)
            dest_dir = os.path.join(data_dir, os.path.dirname(rel_path))
            os.makedirs(dest_dir, exist_ok=True)
            ext = os.path.splitext(fname)[1].lower()
            # Gzip compress JS, CSS, HTML
            if ext in ('.js', '.css', '.html'):
                dest_path = os.path.join(dest_dir, rel_path + '.gz')
                try:
                    with open(src_path, 'rb') as f_in, gzip.open(dest_path, 'wb', compresslevel=9) as f_out:
                        shutil.copyfileobj(f_in, f_out)
                    print(f"[INFO] Compressed {rel_path} -> {rel_path}.gz")
                except Exception as e:
                    print(f"[ERROR] Failed to gzip {rel_path}: {e}")
            else:
                # Copy other files directly (e.g., manifest.json, images)
                dest_path = os.path.join(dest_dir, rel_path)
                try:
                    shutil.copy2(src_path, dest_path)
                    print(f"[INFO] Copied {rel_path}")
                except Exception as e:
                    print(f"[ERROR] Failed to copy {rel_path}: {e}")

# Hook into buildfs (SPIFFS) generation
if env:
    # Hook into SPIFFS build and upload targets to gzip assets and exclude originals
    env.AddPreAction("buildfs", gzip_assets)
    env.AddPreAction("uploadfs", gzip_assets)

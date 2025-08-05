#!/usr/bin/env python3

"""
Post-upload script for ESP32 monitoring
This script runs automatically after a successful upload
"""

import subprocess
import time
import os
import sys
from pathlib import Path

def post_upload_action(source, target, env):
    """PlatformIO post-upload callback function"""
    print("\n" + "="*60)
    print("           POST-UPLOAD MONITORING STARTED")
    print("="*60)
    
    # Get project directory
    project_dir = env.get("PROJECT_DIR", os.getcwd())
    print(f"Project: {project_dir}")
    
    # Wait for device to initialize
    print("Waiting for device to initialize after upload...")
    time.sleep(8)
    
    # Run monitoring script
    # Check if we're on Windows and use PowerShell, otherwise use bash
    if os.name == 'nt':  # Windows
        # Use dedicated PowerShell monitoring script
        ps_monitor = Path(project_dir) / "scripts" / "post_upload_monitor.ps1"
        if ps_monitor.exists():
            print("Running Windows PowerShell monitoring...")
            try:
                result = subprocess.run(
                    ["powershell.exe", "-ExecutionPolicy", "Bypass", "-File", str(ps_monitor)],
                    cwd=project_dir,
                    timeout=45
                )
                if result.returncode == 0:
                    print("✓ Post-upload monitoring completed")
                else:
                    print(f"⚠ Monitoring completed with code {result.returncode}")
            except subprocess.TimeoutExpired:
                print("⚠ Monitoring timed out")
            except Exception as e:
                print(f"⚠ Error running monitoring: {e}")
        else:
            print("⚠ PowerShell monitoring script not found")
            print("💡 Manual steps:")
            print("   . .\\scripts\\scripts.ps1")
            print("   telnet-monitor")
    else:
        # Unix/Linux - use bash script
        monitor_script = Path(project_dir) / "monitor_esp32.sh"
        if monitor_script.exists():
            print("Running device monitoring script...")
            try:
                result = subprocess.run(
                    [str(monitor_script)],
                    cwd=project_dir,
                    timeout=60
                )
                if result.returncode == 0:
                    print("✓ Monitoring completed successfully")
                else:
                    print(f"⚠ Monitoring script exited with code {result.returncode}")
            except subprocess.TimeoutExpired:
                print("⚠ Monitoring script timed out")
            except Exception as e:
                print(f"⚠ Error running monitoring script: {e}")
        else:
            print("⚠ Monitoring script not found, skipping...")
    
    print("\n" + "="*60)
    print("           POST-UPLOAD COMPLETE")
    print("="*60)
    print("Quick commands:")
    print("  ./monitor_esp32.sh          # Run monitor again")
    print("  ./monitor_esp32.sh --telnet # Connect to telnet")
    print("  ./reboot_esp32.sh           # Remote reboot")
    print("="*60 + "\n")

# For standalone execution (testing)
def standalone_execution():
    """Function to test the script standalone"""
    print("Running post-upload script in standalone mode...")
    
    # Mock environment for testing
    class MockEnv:
        def get(self, key, default):
            return os.environ.get(key, default)
    
    mock_env = MockEnv()
    post_upload_action(None, None, mock_env)

# PlatformIO integration
try:
    # This will only work when called from PlatformIO
    Import("env")
    env.AddPostAction("upload", post_upload_action)
    print("Post-upload monitoring script registered!")
except NameError:
    # Running standalone for testing
    if __name__ == "__main__":
        standalone_execution()

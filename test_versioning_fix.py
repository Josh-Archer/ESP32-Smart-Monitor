#!/usr/bin/env python3
"""
Test script to validate the versioning fix
==========================================

This script demonstrates that the versioning issue has been resolved
and shows how the enhanced version manager behaves in different scenarios.
"""

import sys
import os

# Add the project root to the path
project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, project_root)

import scripts.version_manager as vm

def test_scenario(name, commits, files, expected):
    """Test a specific scenario and verify the expected result."""
    print(f"\n📋 Testing: {name}")
    print("=" * (len(name) + 11))
    
    manager = vm.VersionManager(dry_run=True)
    result = manager.determine_version_increment(commits, files)
    
    status = "✅ PASS" if result == expected else "❌ FAIL"
    print(f"Expected: {expected}")
    print(f"Got: {result}")
    print(f"Status: {status}")
    
    return result == expected

def main():
    print("🔍 ESP32 Smart Monitor - Versioning Fix Validation")
    print("==================================================")
    
    # Test scenarios
    scenarios = [
        {
            "name": "Infrastructure-only changes",
            "commits": ["Update CI workflow", "Add deployment scripts"],
            "files": [".github/workflows/ci.yml", "scripts/deploy.sh", "k8s/service.yaml"],
            "expected": "none"
        },
        {
            "name": "Source code bug fix",
            "commits": ["Fix memory leak in MQTT client"],
            "files": ["src/mqtt_manager.cpp"],
            "expected": "patch"
        },
        {
            "name": "New feature addition",
            "commits": ["Add DNS monitoring feature"],
            "files": ["src/dns_manager.cpp", "src/dns_manager.h"],
            "expected": "minor"
        },
        {
            "name": "Manual override via commit message",
            "commits": ["Add comprehensive logging [no-version]"],
            "files": ["src/logger.cpp", "src/logger.h"],
            "expected": "none"
        },
        {
            "name": "Large initial setup (original problematic scenario)",
            "commits": ["Automatic version update to v2.6.0"],
            "files": [
                ".github/copilot-instructions.md", ".github/workflows/build-and-tag.yml",
                ".gitignore", ".vscode/arduino.json", ".vscode/extensions.json",
                "CHANGES.md", "README.md", "credentials.template.cpp",
                "docs/VERSION_TAGGING.md", "features.md", "k8s/Dockerfile",
                "k8s/deployment.yaml", "scripts/version_manager.py", "scripts/deploy_web.sh",
                "src/config.cpp", "src/config.h", "src/main.cpp", "src/mqtt_manager.cpp",
                "web/app.js", "web/index.html"
            ] * 4,  # 80 files total to trigger initial setup detection
            "expected": "none"
        }
    ]
    
    # Run tests
    passed = 0
    total = len(scenarios)
    
    for scenario in scenarios:
        if test_scenario(scenario["name"], scenario["commits"], scenario["files"], scenario["expected"]):
            passed += 1
    
    print(f"\n🎯 Summary")
    print("=" * 9)
    print(f"Tests passed: {passed}/{total}")
    print(f"Tests failed: {total - passed}/{total}")
    
    if passed == total:
        print("\n🎉 All tests PASSED! The versioning fix is working correctly.")
        return 0
    else:
        print("\n💥 Some tests FAILED! Please check the version manager logic.")
        return 1

if __name__ == "__main__":
    sys.exit(main())
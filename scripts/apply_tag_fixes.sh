#!/bin/bash
# Apply Tag Fixes to Remote Repository
#
# This script applies the local tag fixes to the remote repository.
# Run this AFTER the PR is merged to update the remote tags.
#
# What this does:
# 1. Deletes the old mismatched v2.7.0 tag from remote
# 2. Pushes the new v2.6.0 tag (points to commit with version 2.6.0)
# 3. Pushes the corrected v2.7.0 tag (points to commit with version 2.7.0)

set -e  # Exit on any error

echo "=== Applying Tag Fixes to Remote Repository ==="
echo ""

# Verify we're in the right repository
if [ ! -f "src/config.cpp" ]; then
    echo "❌ Error: Not in ESP32-Smart-Monitor repository root"
    echo "Please run this script from the repository root directory"
    exit 1
fi

# Verify local tags are correct before pushing
echo "Verifying local tag alignment before pushing..."
./scripts/fix_version_tags.sh

if [ $? -ne 0 ]; then
    echo "❌ Local tag verification failed. Please fix local tags first."
    exit 1
fi

echo ""
echo "Local tags verified. Proceeding with remote update..."
echo ""

# Step 1: Delete the old mismatched v2.7.0 tag from remote
echo "1. Deleting old mismatched v2.7.0 tag from remote..."
if git push origin :refs/tags/v2.7.0; then
    echo "   ✅ Old v2.7.0 tag deleted from remote"
else
    echo "   ⚠️  Failed to delete v2.7.0 tag (might not exist on remote)"
fi

echo ""

# Step 2: Push the new v2.6.0 tag
echo "2. Pushing new v2.6.0 tag..."
if git push origin v2.6.0; then
    echo "   ✅ v2.6.0 tag pushed to remote"
else
    echo "   ❌ Failed to push v2.6.0 tag"
    exit 1
fi

echo ""

# Step 3: Push the corrected v2.7.0 tag
echo "3. Pushing corrected v2.7.0 tag..."
if git push origin v2.7.0; then
    echo "   ✅ v2.7.0 tag pushed to remote"
else
    echo "   ❌ Failed to push v2.7.0 tag"
    exit 1
fi

echo ""
echo "=== Remote Tag Update Complete ==="
echo ""
echo "✅ All tag fixes have been applied to the remote repository!"
echo ""
echo "Final tag alignment:"
echo "  - v2.6.0 points to commit with firmwareVersion = \"2.6.0\""
echo "  - v2.7.0 points to commit with firmwareVersion = \"2.7.0\""
echo ""
echo "The Git tag version mismatch issue has been resolved."
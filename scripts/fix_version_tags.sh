#!/bin/bash
# Fix Version Tag Mismatch
# 
# This script fixes the Git tag version mismatch issue where v2.7.0 tag
# pointed to code with version 2.6.0 instead of 2.7.0.
#
# Issue: Git tag v2.7.0 pointed to commit with firmwareVersion = "2.6.0"
# Fix: 
#  1. Deleted mismatched v2.7.0 tag
#  2. Created v2.6.0 tag pointing to the commit that actually has version 2.6.0 
#  3. Created new v2.7.0 tag pointing to main branch with version 2.7.0
#
# To apply this fix to remote repository, run:
#   git push origin :refs/tags/v2.7.0  # Delete old v2.7.0 tag from remote
#   git push origin v2.6.0             # Push new v2.6.0 tag
#   git push origin v2.7.0             # Push corrected v2.7.0 tag

echo "=== Git Tag Version Mismatch Fix ==="
echo ""
echo "Verifying current tag alignment:"

# Check each version tag matches its code
for tag in v2.6.0 v2.7.0; do
    echo -n "  $tag -> "
    version=$(git show $tag:src/config.cpp 2>/dev/null | grep 'firmwareVersion = ' | cut -d'"' -f2)
    expected=${tag#v}  # Remove 'v' prefix
    
    if [ "$version" = "$expected" ]; then
        echo "version $version ✅ MATCH"
    else
        echo "version $version ❌ MISMATCH (expected $expected)"
    fi
done

echo ""
echo "Tag alignment verification complete."

# Show the commits that each tag points to
echo ""
echo "Tag commit mapping:"
git log --oneline --decorate -10 | grep -E "(v2\.[67]\.0|main)" || true
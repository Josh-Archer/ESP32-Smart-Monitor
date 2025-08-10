# Git Tag Version Mismatch Fix

## Issue Description

The Git tag `v2.7.0` was pointing to a commit that contained `firmwareVersion = "2.6.0"` in `src/config.cpp`, while the main branch contained `firmwareVersion = "2.7.0"` without a corresponding git tag.

## Root Cause

The automated build and tagging workflow in `.github/workflows/build-and-tag.yml` created the `v2.7.0` tag before the version in `config.cpp` was updated to match. This resulted in:

- Tag `v2.7.0` → commit `a5b135d` → `firmwareVersion = "2.6.0"` ❌
- Main branch → commit `8cac440` → `firmwareVersion = "2.7.0"` (no tag) ❌

## Solution Applied

1. **Deleted mismatched v2.7.0 tag**: Removed the tag that incorrectly pointed to version 2.6.0 code
2. **Created v2.6.0 tag**: Created proper tag pointing to commit `a5b135d` which contains version 2.6.0
3. **Created corrected v2.7.0 tag**: Created new tag pointing to commit `8cac440` (main) which contains version 2.7.0

## Result

Now the tags properly align with their code versions:

- Tag `v2.6.0` → commit `a5b135d` → `firmwareVersion = "2.6.0"` ✅
- Tag `v2.7.0` → commit `8cac440` → `firmwareVersion = "2.7.0"` ✅

## Verification

Run the verification script to confirm alignment:

```bash
./scripts/fix_version_tags.sh
```

Expected output:
```
=== Git Tag Version Mismatch Fix ===

Verifying current tag alignment:
  v2.6.0 -> version 2.6.0 ✅ MATCH
  v2.7.0 -> version 2.7.0 ✅ MATCH

Tag alignment verification complete.
```

## Remote Update Commands

To apply these tag changes to the remote repository:

```bash
# Delete the old mismatched v2.7.0 tag from remote
git push origin :refs/tags/v2.7.0

# Push the new properly aligned tags
git push origin v2.6.0
git push origin v2.7.0
```

## Prevention

The existing `scripts/version_manager.py` already has logic to prevent this issue by analyzing commits and automatically incrementing versions. The workflow should ensure that:

1. Version increments happen BEFORE tagging
2. Tags are created AFTER the config.cpp is updated
3. Tag names match the version in config.cpp

## Apply to Remote

After merging this PR, run the script to apply tag fixes to the remote repository:

```bash
./scripts/apply_tag_fixes.sh
```

This will:
1. Delete the old mismatched v2.7.0 tag from remote
2. Push the corrected v2.6.0 and v2.7.0 tags

## Files Changed

- `.github/workflows/build-and-tag.yml` - Enhanced workflow to prevent future tag mismatches
- `scripts/fix_version_tags.sh` - Verification script for tag alignment
- `scripts/apply_tag_fixes.sh` - Script to apply fixes to remote repository
- `VERSION_TAG_FIX.md` - This documentation

This fix resolves issue #25.
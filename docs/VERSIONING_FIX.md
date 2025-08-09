# Versioning Fix Documentation

## Issue Description
An unwanted v2.6.0 tag was created automatically when the repository was initially set up with infrastructure files. The version manager incorrectly detected source code changes and incremented the version, when it should have recognized this as an infrastructure-only setup.

## Root Cause
The automatic version manager detected changes to source files (`src/*.cpp`, `src/*.h`) in the initial setup commit and correctly followed its logic to increment the version. However, it failed to recognize that this was an initial repository setup scenario with primarily infrastructure files.

## Fix Implementation

### 1. Version Revert
- Reverted firmware version in `src/config.cpp` from "2.6.0" back to "2.5.0"
- This ensures continuity with the previous legitimate version

### 2. Enhanced Infrastructure Detection
Added several improvements to the version manager (`scripts/version_manager.py`):

#### Expanded Infrastructure Patterns
```python
self.infrastructure_patterns = [
    r'\b(ci|workflow|github|action)\b',
    r'\b(docker|k8s|kubernetes|deploy)\b',
    r'\b(script|automation|build)\b',
    r'\.github/',
    r'k8s/',
    r'scripts/',
    r'platformio\.ini',
    r'\.vscode/',        # Added
    r'docs/',            # Added
    r'data/',            # Added
    r'web/',             # Added
    r'credentials\.template\.', # Added
    r'features\.md',     # Added
    r'CHANGES\.md',      # Added
    r'test_build_configs\.sh', # Added
    r'upload_and_monitor\.sh', # Added
    r'monitor_esp32\.sh', # Added
    r'reboot_esp32\.sh', # Added
]
```

#### Initial Setup Detection
- Added logic to detect large-scale file additions (>30 files)
- If >70% of changed files are infrastructure/documentation, treat as infrastructure-only
- Prevents version increments for initial repository setup scenarios

#### Manual Override Support
Added support for manual overrides in commit messages and PR labels:
- Commit message keywords: `[no-version]`, `[infrastructure]`, `[infrastructure-only]`
- PR labels: `infrastructure`, `no-version`

### 3. Testing
The fix has been tested with the original problematic file set (70 files) and correctly identifies it as infrastructure-only, returning 'none' for version increment.

## Prevention
To prevent similar issues in the future:

1. **Use override keywords** in commit messages when making infrastructure-only changes
2. **Apply appropriate labels** to PRs that shouldn't trigger version increments
3. **Separate infrastructure changes** from source code changes when possible
4. **The enhanced logic** will automatically detect initial setup scenarios

## Future Enhancements
- Consider adding a configuration file to customize infrastructure patterns
- Add support for `.versionignore` file to explicitly exclude certain file patterns
- Implement dry-run mode for testing version increment logic before applying
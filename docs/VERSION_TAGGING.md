# Smart Version Tagging System

## Overview

The ESP32 Smart Monitor now features an intelligent version tagging system that automatically analyzes code changes and determines appropriate semantic version increments. This system integrates with GitHub workflows to provide automated version management while respecting manual overrides.

## Features

### 🤖 Automatic Version Analysis
- **Semantic Analysis**: Analyzes commit messages, file changes, and PR content
- **Change Classification**: Distinguishes between major, minor, patch, and infrastructure-only changes
- **GitHub Integration**: Leverages GitHub API for PR and issue label analysis
- **Smart Infrastructure Detection**: Ignores CI/CD, documentation, and deployment changes

### 🏷️ Semantic Versioning Rules

| Change Type | Version Increment | Examples |
|-------------|------------------|----------|
| **Major (X.0.0)** | Breaking changes, large refactors | API changes, major feature overhauls, multiple new modules |
| **Minor (x.Y.0)** | New features, enhancements | New functionality, Home Assistant integration, web interface additions |
| **Patch (x.y.Z)** | Bug fixes, small improvements | Bug fixes, documentation updates, small tweaks |
| **None** | Infrastructure only | CI/CD changes, Docker updates, build script modifications |

### 🔧 Manual Override Options

1. **Issue Labels**: Add `major`, `minor`, or `patch` labels to linked issues
2. **Force Version**: Use `--force-version X.Y.Z` for specific versions
3. **PR Labels**: Labels on PRs take precedence over automatic detection

## Usage

### Local Development

#### Preview Version Changes
```bash
# See what version increment would be applied
./scripts/preview_version.sh

# Or using Python directly
python3 scripts/version_manager.py --dry-run
```

#### Apply Version Updates
```bash
# Apply automatic version increment
python3 scripts/version_manager.py

# Force a specific version
python3 scripts/version_manager.py --force-version 2.5.0

# Analyze a specific PR
python3 scripts/version_manager.py --analyze-pr 123
```

#### PowerShell (Windows)
```powershell
# Load scripts
. .\scripts\scripts.ps1

# Preview changes
preview-version

# Apply updates
update-version

# Force version
force-version "2.5.0"
```

### GitHub Workflow Integration

The system automatically integrates with GitHub Actions:

#### For Pull Requests
- **Build Report**: Shows memory usage and build status
- **Version Analysis**: Analyzes PR content and suggests version increment
- **Override Instructions**: Provides guidance on using labels to override detection

#### For Main Branch Pushes
- **Smart Analysis**: Determines if version increment is needed
- **File Updates**: Automatically updates `src/config.cpp` and `README.md`
- **Git Tagging**: Creates version tags only when changes warrant them
- **Commit Updates**: Commits version changes back to the repository

## File Updates

### src/config.cpp
```cpp
// Before
const char* firmwareVersion = "2.4.1";

// After automatic update
const char* firmwareVersion = "2.5.0";
```

### README.md
The system automatically updates the "What's New" section:
```markdown
## What's New (v2.5.0)

### 2.5.0

- **Automated Version:** Version automatically incremented using smart semantic versioning
- **Release Date:** 2024-01-15
- **Change Summary:** Automated release with version tagging improvements
```

## Configuration

### Environment Variables
- `GITHUB_TOKEN`: Required for GitHub API access and PR analysis
- `GITHUB_OUTPUT`: Used by GitHub Actions for workflow communication

### Patterns and Detection

The system uses configurable regex patterns to detect change types:

#### Major Change Patterns
- `\b(breaking|major|significant|overhaul|rewrite|refactor)\b`
- `\b(remove|delete|deprecate)\s+\w+`
- `\b(api|interface)\s+change`
- `\bmajor\s+(feature|update|change)`

#### Minor Change Patterns
- `\b(add|new|feature|enhancement|implement)\b`
- `\b(support|integration|module)\b`
- `\b(improve|enhance|upgrade)\s+\w+`
- `\bminor\s+(feature|update|change)`

#### Patch Change Patterns
- `\b(fix|bug|issue|patch|correct)\b`
- `\b(update|adjust|tweak|small)\b`
- `\b(documentation|readme|comment)\b`
- `\b(style|format|cleanup)\b`

#### Infrastructure Patterns (Ignored)
- `\b(ci|workflow|github|action)\b`
- `\b(docker|k8s|kubernetes|deploy)\b`
- `\b(script|automation|build)\b`
- Files: `\.github/`, `k8s/`, `scripts/`, `platformio\.ini`

## Workflow Examples

### Example 1: Feature Addition
```
Commits: "Add MQTT Home Assistant integration"
Files: src/mqtt_manager.cpp, src/mqtt_manager.h, src/main.cpp
Result: Minor version increment (2.4.1 → 2.5.0)
```

### Example 2: Bug Fix
```
Commits: "Fix WiFi reconnection issue", "Correct memory leak"
Files: src/main.cpp, src/system_utils.cpp
Result: Patch version increment (2.4.1 → 2.4.2)
```

### Example 3: Infrastructure Only
```
Commits: "Update Docker configuration", "Fix CI workflow"
Files: k8s/deployment.yaml, .github/workflows/build.yml
Result: No version increment
```

### Example 4: Breaking Change
```
Commits: "Refactor configuration system", "Remove legacy API endpoints"
Files: Multiple src/ files
Result: Major version increment (2.4.1 → 3.0.0)
```

## Troubleshooting

### Common Issues

#### No Version Increment Applied
- **Cause**: Only infrastructure files changed
- **Solution**: This is expected behavior - no version bump needed

#### Wrong Version Increment Detected
- **Solution 1**: Add appropriate label (`major`, `minor`, `patch`) to PR or linked issue
- **Solution 2**: Use `--force-version X.Y.Z` locally

#### GitHub API Access Issues
- **Cause**: Missing or invalid `GITHUB_TOKEN`
- **Solution**: Ensure token has appropriate repository permissions

#### File Update Failures
- **Cause**: File permissions or Git conflicts
- **Solution**: Check file permissions and resolve any merge conflicts

### Debug Commands

```bash
# Verbose analysis
python3 scripts/version_manager.py --dry-run --analyze-pr 123

# Check current state
./scripts/preview_version.sh

# Manual version inspection
grep "firmwareVersion" src/config.cpp
git tag --list 'v*.*.*' | sort -V | tail -5
```

## Integration with Existing Workflow

The smart version tagging system seamlessly integrates with existing development practices:

1. **Preserves Manual Control**: Developers can still manually set versions when needed
2. **Respects Git History**: Works with existing tag structure and commit patterns
3. **Non-Breaking**: Falls back gracefully when automatic detection is unclear
4. **CI/CD Compatible**: Fully integrated with GitHub Actions workflow

## Future Enhancements

Potential improvements for the version tagging system:

- **AI-Powered Analysis**: Use GitHub Copilot API for more intelligent change classification
- **Custom Pattern Configuration**: Allow repository-specific pattern configuration
- **Release Note Generation**: Automatically generate release notes from commit messages
- **Multi-Repository Support**: Extend to handle monorepo scenarios
- **Integration Testing**: Validate version increments against test results
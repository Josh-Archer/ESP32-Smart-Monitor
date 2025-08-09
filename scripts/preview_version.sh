#!/bin/bash
# Preview Version Changes
# ======================
# This script helps developers preview what version changes would be made
# before committing their changes.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "🔍 ESP32 Smart Monitor - Version Preview"
echo "========================================"

cd "$REPO_ROOT"

# Check if we're in a git repository
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo "❌ Error: Not in a git repository"
    exit 1
fi

# Check if there are any changes
if git diff --quiet && git diff --cached --quiet; then
    echo "ℹ️  No changes detected in working directory"
    echo "   This preview shows what would happen if you committed current state"
fi

echo ""
echo "📊 Current Status:"
echo "=================="

# Show current version
CURRENT_VERSION=$(grep -o 'firmwareVersion = "[^"]*"' src/config.cpp | cut -d'"' -f2)
echo "📋 Current version in config.cpp: $CURRENT_VERSION"

# Show latest tagged version
LATEST_TAG=$(git tag --list 'v*.*.*' | sort -V | tail -1)
if [ -n "$LATEST_TAG" ]; then
    echo "🏷️  Latest tagged version: $LATEST_TAG"
else
    echo "🏷️  No version tags found"
fi

# Show uncommitted changes
if ! git diff --quiet || ! git diff --cached --quiet; then
    echo ""
    echo "📝 Uncommitted Changes:"
    echo "======================"
    
    # Show staged files
    STAGED_FILES=$(git diff --cached --name-only)
    if [ -n "$STAGED_FILES" ]; then
        echo "🟢 Staged files:"
        echo "$STAGED_FILES" | sed 's/^/   - /'
    fi
    
    # Show unstaged files
    UNSTAGED_FILES=$(git diff --name-only)
    if [ -n "$UNSTAGED_FILES" ]; then
        echo "🟡 Unstaged files:"
        echo "$UNSTAGED_FILES" | sed 's/^/   - /'
    fi
fi

echo ""
echo "🧮 Version Analysis:"
echo "==================="

# Run version analysis in dry-run mode
if python3 scripts/version_manager.py --dry-run; then
    echo ""
    echo "✅ Version analysis completed successfully"
else
    echo ""
    echo "ℹ️  No version increment suggested (likely infrastructure-only changes)"
fi

echo ""
echo "🛠️  Next Steps:"
echo "==============="
echo "• Review the analysis above"
echo "• If you disagree with the suggested increment:"
echo "  - Use: python3 scripts/version_manager.py --force-version X.Y.Z"
echo "  - Or add issue labels: 'major', 'minor', or 'patch'"
echo "• To apply changes: python3 scripts/version_manager.py"
echo "• To just test: python3 scripts/version_manager.py --dry-run"

echo ""
echo "📚 Semantic Versioning Rules:"
echo "============================="
echo "• 🔴 Major (X.0.0): Breaking changes, major refactors, multiple features"
echo "• 🟡 Minor (x.Y.0): New features, enhancements, single feature additions"  
echo "• 🟢 Patch (x.y.Z): Bug fixes, documentation, small improvements"
echo "• ⚫ None: Infrastructure-only changes (CI, scripts, docs)"
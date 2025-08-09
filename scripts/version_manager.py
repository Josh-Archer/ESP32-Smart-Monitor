#!/usr/bin/env python3
"""
Smart Version Tagging for ESP32 Smart Monitor
============================================

This script analyzes git history and PR content to automatically determine 
semantic version increments and update version information across the project.

Usage:
    python scripts/version_manager.py [options]
    
Options:
    --dry-run          Show what would be changed without making changes
    --force-version    Force a specific version (e.g., --force-version 2.5.0)
    --analyze-pr       Analyze specific PR number for version determination
    --github-token     GitHub token for API access (or use GITHUB_TOKEN env var)
    
Semantic Versioning Rules:
    - Major (x.0.0): Large multi-feature changes, breaking changes
    - Minor (x.y.0): Single or limited scope feature additions
    - Patch (x.y.z): Bug fixes, documentation, small improvements
"""

import os
import sys
import re
import json
import subprocess
import argparse
from typing import Tuple, Optional, List, Dict
from datetime import datetime
import requests


class VersionManager:
    def __init__(self, github_token: Optional[str] = None, dry_run: bool = False):
        self.github_token = github_token or os.getenv('GITHUB_TOKEN')
        self.dry_run = dry_run
        self.repo_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        
        # File paths
        self.config_cpp_path = os.path.join(self.repo_path, 'src', 'config.cpp')
        self.readme_path = os.path.join(self.repo_path, 'README.md')
        
        # Semantic versioning patterns
        self.major_patterns = [
            r'\b(breaking|major|significant|overhaul|rewrite|refactor)\b',
            r'\b(remove|delete|deprecate)\s+\w+',
            r'\b(api|interface)\s+change',
            r'\bmajor\s+(feature|update|change)',
        ]
        
        self.minor_patterns = [
            r'\b(add|new|feature|enhancement|implement)\b',
            r'\b(support|integration|module)\b',
            r'\b(improve|enhance|upgrade)\s+\w+',
            r'\bminor\s+(feature|update|change)',
        ]
        
        self.patch_patterns = [
            r'\b(fix|bug|issue|patch|correct)\b',
            r'\b(update|adjust|tweak|small)\b',
            r'\b(documentation|readme|comment)\b',
            r'\b(style|format|cleanup)\b',
        ]
        
        self.infrastructure_patterns = [
            r'\b(ci|workflow|github|action)\b',
            r'\b(docker|k8s|kubernetes|deploy)\b',
            r'\b(script|automation|build)\b',
            r'\.github/',
            r'k8s/',
            r'scripts/',
            r'platformio\.ini',
        ]

    def get_latest_version_tag(self) -> str:
        """Get the latest semantic version tag from git."""
        try:
            # Get all tags, filter for version tags, and sort
            result = subprocess.run(
                ['git', 'tag', '--list', 'v*.*.*'], 
                capture_output=True, text=True, cwd=self.repo_path
            )
            
            if result.returncode != 0:
                print("Warning: Could not get git tags")
                return "0.0.0"
            
            tags = result.stdout.strip().split('\n')
            version_tags = []
            
            for tag in tags:
                if tag.startswith('v'):
                    version = tag[1:]  # Remove 'v' prefix
                    if re.match(r'^\d+\.\d+\.\d+$', version):
                        version_tags.append(version)
            
            if not version_tags:
                return "0.0.0"
            
            # Sort versions naturally
            version_tags.sort(key=lambda v: [int(x) for x in v.split('.')])
            return version_tags[-1]
        
        except Exception as e:
            print(f"Error getting latest version: {e}")
            return "0.0.0"

    def get_current_version_from_config(self) -> str:
        """Extract current version from src/config.cpp."""
        try:
            with open(self.config_cpp_path, 'r') as f:
                content = f.read()
            
            match = re.search(r'const char\* firmwareVersion = "([^"]+)";', content)
            if match:
                return match.group(1)
            
            print("Warning: Could not find version in config.cpp")
            return "0.0.0"
        
        except Exception as e:
            print(f"Error reading config.cpp: {e}")
            return "0.0.0"

    def analyze_commits_since_version(self, since_version: str) -> List[str]:
        """Get commit messages since the specified version tag."""
        try:
            # Get commits since the version tag
            tag_name = f"v{since_version}" if since_version != "0.0.0" else ""
            
            if tag_name:
                cmd = ['git', 'log', f'{tag_name}..HEAD', '--pretty=format:%s']
            else:
                cmd = ['git', 'log', '--pretty=format:%s']
            
            result = subprocess.run(cmd, capture_output=True, text=True, cwd=self.repo_path)
            
            if result.returncode != 0:
                return []
            
            commits = result.stdout.strip().split('\n')
            return [c for c in commits if c.strip()]
        
        except Exception as e:
            print(f"Error analyzing commits: {e}")
            return []

    def analyze_changed_files_since_version(self, since_version: str) -> List[str]:
        """Get list of files changed since the specified version tag."""
        try:
            tag_name = f"v{since_version}" if since_version != "0.0.0" else ""
            
            if tag_name:
                cmd = ['git', 'diff', '--name-only', f'{tag_name}..HEAD']
            else:
                cmd = ['git', 'diff', '--name-only', '--cached']
                if subprocess.run(cmd, capture_output=True).returncode != 0:
                    cmd = ['git', 'ls-files']
            
            result = subprocess.run(cmd, capture_output=True, text=True, cwd=self.repo_path)
            
            if result.returncode != 0:
                return []
            
            files = result.stdout.strip().split('\n')
            return [f for f in files if f.strip()]
        
        except Exception as e:
            print(f"Error analyzing changed files: {e}")
            return []

    def determine_version_increment(self, commits: List[str], changed_files: List[str], 
                                   pr_data: Optional[Dict] = None) -> str:
        """
        Determine version increment type based on commits, files, and PR data.
        Returns: 'major', 'minor', 'patch', or 'none'
        """
        
        # Check if PR has version override labels
        if pr_data and 'labels' in pr_data:
            for label in pr_data['labels']:
                label_name = label['name'].lower()
                if 'major' in label_name or 'breaking' in label_name:
                    return 'major'
                elif 'minor' in label_name or 'feature' in label_name:
                    return 'minor'
                elif 'patch' in label_name or 'bugfix' in label_name:
                    return 'patch'
        
        # Check if changes are infrastructure-only
        infrastructure_only = True
        source_files = []
        
        for file in changed_files:
            is_infrastructure = any(re.search(pattern, file, re.IGNORECASE) 
                                  for pattern in self.infrastructure_patterns)
            if not is_infrastructure and file not in ['.gitignore', 'README.md']:
                infrastructure_only = False
                source_files.append(file)
        
        if infrastructure_only:
            print("Only infrastructure files changed - no version increment needed")
            return 'none'
        
        # Analyze commit messages
        all_text = ' '.join(commits).lower()
        if pr_data:
            all_text += ' ' + pr_data.get('title', '').lower()
            all_text += ' ' + pr_data.get('body', '').lower()
        
        # Check for major changes
        major_score = sum(1 for pattern in self.major_patterns 
                         if re.search(pattern, all_text, re.IGNORECASE))
        
        # Check for minor changes  
        minor_score = sum(1 for pattern in self.minor_patterns 
                         if re.search(pattern, all_text, re.IGNORECASE))
        
        # Check for patch changes
        patch_score = sum(1 for pattern in self.patch_patterns 
                         if re.search(pattern, all_text, re.IGNORECASE))
        
        # Additional heuristics based on file changes
        if len(source_files) > 5:  # Many files changed
            major_score += 1
        elif len(source_files) > 2:  # Several files changed
            minor_score += 1
        
        # Look for new files (likely features)
        new_files = [f for f in source_files if f.endswith(('.cpp', '.h'))]
        if len(new_files) > 1:
            minor_score += 1
        elif len(new_files) == 1:
            minor_score += 0.5
        
        print(f"Version increment analysis:")
        print(f"  Major score: {major_score}")
        print(f"  Minor score: {minor_score}")
        print(f"  Patch score: {patch_score}")
        print(f"  Source files changed: {len(source_files)}")
        
        # Determine increment type
        if major_score >= 2 or (major_score >= 1 and len(source_files) > 3):
            return 'major'
        elif minor_score >= 2 or (minor_score >= 1 and patch_score < minor_score):
            return 'minor'
        elif patch_score >= 1 or len(source_files) > 0:
            return 'patch'
        else:
            return 'none'

    def increment_version(self, current_version: str, increment_type: str) -> str:
        """Increment version according to semantic versioning."""
        if increment_type == 'none':
            return current_version
        
        parts = [int(x) for x in current_version.split('.')]
        major, minor, patch = parts[0], parts[1], parts[2]
        
        if increment_type == 'major':
            major += 1
            minor = 0
            patch = 0
        elif increment_type == 'minor':
            minor += 1
            patch = 0
        elif increment_type == 'patch':
            patch += 1
        
        return f"{major}.{minor}.{patch}"

    def update_config_cpp_version(self, new_version: str) -> bool:
        """Update the version in src/config.cpp."""
        try:
            with open(self.config_cpp_path, 'r') as f:
                content = f.read()
            
            # Replace the firmware version line
            new_content = re.sub(
                r'const char\* firmwareVersion = "[^"]+";',
                f'const char* firmwareVersion = "{new_version}";',
                content
            )
            
            if new_content == content:
                print("Warning: Version line not found in config.cpp")
                return False
            
            if not self.dry_run:
                with open(self.config_cpp_path, 'w') as f:
                    f.write(new_content)
                print(f"Updated config.cpp version to {new_version}")
            else:
                print(f"[DRY RUN] Would update config.cpp version to {new_version}")
            
            return True
        
        except Exception as e:
            print(f"Error updating config.cpp: {e}")
            return False

    def update_readme_version(self, new_version: str) -> bool:
        """Update the version information in README.md."""
        try:
            with open(self.readme_path, 'r') as f:
                content = f.read()
            
            # Create new version section
            today = datetime.now().strftime("%Y-%m-%d")
            new_section = f"""## What's New (v{new_version})

### {new_version}

- **Automated Version:** Version automatically incremented using smart semantic versioning
- **Release Date:** {today}
- **Change Summary:** Automated release with version tagging improvements

---"""
            
            # Replace the "What's New" section
            pattern = r'## What\'s New \(v[^)]+\)'
            if re.search(pattern, content):
                new_content = re.sub(pattern, f"## What's New (v{new_version})", content, count=1)
            else:
                # Insert after the title and description
                insert_pos = content.find('\n## Features')
                if insert_pos == -1:
                    insert_pos = content.find('\n## Configuration')
                if insert_pos == -1:
                    insert_pos = len(content)
                
                new_content = content[:insert_pos] + '\n\n' + new_section + '\n' + content[insert_pos:]
            
            if not self.dry_run:
                with open(self.readme_path, 'w') as f:
                    f.write(new_content)
                print(f"Updated README.md version to {new_version}")
            else:
                print(f"[DRY RUN] Would update README.md version to {new_version}")
            
            return True
        
        except Exception as e:
            print(f"Error updating README.md: {e}")
            return False

    def get_pr_data(self, pr_number: int) -> Optional[Dict]:
        """Get PR data from GitHub API."""
        if not self.github_token:
            print("No GitHub token available for PR analysis")
            return None
        
        try:
            # Extract repository info from git remote
            result = subprocess.run(
                ['git', 'remote', 'get-url', 'origin'], 
                capture_output=True, text=True, cwd=self.repo_path
            )
            
            if result.returncode != 0:
                return None
            
            remote_url = result.stdout.strip()
            
            # Parse repository from URL
            match = re.search(r'github\.com[:/]([^/]+)/([^/\.]+)', remote_url)
            if not match:
                return None
            
            owner, repo = match.groups()
            
            # Get PR data
            url = f"https://api.github.com/repos/{owner}/{repo}/pulls/{pr_number}"
            headers = {'Authorization': f'token {self.github_token}'}
            
            response = requests.get(url, headers=headers)
            if response.status_code == 200:
                return response.json()
            else:
                print(f"Failed to get PR data: {response.status_code}")
                return None
        
        except Exception as e:
            print(f"Error getting PR data: {e}")
            return None

    def run_version_update(self, force_version: Optional[str] = None, 
                          pr_number: Optional[int] = None) -> Tuple[str, str]:
        """
        Main method to analyze and update version.
        Returns: (old_version, new_version)
        """
        
        # Get current state
        latest_tag_version = self.get_latest_version_tag()
        current_config_version = self.get_current_version_from_config()
        
        print(f"Latest tagged version: v{latest_tag_version}")
        print(f"Current config version: {current_config_version}")
        
        # Use the higher of the two as the base
        base_version = max(latest_tag_version, current_config_version, 
                          key=lambda v: [int(x) for x in v.split('.')])
        
        if force_version:
            new_version = force_version
            print(f"Using forced version: {new_version}")
        else:
            # Analyze changes
            commits = self.analyze_commits_since_version(latest_tag_version)
            changed_files = self.analyze_changed_files_since_version(latest_tag_version)
            pr_data = self.get_pr_data(pr_number) if pr_number else None
            
            print(f"Analyzing {len(commits)} commits since v{latest_tag_version}")
            print(f"Files changed: {len(changed_files)}")
            
            if commits:
                print("\nRecent commits:")
                for commit in commits[:5]:  # Show first 5 commits
                    print(f"  - {commit}")
                if len(commits) > 5:
                    print(f"  ... and {len(commits) - 5} more")
            
            # Determine version increment
            increment_type = self.determine_version_increment(commits, changed_files, pr_data)
            new_version = self.increment_version(base_version, increment_type)
            
            print(f"\nVersion increment type: {increment_type}")
            
        if new_version == base_version and not force_version:
            print("No version increment needed")
            return base_version, base_version
        
        print(f"New version: {new_version}")
        
        # Update files
        success = True
        if not self.update_config_cpp_version(new_version):
            success = False
        
        if not self.update_readme_version(new_version):
            success = False
        
        if success:
            print(f"\n✅ Successfully updated version from {base_version} to {new_version}")
        else:
            print(f"\n❌ Errors occurred during version update")
        
        return base_version, new_version


def main():
    parser = argparse.ArgumentParser(description='Smart Version Manager for ESP32 Smart Monitor')
    parser.add_argument('--dry-run', action='store_true', 
                       help='Show what would be changed without making changes')
    parser.add_argument('--force-version', type=str,
                       help='Force a specific version (e.g., 2.5.0)')
    parser.add_argument('--analyze-pr', type=int,
                       help='Analyze specific PR number for version determination')
    parser.add_argument('--github-token', type=str,
                       help='GitHub token for API access')
    
    args = parser.parse_args()
    
    # Create version manager
    version_manager = VersionManager(
        github_token=args.github_token,
        dry_run=args.dry_run
    )
    
    # Run version update
    try:
        old_version, new_version = version_manager.run_version_update(
            force_version=args.force_version,
            pr_number=args.analyze_pr
        )
        
        # Output for GitHub Actions
        if 'GITHUB_OUTPUT' in os.environ:
            with open(os.environ['GITHUB_OUTPUT'], 'a') as f:
                f.write(f"old_version={old_version}\n")
                f.write(f"new_version={new_version}\n")
                f.write(f"version_changed={'true' if new_version != old_version else 'false'}\n")
        
        sys.exit(0 if new_version != old_version or args.force_version else 1)
    
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
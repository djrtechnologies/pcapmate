#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Prepare a fresh local source repository with the documented co-author trailer.
set -eu
cd "$(dirname "$0")/.."
if [ -e .git ]; then
    echo 'This source folder already has a Git repository. No history was changed.'
    git log -1 --format=full
    exit 0
fi
if ! git var GIT_AUTHOR_IDENT >/dev/null 2>&1; then
    echo 'Configure your Git author name/email first; see docs/GITHUB.md.' >&2
    exit 1
fi
git init -b main
git add -- .
# Preserve executable scripts even on filesystems where Git disables filemode detection.
git update-index --chmod=+x build.sh test.sh Tools/prepare-git.sh Tools/smoke.sh
git diff --cached --stat
git commit -F docs/INITIAL_COMMIT_MESSAGE.txt

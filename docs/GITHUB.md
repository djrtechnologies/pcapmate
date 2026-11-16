# Upload pcapmate to GitHub

Repository name: **pcapmate**

About/Description: **A tool to get ATSC 3.0 packet captures from an HDTV Mate.**

Only the contents of this `pcapmate` directory belong in the repository. Do not upload the parent internal-tools project, the source ZIP as the only file, firmware or television recordings.

## Recommended method: Terminal + GitHub CLI

1. Create/sign into your GitHub account in your browser. Install the GitHub CLI if needed:

```sh
brew install gh
```

2. Open Terminal in the **pcapmate source directory** (the folder containing README.md, LICENSE and build.sh). Replace the path below with its actual location:

```sh
cd "/path/to/pcapmate"
```

3. Run local checks if you want to reproduce the macOS validation:

```sh
./build.sh
./test.sh
./Tools/smoke.sh
```

These build outputs stay in ignored `build/`. The Linux status is stated honestly in README/VALIDATION; Linux CI runs after upload.

4. The prepared local source folder already has its first Git commit, including Codex co-authorship. Verify it:

```sh
git log -1 --format=full
git status --short
```

If you extracted the source ZIP instead, it does not contain `.git` history. Create the same attributed initial commit with:

```sh
./Tools/prepare-git.sh
```

That script initializes only this source folder, stages the source according to `.gitignore`, and commits using `docs/INITIAL_COMMIT_MESSAGE.txt`. If a repository already exists, it prints its latest commit and leaves history unchanged.

5. Check the author identity and staged/tracked contents before publishing. If Git reports a missing identity, set your own name/email and rerun the preparation script:

```sh
git config --global user.name "Your Name"
git config --global user.email "YOUR_GITHUB_EMAIL_OR_NOREPLY_ADDRESS"
```

Replace those placeholders. GitHub Settings → Emails gives you a noreply address if you want to keep a personal email private. Your author email must be associated with your account for GitHub to link your contribution. For the supplied already-committed folder, the author uses your existing configured Git identity.

Expected contents include `.github/`, `.gitignore`, `.dockerignore`, Sources/, Tools/, Tests/, upstream/, udev/, LICENSE, LICENSES/, docs/, README.md, ACKNOWLEDGMENTS.md and PROVENANCE.json. Build products, firmware and captures are ignored. Inspect:

```sh
git ls-files
git log -1 --format=%B
```

The commit contains `Co-authored-by: Codex <267193182+codex@users.noreply.github.com>`. GitHub registers the trailer when you push that commit; uploading only loose files through the web editor does not preserve local commit metadata. No contributor invitation or repository access for Codex is required.

6. Authenticate interactively:

```sh
gh auth login --hostname github.com --git-protocol https --web
gh auth setup-git
```

Choose GitHub.com, HTTPS and browser login when prompted. Complete the displayed code/login in your browser. Do not put tokens or passwords in source files or paste them into a chat. GitHub CLI handles credential storage.

7. Create the public repository and upload the initial commit:

```sh
gh repo create pcapmate --public --source=. --remote=origin --description "A tool to get ATSC 3.0 packet captures from an HDTV Mate." --push
gh repo view --web
```

This command performs the public upload when **you** run it. If a `pcapmate` repository already exists, use the existing-repository instructions below instead. Do not create a second README/license using GitHub's new-repository wizard because the source already includes them.

8. On GitHub, check the **About** text. Optionally add topics `atsc3`, `pcap`, `hdtvmate`, `libusb`, `macos`, `linux`.

9. Open **Actions → macOS and Linux** and inspect the first run. Jobs build/test macOS and Linux x86-64/ARM64. Until Linux jobs are green, keep the README's pending-verification note. Even green CI does not prove live Linux USB/RF reception; that still requires a Mate connected to Linux.

10. After a successful source release, you may add a tag/release. Start source-only; GitHub automatically provides source archives. Do not attach firmware or broadcast captures. Prebuilt binaries need matching source, dependency notices and an explicit runtime/dependency packaging plan.

## Existing empty repository / plain Git alternative

If you prefer creating the public repository on https://github.com/new first, name it `pcapmate`, enter the exact About text, choose Public and leave Add README/.gitignore/license off. Then, after committing locally and authenticating with GitHub CLI or your configured Git credential helper:

```sh
git remote add origin https://github.com/YOUR-USERNAME/pcapmate.git
git push -u origin main
```

Replace YOUR-USERNAME with the account that owns the repository. If `origin` is already configured, inspect it with `git remote -v`; use `git remote set-url origin ...` only if it points to the wrong repository. GitHub does not accept your account password as an HTTPS Git password; use the configured CLI/helper authentication.

## Later updates

From this same source repository:

```sh
git add .
git diff --cached --stat
git commit -m "Describe your change"
git push
```

No repository or upload is created automatically by preparing this source folder. These are your explicit publishing steps.

Primary references: https://cli.github.com/manual/gh_repo_create , https://cli.github.com/manual/gh_auth_login , https://docs.github.com/en/get-started/importing-your-projects-to-github/importing-source-code-to-github/adding-locally-hosted-code-to-github .

# Dragon Detector: rules for coding agents

Codex, Claude and any other agent working in this repository follow this file.
`CLAUDE.md` only points here.

## Standing rules from the owner (November 17, 2026; GitHub releases added November 22, 2026)

These replace any older instruction in a local `CLAUDE.md`, a notes file or a
chat history that says to push, tag or publish source.

- **Agents do not push or publish.** Never run `git push`, never open a pull
  request, never create a tag or release on a host, and never upload source,
  bundles or archives anywhere unless the owner asks for that exact step in
  the session. Commits stay in the local repository. This covers GitHub, Gitea
  and every other remote.
- **Releases ship through the web flasher and GitHub Releases.** The web
  flasher is the Cloudflare Pages project `valleytech-dragon-detector`,
  https://valleytech-dragon-detector.pages.dev/; the steps are under
  [Release a web flasher update](#release-a-web-flasher-update). GitHub
  Releases come from `.github/workflows/build.yml`: when the owner pushes a new
  `DRAGON_VERSION` to `main` and all 13 stable boards build, its `release` job
  creates release `v<version>` with `docs/RELEASE-<version>.md` as the notes and
  each board's ZIP plus `SHA256SUMS.txt`. It never replaces an existing
  release, so a version number is published once. The Guition beta stays a
  separate workflow artifact.
- **Say nothing public about it.** Do not add a notice to the README, the docs
  or the website about where the source lives, do not archive or rename the
  repository, and leave existing links alone unless the owner asks.
- **The website publishes `README.md`, `CREDITS.md`, `SECURITY.md` and every
  `docs/**/*.md` file as guide pages.** Internal notes, release procedures and
  anything about these rules go in this file or outside `docs/`, never there.
- **Every feature reaches every board profile** unless the owner says
  otherwise. The 14 PlatformIO environments: `spotpear`, `guition_185_beta`,
  `waveshare` (AMOLED 1.75C), `stopwatch`, `elecrow_21`, `waveshare_175`,
  `waveshare_175_gps`, `waveshare_185b`, `waveshare_143c`, `xiao_round`,
  `waveshare_lite`, `lilygo_175`, `m5dial`, `watcher`. Build all 14 before
  calling a change done.
- **The live site is production.** Agents build, check and prepare an update;
  only the owner starts the upload, from his own signed-in Cloudflare account.
- **Serial ports.** Open a port only after it answers `badge info` with a
  `BADGE_INFO` line. On the owner's PC the AMOLED 1.75C test board is usually
  COM13; COM12 is a different device and must never be opened.

## Public theme presentation

The owner set this showcase preference on October 20, 2026:

- Use Valleytech, BWM, and the Dragon Ball radar for the main README and flasher theme previews.
- Use only TOOOL and TOOOL Dark for the public locksport showcase.
- A request to add a firmware theme does not request new promotional images, a featured gallery, or replacement of existing public photos. Keep the approved showcase unless the owner explicitly asks to change it.
- Keep firmware theme choices, technical documentation, original artwork, and required credits separate from promotional selection. A showcase edit does not remove a theme from the firmware.

## Release a web flasher update

The site is built from this repository plus the firmware packages built here.
The web flasher does not take files from GitHub Actions. Run every step from the repository
root. On the owner's Windows PC, use PlatformIO's Python:

```powershell
$py = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
```

### 1. Version and notes

1. Set the new version in `firmware/include/dragon_version.h` (`DRAGON_VERSION`).
   Every board package takes its version from here.
2. Change every cache token `?v=<old>` in `web/*.js` and `web/index.html` to
   the new version, so browsers load the new scripts. All of them must match:
   `git grep -c "?v=<old>" -- web` must find nothing afterwards.
3. Move the `CHANGELOG.md` entries under a heading for the new version, write
   `docs/RELEASE-<version>.md` (what changed, how it was checked, what is not
   yet confirmed on hardware), and point the "Latest release" link in
   `README.md` and the release-notes link and version line in
   `docs/HOSTING-STATUS.md` at it.
4. Commit locally. Do not push; the owner pushes to `main`, which also
   publishes the GitHub release for the new version.

### 2. Tests

```powershell
node --test tests/*.test.js
& $py tools/test_flash_budget.py
& $py tools/test_core.py --cxx g++          # and the other tools/test_*.py --cxx g++ host tests
& $py -m pip install pillow ziglang==0.17.1   # once, for the two render checks
& $py tools/render_node_screen.py
& $py tools/render_m5dial_ui.py
```

The C++ host tests need `g++` or `clang++` (`--cxx`). `render_node_screen.py`
also calls `cc`, `c++` and `ar` by those names and needs Pillow;
`render_m5dial_ui.py` compiles with `python -m ziglang`. On Windows the
compilers come from MSYS2 (UCRT64) or WSL. If a check cannot run on this
machine, say so in the hand-off and run it where it can; do not skip it
silently. `tools/validate_release.py` (step 4) also refuses a release whose
`docs/RELEASE-<version>.md` is missing or not linked, or whose script cache
tokens do not all match the version.

### 3. Build every board package

```powershell
& $py tools/build_release.py                              # the 13 stable boards
& $py tools/build_release.py --board guition_185_beta     # the separate Guition beta
```

Each board lands in `web/firmware/<board>/` with its manifests, split images,
ZIP, `SHA256SUMS.txt`, `build-info.json` and `memory-budget.json`. The packager
refuses an image that contains a private workstation path or one that does not
fit its partition. A failure on any board stops the release.

### 4. Build and check the site

```powershell
& $py -m pip install Markdown==3.10.2      # once
& $py tools/validate_release.py
node tools/check_web_manifests.mjs
& $py tools/build_static_site.py --output .build/site    # .build/site must be empty or absent
```

`web/betas/t5-e1/` and `web/guides/T5-E1-AMOLED-175.html` are the separate
Tuya T5-E1 beta. They are not built here, but they must stay in every upload,
because the flasher's board list links to them.

Before uploading, compare `.build/site` with the live site: every file the
release did not mean to change must be byte for byte the same, apart from
CRLF/LF line endings in text files. Expect changes only in `index.html`, the
changed `*.js`, `guides/*.html`, and `firmware/<board>/` for the boards that
were rebuilt.

Then record the checked site and the live site's current state for the upload:

```powershell
& $py tools/deploy_web_flasher.py prepare      # writes .build/deploy-plan.json
```

Rebuilding `.build/site` after this means running `prepare` again; the upload
refuses a site that is not the prepared one.

### 5. Upload (owner only)

The agent saves `Upload Dragon Detector flasher.cmd` in the repository root
with exactly these two lines, and the owner double-clicks it:

```bat
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" "%~dp0tools\deploy_web_flasher.py" upload
pause
```

It checks the site against the plan and the live site, signs in to Cloudflare
in the owner's browser when asked, uploads to the production branch `main` of
`valleytech-dragon-detector`, reads the new files back, and logs everything to
`.build/deploy-log.txt`. The last line says `DEPLOY OK`, `DEPLOY UPLOADED` or
`DEPLOY STOPPED` and why. Wrangler runs from an empty temporary folder without
any `CLOUDFLARE_*`, `CF_*` or `WRANGLER_*` setting, so no `wrangler.toml`,
`.env` or cached account can send the upload elsewhere.

Only if the script cannot be used, the bare command is
`npx --yes wrangler@4.139.0 pages deploy .build/site --project-name valleytech-dragon-detector --branch main`,
run from a folder with no `wrangler.toml`, `wrangler.json`, `.env`,
`.env.local` or `.wrangler/` and with none of those variables set.

### 6. After the upload

Read back `index.html`, `app.js`, `board-profiles.js` and each rebuilt board's
`factory.json` and `update.json` from https://valleytech-dragon-detector.pages.dev/
and compare them with `.build/site`. Then install the update on a real board
through the flasher and check it reports the new version (`badge info`).

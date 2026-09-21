# Uploading and maintaining this GitHub repository

Repository: https://github.com/valleytechsolutions/DragonBall-Z-Radar

## Recommended: GitHub Desktop

1. Install [GitHub Desktop](https://desktop.github.com/) and sign in to the account that can write to the repository.
2. Choose **File → Clone repository**, select DragonBall-Z-Radar, and choose a local folder.
3. Edit the clone. Keep the root README and docs together, and keep each board's source and firmware under its matching `hardware/` folder. If applying a prepared folder, copy its contents into the clone without copying another `.git` directory.
4. Review the Changes list. Exclude device backups, recordings, captures, credentials, caches, and build/upload logs. The included `.gitignore` handles common cases.
5. Enter a clear summary, commit, then choose **Push origin**. For collaborative changes, create a branch and open a pull request instead of editing main directly.
6. Open the repository in a browser and check the README links and images.

## Adding pictures

Put shared pictures in `media/`, or board-specific pictures in that board's existing media/docs folder. Use short filenames with no spaces. Reference them with a path relative to the Markdown file:

```markdown
![Description of the picture](media/my-picture.png)
```

Commit both the Markdown and image. Check that letter casing matches. Use only images you own or have permission to distribute; manufacturer photos in this project stay linked to their original hosts. Do not label generated artwork or renderer captures as photographs of hardware.

## Updating firmware files

1. Build and validate the correct hardware variant using BUILDING.md.
2. Name the merged BIN with its hardware, version and `0x0` address. Keep app-only images separate and label their `0x10000` address.
3. Replace/add only the matching board's release file. Do not publish a raw dump from your personal device.
4. Update FIRMWARE-SHA256SUMS.txt, that board's checksum/manifest if present, the README download link, version and validation notes together.
5. Commit and push. A GitHub Release may also provide convenient downloads: choose **Releases → Draft a new release**, select the validated commit/tag, write release notes, and attach the matching BIN and checksums. Do not claim a GitHub Release exists until published.

## Browser-only uploads

For a small documentation or picture update, use **Add file → Upload files**, choose the files, review the commit message and commit. For the complete source tree, prefer GitHub Desktop/Git: the browser upload has per-file and batch limits and is easy to use incorrectly with large nested vendor libraries. Keep the extracted source tree, not just a ZIP, at the expected repository paths.

## Command-line alternative

```sh
git clone https://github.com/valleytechsolutions/DragonBall-Z-Radar.git
cd DragonBall-Z-Radar
# Edit files, then inspect the changes:
git status
git diff
git add README.md docs hardware media CREDITS.md FIRMWARE-SHA256SUMS.txt
git commit -m "Describe the actual update"
git push origin main
```

Sign in through Git Credential Manager when requested. Never place an access token in the repository or a command saved in documentation. Keep the JustCallMeKoko, CodeHedge and third-party credits and original licenses in future updates.

References: [GitHub's project upload guide](https://docs.github.com/en/get-started/start-your-journey/creating-a-repository-for-your-project-on-github) and [GitHub Desktop documentation](https://docs.github.com/en/desktop).

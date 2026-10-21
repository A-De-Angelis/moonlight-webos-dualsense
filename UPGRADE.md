# Upgrade your existing A-De-Angelis/moonlight-webos-dualsense checkout

This archive builds on the previously successful **2026-10-20 GitHub Actions
build #3**. It includes the unmerged adaptive trigger update plus bounded and stale-safe
main-thread dispatch, controller slot mapping, and a TV read-only diagnosis
script. **The changed version has not been ARM cross-built or TV-tested.**

Extract **this ZIP directly into the repository root**, keeping existing
files not listed here. Before applying, check `git status` for uncommitted work.

## PowerShell

```powershell
# In your local moonlight-webos-dualsense Git checkout:
git pull --ff-only
Expand-Archive "$env:USERPROFILE\Downloads\moonlight-webos-dualsense-v2-update.zip" -DestinationPath . -Force
bash scripts/test.sh  # Optional; requires Bash, e.g. Git Bash or WSL
# Inspect changes; then commit and push
 git add README.md UPGRADE.md docs/controller-connection-modes.md `
  scripts/bootstrap.py scripts/diagnose-webos-dualsense.sh scripts/test.sh `
  tests/test_overlay.py haptics/dualsense_effects.h haptics/dualsense_effects_test.c `
  .github/workflows/build-webos.yml
 git diff --cached --stat
 git commit -m "Complete DualSense adaptive trigger forwarding and safer controller mapping"
 git push origin main
```

GitHub Actions will build an IPK on each push to `main`. Download its IPK
artifact **only after** the new run succeeds. Do not interpret success as
hardware validation.

**Note:** ChatGPT's connected GitHub integration returned GitHub API HTTP 403
when attempting writes to this repository. The archive is supplied because the
remote could not be updated directly, even though the app-level permission
was set to Full Access. Review/re-authorize the GitHub connection's actual
repository installation scopes if you want direct changes in future turns.

## Hardware checks

With the DualSense paired to the TV over Bluetooth, run the included read-only
`scripts/diagnose-webos-dualsense.sh` on your rooted TV, and check Moonlight DS5
logs for `Bluetooth DualSense waveform ready for slot N`. Collect whether the
app's user can **write** the relevant Sony `/dev/hidraw*` device; root-only
writability is insufficient.

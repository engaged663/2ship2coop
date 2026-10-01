# libultraship patch

The co-op host (`HostMode`, the server's headless game) needs a windowless backend in libultraship
(`Fast::Fast3dWindow::SetHeadless`). The change is small (5 files, ~330 lines) and lives here as a patch
so the repo can keep pointing at the upstream submodule commit (`7f9b86a5`).

After cloning:

```
git submodule update --init --recursive
./coop/patches/apply-libultraship-patch.sh        # or apply-libultraship-patch.ps1 on Windows PowerShell
```

The scripts are idempotent. Regenerate the patch after editing libultraship with
`git -C libultraship diff 7f9b86a5 > coop/patches/libultraship-headless.patch`.

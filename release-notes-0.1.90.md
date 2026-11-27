# NEXAVERSE 0.1.90

- Settings → Logs opens GTA VI logs and launcher crash reports in File Explorer.
- GTA VI three-dot settings now include a paging-file drive selector. Applying memory setup verifies capacity on the selected drive before removing other paging-file configurations; Windows may require a restart to finish the move.
- Play checks active Windows commit headroom without automatically changing paging files or requiring the recommended 64 GiB capacity when sufficient headroom is already available.
- GTA VI logs now include launcher version, presentation build/hash, core hash and verified checkpoint commit, CPU model/thread count, RAM/commit snapshot, and detected GPU names/VRAM. GPU inventory is not a claim about the renderer's selected adapter.

The gameplay core remains checkpoint 2a9910e. The presentation wrapper adds diagnostics; rendering, shader/driver crashes, and performance remain experimental and unresolved. This release does not claim a graphics, crash, or 30 FPS fix. Existing saves, shader caches, rendering profile, F3 settings, fullscreen borderless, loading bar, and keyboard bindings are preserved. Other game runtimes are unchanged.

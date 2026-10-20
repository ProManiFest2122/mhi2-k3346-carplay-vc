# Scope of archived v8S manifests

`release_evidence/V8S_SOURCE_MANIFEST.json` is an **immutable historical snapshot**, covering 234 paths in the original development workspace, **including compiled intermediates, locally generated output, private reference inputs and work-in-progress records**. It is not the present public source tree's file inventory.

`SOURCE_TREE_SHA256SUMS.txt` records the original 131-file **v1.0 initial public source export**. Later documentation/source-publication corrections are additive and must not silently rewrite this historical list.

For the current published Git tree, use the latest `main` tree and the Git SHA of the applicable commit, not either historical list as proof that the file tree is complete.

For the immutable car-tested v8S installation use `release_evidence/V8S_PACKAGE_MANIFEST.json` with the compiled installer ZIP. `tools/verify_public_source.py <installer.zip>` checks every installer payload, including the copied AWK sources.

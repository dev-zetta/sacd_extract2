# Upstream provenance

This GUI was adopted from
[`git-moiseev/sacd_extract`](https://github.com/git-moiseev/sacd_extract) at
commit `6ef5f235e6a8e43dd8d64d9ef29502792457d386` (2026-07-30).

The upstream `package.json` identifies git-moiseev
`<mail.moiseev@gmail.com>` as the author and declares the code under the MIT
license. The upstream snapshot did not contain a standalone license file, so
the corresponding MIT license text and upstream copyright attribution are
preserved in [`LICENSE`](LICENSE).

Adoption changes include Linux support, discovery of packaged and locally
built `sacd_extract` executables, current long-form command-line options,
automated tests, and GitHub Actions packaging. The upstream nested `.git`
directory and its bundled, obsolete `sacd_extract.exe` binary were intentionally
not imported.

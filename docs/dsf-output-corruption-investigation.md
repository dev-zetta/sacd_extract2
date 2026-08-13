# DSF output corruption investigation

## Summary

The analyzed copy of `1-08 - Cast No Shadow.dsf` is structurally valid but its
audio payload has been overwritten. This is not a decoder-specific playback
problem in the Mechen M30: FFmpeg reproduces the same noise, and the damaged
regions contain data from the extraction host rather than DSD audio.

The most likely carrier was sacd-ripper's caller-owned 1 MiB `setvbuf()` output
cache. Both damaged regions are exactly the size of that cache and begin at its
flush boundaries. The cache was removed and stdio output is now unbuffered;
the operating system still provides its normal file cache.

The original Oasis SACD image was not available, so this conclusion cannot be
confirmed by re-extracting the affected track. A clean source image is required
to replace the damaged DSF.

## Affected file

- SHA-256: `da4e33554c074b68bb5fc7af9cde5593635aa0dad21fba5d34a16f8504cc870e`
- Size: 207,707,252 bytes
- Format: stereo DSD64, 2,822,400 Hz, LSB first
- Duration from the DSF header: 291.6666667 seconds
- Modification time retained by the copied file: 2026-02-17 01:27:48 +0100

The header, chunk sizes, sample count, 4,096-byte channel block size, data
length, metadata offset, and ID3v2.3 footer are mutually consistent with the
[Sony DSF specification](DSF_file_format_specification_E.pdf).

## Corrupt regions

The two corrupt regions are aligned to the DSF audio payload, whose file offset
is 92 bytes:

| Payload offset | File offset | Length | Approximate audio interval | SHA-256 |
| ---: | ---: | ---: | ---: | --- |
| 20 MiB | 20,971,612 | 1 MiB | 29.721542-31.207619 s | `e35ec7f060d9e62e41e6bbbc63b1173868ae03933b3b9f25b19f67ac5e45be86` |
| 28 MiB | 29,360,220 | 1 MiB | 41.610159-43.096236 s | `0a531da63ea34849b2491a8ef954225024aeb234f198cd7fdb4828cfa2a48874` |

Normal regions have the statistical characteristics of DSD audio. The corrupt
regions have nearly 8 bits/byte of entropy, use all 256 byte values, and are
effectively incompressible. Decoding them to 88.2 kHz PCM produces near-full-
scale noise and a zero-crossing rate around 0.45, versus approximately
0.02-0.04 in adjacent music.

The first region contains fragments that exactly match Intel AX210 `iwlwifi`
firmware data used by the extraction host, including the text `Secure LTF key
seed`. The source SACD images checked during this investigation do not contain
that text. Those bytes therefore cannot be decoded content from this track.

## Extractor defect

`scarletbook_output.c` previously allocated a 1 MiB buffer for every output and
passed it to `setvbuf()`. The feature was re-enabled by upstream commit
[b84c45f2](https://github.com/dev-zetta/sacd_extract2/commit/b84c45f2d297816ed468ad923213a7749f7c055e)
(`reactivated write cache`). A
corrupted buffer could consequently be flushed as a complete 1 MiB region while
leaving every DSF size and metadata field valid.

The fix removes the buffer and its ownership field, and explicitly selects
unbuffered stdio output. DSF writes are already 4,096-byte channel blocks, so
the kernel can cache efficient block-sized writes without retaining a second
long-lived copy in the process.

An UndefinedBehaviorSanitizer run also found a signed left-shift overflow in
the DST bit reader. Casting the shifted value to unsigned arithmetic preserves
the intended bits and removes the undefined behavior.

## Validation

The imported source was verified as `dev-zetta/sacd_extract2` master at
commit
[c9af7d4](https://github.com/dev-zetta/sacd_extract2/commit/c9af7d40a2a186aee1763ddc4c73f60c32270f8c)
before applying the fix.

- Release and AddressSanitizer/UndefinedBehaviorSanitizer builds complete.
- With leak detection disabled because it is unsupported in the execution
  environment, the sanitizer extraction exits successfully with no reported
  address or undefined-behavior errors.
- Track 8 from `Genesis.iso` is byte-identical before and after the cache fix:
  `3a7856c3920a67c3f3aaaa97a23ffb8e1c7e6e6ed21e5f66a48c21828421406c`.
- Track 8 from `Mobile Fidelity Sound Lab - Piano Man.iso` is byte-identical to
  the earlier extraction:
  `49b31125f8ed643f89bf3ab933d8ca9553ff05b38604b37e6fcc9a6bcbc9e2d9`.
- Both validation outputs have consistent DSF structures and no anomalous
  high-entropy 1 MiB regions.

This validates output compatibility and removes the identified corruption
carrier. It does not reconstruct bytes already lost from the Oasis track.

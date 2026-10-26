# Validation and boundaries

Remote Workstation SDK 0.1.0 targets Windows 10 version 2004/build 19041+ x64 and Windows 11 x64. It uses public Windows
capture/input/cryptography APIs plus a shared private entitlement runtime.

## Offline checks

Normal and AddressSanitizer builds pass 12 real-loopback TLS assertions and
106,164 protocol/compression/bounds assertions. The TLS fixture verifies a
70,001-byte echo, certificate pin refusal and absolute deadlines under incomplete
record trickling. It also checks a healthy 256-echo session spanning multiple
per-operation timeout budgets. Protocol tests cover independent image bytes, dimensions,
truncation and bounded signed pointer/key/wheel values. They do not capture a
desktop, inject Windows input or call licensing Begin. All applications and
setups compile W4/WX, static CRT, CFG, ASLR and NX.

## Completed guest campaigns

The frozen candidate completed both a normal campaign and a targeted Driver
Verifier campaign in the owned Hyper-V Windows build 19045
(10.0.19045) guest. Each completed installation, its explicit runtime
fixtures, package removal and required normal restarts. The guest was restored
to its original checkpoint and finished Off; host boot/security policy was not
changed. These results cover that candidate and fixture, not every Windows PC.

New kernel candidates used guest-only test signing. Independent fixture drivers
retained their verified Microsoft signatures. This is functional qualification,
not a Microsoft attestation/HLK/WHQL claim. Returned Microsoft packages and the
final embedded setup still require their own trust, membership and installation
checks before installable binaries are distributed.

## Demo and activation qualification

The production runtime is configured for one hour per product per Windows boot.
The production functional campaigns **did not wait a full hour**. A separately
pinned private build of the same runtime code used an explicit twelve-second
duration to exercise real expiry, per-product independence, no reset by closing
and reopening handles or repeated Begin, full-mode activation after expiry, and
full-mode persistence beyond another twelve seconds. Products 8 and 10 expired
on their own deadlines and were not unlocked by product 7's activation. Product
9 was already full after its own USB
test and is not described as an unused slot in the lifecycle probe.

The USB short-runtime fixture independently kept a read pending across the real
cutoff, observed completion after expiry and before its independent physical-read
timeout budget, and refused a new export after expiry. Genuine full-mode transfer
recovery followed. Known-IOCTL malformed buffers, restricted callers, limited
handle access, wrong-handle/cross-product proofs, modified signatures, replaced
and consumed nonces, and accepted-proof replay were refused. Five original
broker grants were independently verified against the public P256 key. These
tests are not proof of unbreakable licensing or immunity to kernel compromise.

Normal and AddressSanitizer offline builds also passed the strict shared runtime
installer's 115 identity/trust-refusal checks. License-server HTTP and signature
verification run during activation under a monotonic whole-operation budget.
Data paths retain their local entitlement checks and normal processing costs;
encrypted transports still perform their own TLS work.

## Remote fixture scope

Both guest campaigns passed 256 lossless synthetic application-frame TLS
roundtrips, incorrect certificate-pin/session-secret refusal before capture,
explicit input opt-in, bounded callback delivery, held-input cleanup, oversized
wire refusal, sixteen owner lifecycles and genuine full-boot activation.
Synthetic callbacks alone do not qualify real desktop capture or Windows input.

A separate owned interactive-session fixture passed in the normal and targeted Verifier campaigns.
It guarded its own foreground window before capture/input, compared known RGB
pixels transported over TLS, observed bounded F24/click messages and verified key
release after disconnect. It never captured or controlled the physical host.
This qualifies that owned guest window, not an arbitrary game, GPU or desktop.

The frozen SDK implementation SHA256 is 2e07e6d04e6950ddf1c339c8df4edc33570d39eea2d93a07a4de80e0b36a465c.
The shared production runtime input SHA256 is 5ee4e64afeffe527a02b74fc50bdcc7eed4808e6e3ec9a3db028039057c6bf80.

## Remaining boundaries

Representative Windows11 builds, Secure Boot/HVCI, additional GPU/display stacks,
high-DPI/multi-monitor layouts, locale/layout differences, third-party security
products, slow/hostile networks, suspend/resume and long-duration performance and
recovery remain separate qualification. TLS 1.2 and Windows cryptography provide
the implementation's transport boundary, not immunity to a compromised host,
stolen pairing code, denial of service or arbitrary kernel execution.

This first implementation uses primary-screen GDI capture, lossless compression
and standard same-session Windows `SendInput`. It does not claim audio transport,
GPU video encoding, hardware latency, secure-desktop access, low-level driver
input delivery, RDP compatibility or universal game compatibility.

The kernel runtime owns the supplied product's demo/full state. Public Windows
APIs and independent user-mode reimplementations remain usable outside it. A
public-source user-mode implementation is not unbreakable licensing enforcement.

Private kernel source, CABs, PDBs, lab probes, signing credentials and customer
keys are excluded from public repositories and downloads.

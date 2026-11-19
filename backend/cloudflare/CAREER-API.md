# NEXAVERSE player storage API

This is persistent backend storage, not a claim of PS5 MyCAREER compatibility.
The game protocol adapter is incomplete. `/health` reports `mycareerReady: false`
and `gameFormatVerified: false`. Do not create JSON records and label them valid
NBA 2K22 save files.

All routes below are under `/client/v1` on the NEXAVERSE Worker. They require the
player's Discord OAuth bearer token, verified with Discord on each request.
Browser-origin requests are rejected. IDs in the URL never select another user's
account: the verified Discord ID chooses the isolated Durable Object. Owner
research credentials are not used by player clients. Login also initializes or
updates the cloud profile with the verified Discord display name.

| Method | Route | Behavior |
| --- | --- | --- |
| GET | `/profile` | Create/read profile and synchronize verified display name. |
| PUT | `/profile/data` | Store opaque profile data with `If-Match: "revision"`. |
| GET | `/profile/data` | Download the stored profile data with checksum and revision. |
| GET | `/careers` | List this account's career records. |
| POST | `/careers` | Create a slot using `{requestId, slot, name, player}`. |
| PATCH | `/careers/{id}/progress` | Store `{baseRevision, progression}`. |
| PUT | `/careers/{id}/save` | Store opaque binary data with `If-Match: "revision"`. |
| GET | `/careers/{id}/save` | Download latest save; ETag revision and SHA256 header. |
| GET | `/careers/{id}/history` | List the three retained save versions. |
| POST | `/careers/{id}/restore` | Restore `{baseRevision, revision}` as a new version. |

Career creation uses a UUID requestId for retry safety. Slots are 1–10. A reused
request ID with different input or an occupied slot returns 409. Revision checks
reject stale writes from another device with 409; missing revisions return 428.

Saves are limited to 4 MiB and split into 64 KiB SQLite chunks. Chunks, checksum,
manifest and progression snapshot commit in one transaction. Downloads validate
size, chunk order and SHA256. An interrupted write cannot expose a partial save.
Restores preserve the older progression snapshot and create a new revision.

Progression is explicitly client-reported. This API does not award official VC,
unlock purchases, invent statistics, or supply proprietary game save formats.
Only the three newest saves are retained per career. Test restart persistence,
conflicting writes, rollback and account isolation before changing this contract.

Run `npm run check`, `node career-store.test.mjs`, `node player-service.test.mjs`
and `node player-auth.test.mjs`. Tests use an isolated local SQLite Worker and
synthetic payloads; they do not modify real player data or prove game compatibility.

The Worker also advertises the observed PPSA03669 profile upload service
`0x8290b650` at `/Session/OnlineContent/upload` and download service `0xca7a1109`
at `/Session/OnlineContent/download`. These require both Discord authentication
and an issued game session. The framed adapter splits descriptor metadata from
opaque payload bytes using `VCFIELDLIST_SIZE`, persists profile data, and returns
enumeration metadata and framed downloads. Run `node profile-content.test.mjs`
for authenticated HTTP upload/enumerate/download and restart tests. Framing and
game acceptance still require confirmation against the live PS5 client's upload;
synthetic round trips alone do not establish MyCAREER compatibility.

The native name-vetting service `0x0635c793` is advertised at
`/Session/Names/validate`. It accepts one or two ordered STRING descriptors with
ID `0x6e46752f` and returns ordered BOOL descriptors under that same ID, plus the
native result. It uses a private-network name policy (1–20 UTF-16 units, letters,
marks, spaces, apostrophes, periods and hyphens). Names are neither stored by this
handler nor included in protocol diagnostics. Discord and game-session checks
remain required. Run `node backend/name-validation.test.cjs` from the launcher
directory and the Worker profile-content test for coverage. In-game name commit
verification is tracked separately from protocol tests.

## Native checkpoint candidate

The optional `NEXAVERSE_NATIVE_CHECKPOINTS=1` binding advertises service
`0xfcfb638d` (`MYCAREER:CHECKPOINT`) at `/Session/MyCareer/checkpoint`.
PPSA03669 constructs it in `CAREERMODE::CHECKPOINTS::SendCheckpointRequest`
(RVA `0x17b9050`); missing service lookup leads to error `74abf406`.

The candidate validates the observed quest/context/checkpoint/type fields and
player/build scope, persists the client's opaque checkpoint state transactionally,
and returns SUCCESS plus the committed U64 checkpoint ID. START, PROGRESS_UPDATE,
COMPLETION and TERMINATION are client-reported transitions. Duplicate retries are
idempotent, closed checkpoints reject stale writes, and three previous versions
are retained. Unknown fields/types are rejected. Tokens are not stored in these
records and the protocol journal records schema only.

Run `node native-checkpoint.test.mjs` after `npm run check`. The synthetic native
wire test covers restart, retries, historical state and account isolation. The
response's optional quest/reward/global-state fields are not fabricated. This is
an experimental checkpoint commit adapter; live client acceptance, career save
reload and City session services remain unverified. `/health` stays
`mycareerReady: false`.

## Native career-upload candidate

With the checkpoint candidate enabled, native upload service `0xc5f871c3`
(RVA `0x1c05140`) is advertised at `/Session/MyCareer/upload`. The native request
contains U64 `SAVE_ID` (`0x67267fb1`) and payload length (`0x53d25be9`). The
response visitor at `0x1c05377` reads RESULT. The adapter acknowledges only after
the complete declared binary save, digest and manifest commit atomically in the
account's Durable Object. Identical retries preserve the revision; the three
latest versions are retained. Data is opaque and is not rewritten as a synthetic
save. `native-career-saves.test.mjs` covers a synthetic 4,154,320-byte upload,
truncation rejection, restart and account isolation.

The emulator's upload-length inference also covers this route so 32 KiB send
chunks remain private until the whole declared save arrives. The standalone
`diagnostics/test-career-upload-assembly.cpp` regression covers that behavior.
Native career enumeration, download/reload, quest-state acceptance and City
sessions remain unverified or incomplete. This does not make MyCAREER playable
by itself and does not change `/health` readiness claims.


## Native career-creation candidate

With the checkpoint candidate enabled, service `0x914b32f9` is advertised at
`/Session/MyCareer/create`. The PPSA03669 request builder at RVA `0x96c340`
sends the selected build, body fields and indexed attribute/initial/cap arrays.
The callback at `0x97b080` requires five selection fields, four body fields and
a nonzero U64 career ID (`0xd5e5f21d`). The handler persists the submitted build
transactionally and returns those fields with the assigned account-scoped ID.
It does not invent rewards, currency or progression.

Identical retries reuse the committed ID; different builds in an occupied slot
reject with conflict rather than overwriting it. Unassigned START checkpoints
for that exact slot are bound to the committed ID. This binding remains subject
to live-client verification. Run `node native-career-creation.test.mjs` for
required native response fields, indexed arrays, restart, retry, conflict and
account-isolation coverage. Native City entry and save/reload remain unverified;
`/health` continues to report `mycareerReady: false`.


## Native remote-save enumeration candidate

Service `0xba5ebe12` is advertised at `/Session/MyCareer/enumerate`. The PS5
synchronous consumer at RVA `0x1c04980` requires `NUM_SAVES` (`0x7eddfc4d`)
and indexed `SAVE_ID` (`0x67267fb1`), CRC64 (`0x620144f4`) and DATETIME
(`0xc3ccd0ad`) descriptors. Empty accounts return zero records. Populated
accounts list only completely committed native uploads, never synthetic saves.
CRC64 uses the reflected ECMA table with initial/final complement, matching
the native implementation at `0x28ddfb0`. The save's server commit timestamp
uses the adapter's existing millisecond DATETIME convention; calendar display
and native verification of that timestamp remain unverified.

`native-career-saves.test.mjs` checks the standard CRC64 vector, empty and
multiple-save lists, descriptor ordering, authentication, restart persistence
and account isolation. Native download/restore and City sessions remain
incomplete or unverified; this service is not proof of playable MyCAREER.


## Initial saved-build lookup candidate

Service `0x4e1deedc` is advertised at `/Session/MyCareer/attributes`. Its PS5
builder at RVA `0x1801f10` uses the common career envelope; the callback at
`0x185a480` passes successful results into `0x17b8160`. The field reader at
`0x17fd8d0` recognizes the selection fields also returned by creation.

This read-only adapter returns the authenticated account's exact committed
creation metadata for the supplied nonzero career ID. It rejects missing IDs,
slot mismatches and absent records. It does not invent rewards or infer ongoing
progression from opaque binary saves. The initial-build lookup is therefore not
a complete implementation of the original attributes/progression service.
Tests cover two accounts with the same numeric career ID but different builds,
exact response preservation, malformed requests, slot conflicts and restart.
Live usage as part of City entry remains unverified.


### Current ratings in creation and build lookup

The native rating reader at RVA `0x17ffab0` reads distinct indexed arrays:
`ATTRIBUTE` (`0xbeb8f114`), `ATTRIBUTE_VALUE` (`0x9a0efa17`),
`ATTRIBUTE_INITIAL` (`0x96bc2b19`) and `ATTRIBUTE_CAP` (`0x2947bfd5`).
The count check at `0x17ffccd` expects 22 attributes. Caps/initial values alone
are accepted but do not set current values. Creation and lookup now include all
22 current values, persisted in `native_career_values` and initialized from the
submitted build's actual initial ratings. Reads and retries preserve persisted
values. No upgrade/progression mutation API is implemented by this change.

Existing creation snapshots are backfilled only when the account has no opaque
native career upload that might contain newer progression. Otherwise the
adapter returns `career_progression_import_required` rather than resetting it.
New builds commit their current values atomically with creation. Tests cover
full current arrays, isolation, retry and restart preservation. This corrects
a verified response omission; its relationship to City entry is unverified.


## City election configuration

PPSA03669's builder at RVA `0x72ffa0` requests service `0x612f6478`.
The missing-service branch at `0x73026d` calls the generic error-dialog path;
read-only thread snapshots from a failed career attempt contained return
locations `0x730272` and `0x73029a` in that branch. Stack candidates are sampling
evidence, not a complete call trace or proof this is the only City blocker.

The private backend advertises `/Session/City/election`. With no election
configured, it returns an empty history JSON array in STRING `0x9b205d17` and
`{"candidates":[]}` in STRING `ELECTION` (`0x1a93a44c`). The native callback
at `0x730c40` parses both; its empty candidates branch at `0x7314f0` completes
the lookup. No election ID, times, candidates or winners are fabricated.
The native selector `0x45c6fcb2` must be STRINGCRC; authentication is required.
Run `node native-city-election.test.mjs`. Live game acceptance and City movement
remain unverified, and readiness remains false.


## Additional native OnlineContent payloads

The live trace captured `/Session/OnlineContent/upload` returning HTTP 501
(`online_content_protocol_not_implemented`) for a non-profile content scope.
The descriptor shape includes CATEGORY/TYPE, declared payload size, native
version, name, filename and description. The account-scoped native-content
adapter now persists these opaque payloads rather than rejecting the upload.
The original profile adapter retains priority for its verified category/type.

Files are keyed by category, type and filename within the verified Discord
account's Durable Object. Complete declared bytes, SHA-256, bounded metadata
and chunks commit atomically before acknowledgement. Retries are idempotent;
three versions are retained, with 32 files and 64 MiB retained payload per account.
Uploads cannot overwrite records in another category or account. Filenames are
SQL metadata, never filesystem paths. Tokens are excluded from stored metadata
and diagnostics log only schema, content selector CRCs and response codes.

The adapter supports the observed enumeration operation (`0x636556e9`), native
name/filename CRC filtering and scoped framed downloads with integrity checks.
`native-online-content.test.mjs` covers exact 4,154,320-byte HTTP round trips,
restart, retries, isolation, multiple files, retained versions and truncation/
integrity rejection. The category of the live rejected upload and its effect
on City loading still require verification; additional operations remain
explicitly unsupported and readiness remains false.

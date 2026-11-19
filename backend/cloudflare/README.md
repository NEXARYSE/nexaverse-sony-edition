# Cloudflare staging backend

This private protocol research backend does not implement MyCAREER login, career creation, saves or progression. `/health` explicitly reports `mycareerReady: false`; game requests return HTTP 501. Do not advertise this deployment as playable MyCAREER.

The Worker uses a SQLite Durable Object, supported on Workers Free. It stores at most 256 redacted protocol schema summaries. Raw requests, usernames, account IDs, query strings and credentials are not persisted remotely. All routes except health require a private bearer secret. Never distribute the owner research secret inside the public launcher.

Run `npm ci`, `npm run dev`, then `node smoke.test.mjs` for the local workerd and SQLite integration test. The local fixture token belongs only in ignored `.dev.vars`; production uses an independently generated random secret configured with `wrangler secret put NEXAVERSE_RESEARCH_TOKEN`.

Run `npm run check` for deployment bundling. `npm run deploy` requires the owner's Cloudflare authorization. Remote tests accept `WORKER_TEST_ORIGIN` and `WORKER_TEST_TOKEN` through the environment; never paste the secret into command arguments or logs.

Live staging URL and verification results are recorded in `deployment.json`. Public launcher releases remain separate from staging deployment.

For the owner's diagnostic runtime, run `node relay.cjs` from this folder. It binds
only 127.0.0.1:32122 and reads the private secret from `.private/research-token`.
The native runtime must explicitly opt in with `NEXAVERSE_NBA22_BACKEND_PORT=32122`.
Close the old local diagnostic backend before starting the relay on the same port.
Run `node relay.test.cjs` for isolated forwarding checks; set `TEST_LIVE_WORKER=1`
to additionally verify authenticated round trips against the deployed Worker.
This owner relay and its secret must not be bundled into the public launcher.

Player gateway (experimental)

`/client/Session/...` now verifies the player's Discord OAuth token directly with
Discord and dispatches to a Durable Object isolated by the verified Discord ID.
Issued game sessions persist across object eviction. No owner research token is
included in launcher clients. The launcher bridge forwards only bounded binary
game-service requests to this fixed Worker origin, without cookies or redirects.
The game HTTP adapter intercepts the original service address before DNS or TLS;
it connects only to the loopback bridge. There is no fallback to 2K servers.

Login and account lookup are experimental. Live tracing identified missing
OnlineContent service 0x8c8b8bf4 at lookup caller 0x901fd3ae0. It is now advertised
for protocol inspection; its data handler explicitly returns 501. MyCAREER
profiles, career saves and progression remain unimplemented. Do not describe
this routing deployment as playable MyCAREER.

Run player-auth.test.mjs, player-service.test.mjs (after npm run check), and the
root nba22-cloud.test.cjs for gateway validation. Player payloads and OAuth
tokens are not included in redacted protocol schema journals.

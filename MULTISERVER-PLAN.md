# MultiServer Architecture Plan

## Overview

Integrate MultiServerReplicationEx into OWS to enable flexible per-zone server configuration — from traditional single-server direct connections to full seamless DSTM-based multi-server replication with proxy multiplexing.

## Regions

Zones are grouped into **Regions**. Each region has its own proxy infrastructure.

- A **default** region always exists and cannot be deleted
- Each zone belongs to exactly one region (defaults to `default`)
- Each region has at least one proxy when active (spun up on first player connection to a proxy-enabled zone in that region, or kept ready if `PreemptiveProxySpinUp` is ON)
- A zone only registers with the region's proxy if it has `ProxyEnabled = true` at zone level, but the region assignment is always configurable regardless of proxy setting

## Zone Configuration Modes

Each zone is independently configured with three settings:

| Setting | Options |
|---------|--------|
| **Region** | Which region this zone belongs to (default: `default`) |
| **Server Mode** | `SingleServer` or `MultiServer` |
| **Proxy** | `Enabled` or `Disabled` |

### Mode Matrix

| Mode | Proxy | DSTM | Travel Type | Server Jump | Cross-Border Actors |
|------|-------|------|-------------|-------------|---------------------|
| SingleServer | Disabled | No | Hard travel | Reconnection | No |
| SingleServer | Enabled | No | Hard travel | Seamless (proxy handled) | No |
| MultiServer | Disabled | Yes | Hard travel | Reconnection | Yes* |
| MultiServer | Enabled | Yes | Seamless travel** | Seamless (proxy handled) | Yes* |

> *Actors seen across zone borders requires the adjacent zone to have DSTM enabled (MultiServer mode) AND the same UE Map configured
>
> **Seamless travel in MultiServer mode requires the adjacent zone to have the same UE Map configured

### Mode Behaviors

- **SingleServer + No Proxy**: Current OWS behavior. Client gets direct server IP:Port. Zone change = full disconnect + reconnect.
- **SingleServer + Proxy**: Client connects to proxy. Zone change = proxy handles backend swap internally; client stays connected but gets a loading screen / hard travel (no actor continuity).
- **MultiServer + No Proxy**: DSTM mesh active between servers. Actors replicate across boundaries. But zone change requires client reconnect to new server (no proxy to maintain session).
- **MultiServer + Proxy**: Full experience. Client on proxy, DSTM mesh active, seamless boundary crossing, actors visible across zones, no disconnect ever.

## Architecture Decisions

1. **No global MultiServer flag** — configuration is per-zone in the `Maps` table
2. **Regions**: Zones are grouped into regions. A `default` region always exists. Each region manages its own proxy infrastructure
3. **Player never switches proxies** — stays on initial proxy for lifetime of session (when proxy-enabled zones)
4. **Proxy lifecycle**: Dynamic, spun up per-region on demand (first connection to a proxy-enabled zone in a region waits for that region's proxy + game server)
5. **Proxy-to-region mapping**: One proxy per region (all proxy-enabled zones in a region behind that region's proxy), scalable
6. **PreemptiveZoneSpinUp**: Three modes — `Off` (on-demand only, shimmer wall until ready), `Adjacent` (spin up adjacent zones when any zone has players), `Full` (keep at least one server per zone running at all times)
7. **PreemptiveProxySpinUp**: Toggle that keeps one proxy ready per region as long as at least one zone in that region has `ProxyEnabled = true`. When OFF → proxy spun up on first player connection
8. **DSTM args**: Only included in launch command for MultiServer zones. SingleServer zones launch without DSTM/peer args

## Connection Flows

### SingleServer + No Proxy (current behavior)
```
Player Login → GetServerToConnectTo
  → Return direct server IP:Port
  → Client connects directly to game server

Zone change:
  → Client disconnects
  → Calls GetServerToConnectTo for new zone
  → Connects to new server IP:Port
```

### SingleServer + Proxy
```
Player Login → GetServerToConnectTo
  ├─ Region's proxy exists & ready?
  │    ├─ YES → return Proxy IP:Port
  │    └─ NO  → Spin up region's Proxy → wait for ready
  │              → Spin up zone server → wait for ready
  │              → return Proxy IP:Port
  │
  Client connects to Proxy (stays connected)

Zone change:
  → Proxy swaps backend server internally
  → Client experiences hard travel (loading screen) but no disconnect
  → No actors visible across borders
```

### MultiServer + No Proxy
```
Player Login → GetServerToConnectTo
  → Return direct server IP:Port (DSTM-enabled server)
  → Client connects directly

Zone boundary:
  → DSTM mesh active — actors visible across borders
  → Actual zone change requires client disconnect + reconnect to new server
```

### MultiServer + Proxy (full experience)
```
Player Login → GetServerToConnectTo
  ├─ Region's proxy exists & ready?
  │    ├─ YES → return Proxy IP:Port
  │    └─ NO  → Spin up region's Proxy → wait for ready
  │              → Spin up first zone server → wait for ready
  │              → return Proxy IP:Port
  │
  Client connects to Proxy (stays connected forever)
  Proxy routes to primary backend game server
```

### Zone Boundary Crossing
```
  Player walks toward zone boundary:
  ├─ PreemptiveZoneSpinUp = Full?
  │    └─ All zones already have a server running — no wait
  ├─ PreemptiveZoneSpinUp = Adjacent?
  │    └─ Adjacent zones spun up automatically when any zone has players
  ├─ PreemptiveZoneSpinUp = Off?
  │    └─ Shimmer wall active until target zone is running
  │       Player approaches → triggers spin-up request to OWS
  │       Zone server starts → registers with proxy if proxy-enabled
  │       Shimmer wall dissolves
  │
  Transfer depends on zone config:
  ├─ SingleServer + No Proxy  → Client disconnects, reconnects to new server
  ├─ SingleServer + Proxy     → Proxy swaps backend, client sees hard travel
  ├─ MultiServer + No Proxy   → Client disconnects, reconnects (DSTM mesh stays for actors)
  └─ MultiServer + Proxy      → DSTM seamless transfer:
       Source GameMode detects boundary cross
       → TransferActorToServer(Pawn, DestServerId)
       → TransferActorToServer(PC, DestServerId)
       → Proxy reassigns primary route to destination backend
       → Player is now in new zone, never left the proxy
```

## OWS Backend Changes

| Layer | Change |
|-------|--------|
| **Database — Regions table** | New table: RegionID, Name, CustomerGUID. Default region `default` always exists and cannot be deleted |
| **Database — Maps table** | Add `RegionID` (FK to Regions, default: `default`), `ServerMode` (SingleServer/MultiServer), and `ProxyEnabled` (bool) columns to zone configuration |
| **Database — ProxyInstances** | New table: ProxyID, RegionID, WorldServerID, IP, Port, Status, MaxPlayers, ConnectedPlayers, LastHeartbeat |
| **GetServerToConnectTo** | Check zone's ServerMode + ProxyEnabled: if proxy-enabled, find/create the region's proxy and return proxy address; otherwise route to direct server |
| **Instance Launcher** | New `SpinUpProxy` message type. Proxy launched with `-ProxyRegistrationPort`. MultiServer + Proxy zones launched with `-JoinProxy`, `-DedicatedServerId`, `-DSTMListenPort`. MultiServer + No Proxy zones launched with `-DedicatedServerId`, `-DSTMListenPort`, `-DSTMPeers`. SingleServer zones launched as today |
| **New Endpoints** | `RegisterProxy`, `HeartbeatProxy`, `GetProxyStatus`, `GetZoneStatusForProxy`, `GetZoneConfig`, `AddRegion`, `GetRegions`, `UpdateRegion`, `DeleteRegion` |
| **Config** | `PreemptiveZoneSpinUp` option (Off / Adjacent / Full), `PreemptiveProxySpinUp` toggle |
| **Zone Server Spin-up** | Reads zone config to decide which args to include in launch command |

## HubWorldMMO Game Changes

| Layer | Change |
|-------|--------|
| **GameInstance** | Detect proxy connection mode, swap NetDriver to `ProxyNetDriver` when connecting through proxy |
| **GameMode** | Zone boundary detection + DSTM migration trigger for MultiServer zones (like Nyx's `CheckZoneBoundaries`) |
| **Zone Boundary Actor** | Blueprint/C++ actor with shimmer wall visual. Shimmer wall blocks when target zone's server is not running (regardless of mode). Once ready: hard travel trigger (SingleServer), or seamless pass-through (MultiServer + Proxy) |
| **OWSPlugin** | New API calls: query zone config/status, request zone spin-up, proxy heartbeat |

## What Stays Unchanged

- **MultiServerReplicationEx** — already has everything needed (proxy, DSTM, dynamic registration)
- **SingleServer + No Proxy zones** — behave exactly like current OWS (zero impact on existing deployments)

## Implementation Order

- [ ] **Phase 1: OWS Backend** — Regions table + zone config columns (RegionID, ServerMode, ProxyEnabled) + ProxyInstances table + modified GetServerToConnectTo routing
- [ ] **Phase 2: Instance Launcher** — Proxy spin-up support + conditional DSTM args based on zone config
- [ ] **Phase 3: OWSPlugin** — New API calls for zone config, proxy/zone status
- [ ] **Phase 4: HubWorldMMO GameInstance** — Proxy detection + conditional NetDriver swap
- [ ] **Phase 5: HubWorldMMO GameMode** — Boundary detection + mode-aware transfer logic
- [ ] **Phase 6: HubWorldMMO Zone Boundary** — Shimmer wall actor + zone status polling + preemptive spin-up toggle
- [ ] **Phase 7: Integration Test** — Test all 4 mode combinations end-to-end

## Reference

- **[Nyx project](https://github.com/jota2rz/Nyx)**: Reference implementation of DSTM transfer + proxy mode
- **[MultiServerReplicationEx](https://github.com/jota2rz/MultiServerReplicationEx)**: Proxy net driver, DSTM subsystem, dynamic registration
- **Key launch args**: `-DedicatedServerId`, `-DSTMListenPort`, `-JoinProxy`, `-ProxyRegistrationPort`, `-DisableGarbageElimination`

# MultiServer Architecture Plan

## Overview

Integrate MultiServerReplicationEx into OWS to enable seamless DSTM-based player transfer between zone servers through a proxy, with no client disconnect.

## Architecture Decisions

1. **Player never switches proxies** — stays on initial proxy for lifetime of session
2. **GetServerToConnectTo** returns proxy IP:Port in MultiServer mode; seamless migration is server-to-server via DSTM
3. **Proxy lifecycle**: Dynamic, spun up on demand (first connection waits for proxy + first game server to be ready)
4. **Proxy-to-zone mapping**: One proxy per "world" (all zones behind one proxy), scalable
5. **Zone spin-up**: Togglable preemptive adjacent zone spin-up. When OFF → shimmer wall blocks until target zone is up
6. **Fallback**: MultiServer OFF = current OWS behavior unchanged (direct connections, hard travel)

## Connection Flow (MultiServer Mode ON)

```
Player Login → GetServerToConnectTo
  ├─ Proxy exists & ready?
  │    ├─ YES → return Proxy IP:Port
  │    └─ NO  → Spin up Proxy → wait for ready
  │              → Spin up first zone server → wait for ready
  │              → return Proxy IP:Port
  │
  Client connects to Proxy (and stays there forever)
  Proxy routes to primary backend game server

  Player walks toward zone boundary:
  ├─ PreemptiveSpinUp ON?
  │    └─ All adjacent zones spun up automatically when any zone has players
  ├─ PreemptiveSpinUp OFF?
  │    └─ Shimmer wall active on boundaries where target zone is NOT running
  │       Player approaches → triggers spin-up request to OWS
  │       OWS spins up zone server → server registers with proxy via -JoinProxy
  │       Shimmer wall dissolves → DSTM seamless transfer
  │
  DSTM transfer (server-to-server, client never disconnects):
    Source GameMode detects boundary cross
    → TransferActorToServer(Pawn, DestServerId)
    → TransferActorToServer(PC, DestServerId)
    → Proxy reassigns primary route to destination backend
    → Player is now in new zone, never left the proxy
```

## OWS Backend Changes

| Layer | Change |
|-------|--------|
| **Database** | New `ProxyInstances` table (ProxyID, WorldServerID, IP, Port, Status, MaxPlayers, ConnectedPlayers, LastHeartbeat) |
| **Config** | `MultiServerEnabled` flag (default: false). When false, everything works as today |
| **GetServerToConnectTo** | If MultiServer ON: find/create proxy → ensure first zone server → return proxy address. If OFF: current behavior |
| **Instance Launcher** | New `SpinUpProxy` message type. Proxy launched with `-ProxyRegistrationPort`. Zone servers launched with `-JoinProxy`, `-DedicatedServerId`, `-DSTMListenPort` |
| **New Endpoints** | `RegisterProxy`, `HeartbeatProxy`, `GetProxyStatus`, `GetZoneStatusForProxy` |
| **Zone Server Spin-up** | Extended to include DSTM/proxy args in the launch command |

## HubWorldMMO Game Changes

| Layer | Change |
|-------|--------|
| **GameInstance** | Detect proxy connection mode, swap NetDriver to `ProxyNetDriver` (like Nyx) |
| **GameMode** | Zone boundary detection + DSTM migration trigger (like Nyx's `CheckZoneBoundaries`) |
| **Zone Boundary Actor** | Blueprint/C++ actor with shimmer wall visual. Queries OWS for target zone status. Blocks when zone not ready, dissolves when ready |
| **OWSPlugin** | New API calls: query zone status, request zone spin-up, proxy heartbeat |
| **Config** | `PreemptiveAdjacentZoneSpinUp` toggle |

## What Stays Unchanged

- **MultiServerReplicationEx** — already has everything needed (proxy, DSTM, dynamic registration)
- **MultiServer OFF mode** — entire current OWS flow untouched (direct connections, `GetServerToConnectTo` returns game server IP:port)

## Implementation Order

- [ ] **Phase 1: OWS Backend** — Proxy table + MultiServer config + modified spin-up flow + new endpoints
- [ ] **Phase 2: Instance Launcher** — Proxy spin-up support + DSTM args for zone servers
- [ ] **Phase 3: OWSPlugin** — New API calls for proxy/zone status
- [ ] **Phase 4: HubWorldMMO GameInstance** — Proxy detection + NetDriver swap
- [ ] **Phase 5: HubWorldMMO GameMode** — Boundary detection + DSTM transfer
- [ ] **Phase 6: HubWorldMMO Zone Boundary** — Shimmer wall actor + zone status polling + preemptive spin-up toggle
- [ ] **Phase 7: Integration Test** — Full flow: login → proxy spin-up → zone spin-up → connect → walk to boundary → seamless transfer

## Reference

- **[Nyx project](https://github.com/jota2rz/Nyx)**: Reference implementation of DSTM transfer + proxy mode
- **[MultiServerReplicationEx](https://github.com/jota2rz/MultiServerReplicationEx)**: Proxy net driver, DSTM subsystem, dynamic registration
- **Key launch args**: `-DedicatedServerId`, `-DSTMListenPort`, `-JoinProxy`, `-ProxyRegistrationPort`, `-DisableGarbageElimination`

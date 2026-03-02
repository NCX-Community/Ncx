# Feature Specification: Engine Cluster Management — NCX Engine Side

**Feature Branch**: `001-grpc-engine-cluster`
**Created**: 2026-03-02
**Status**: Draft
**Input**: User description: "Engine Cluster Management Module — engine registration, deregistration, heartbeat, and proxy port query via persistent stream connection between NCX engine and engine proxy"

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Engine Registers with the Engine Proxy (Priority: P1)

When an NCX engine instance starts up, it establishes a persistent streaming connection to the engine proxy and announces itself, providing its network address and initial load state. The engine proxy records this engine as active and available to receive proxy tunnel requests.

**Why this priority**: Without registration, the engine proxy cannot route any traffic to an engine. This is the foundational capability everything else depends on.

**Independent Test**: Start one NCX engine targeting a configured engine proxy address. Verify the proxy's active engine list now contains the engine's address and reports its initial occupied proxy count.

**Acceptance Scenarios**:

1. **Given** an NCX engine is not yet registered, **When** the engine starts and connects to the engine proxy, **Then** the engine proxy adds the engine to its active cluster with the engine's reported address (ip:port) and current occupied proxy count.
2. **Given** an NCX engine has sent its registration data, **When** the engine proxy receives it, **Then** the proxy acknowledges the registration and the engine is eligible for load-balanced selection.
3. **Given** an engine with zero occupied proxy ports registers, **When** the engine proxy consults the cluster for routing, **Then** this engine is preferred as least-loaded.

---

### User Story 2 — Engine Sends Periodic Heartbeats and Status Updates (Priority: P2)

While connected to the engine proxy, the NCX engine continuously reports its liveness and updated load information (current number of occupied proxy addresses) over the same persistent connection. This allows the engine proxy to detect stale or overloaded engines and route traffic accordingly.

**Why this priority**: Without heartbeats, the engine proxy cannot detect crashed or disconnected engines, leading to routing failures. Without load updates, load balancing decisions become inaccurate.

**Independent Test**: Register an engine, then have it open several proxy tunnels. Verify that subsequent status messages observed at the engine proxy reflect the updated occupied proxy count.

**Acceptance Scenarios**:

1. **Given** a registered engine is active, **When** the engine sends a heartbeat message, **Then** the engine proxy updates the engine's last-seen timestamp and considers it alive.
2. **Given** a registered engine opens new proxy tunnels, **When** it sends a status update with an increased occupied proxy count, **Then** the engine proxy updates the stored load value for that engine.
3. **Given** a registered engine stops sending heartbeats for longer than the maximum allowed interval, **When** the heartbeat deadline is exceeded, **Then** the engine proxy marks the engine as unavailable and removes it from routing consideration.

---

### User Story 3 — Engine Proxy Queries Available Proxy Ports from an Engine (Priority: P3)

When the engine proxy needs to route a new connection request to a specific NCX engine, it sends a downstream query over the persistent stream, asking the engine to report its currently available (unoccupied) proxy ports. The engine responds in real time so the proxy can allocate the correct port to the incoming request.

**Why this priority**: This enables dynamic, real-time port allocation rather than relying on stale cached data, ensuring accurate resource tracking as proxy sessions open and close.

**Independent Test**: Register an engine with known available ports. Trigger a proxy-port query from the engine proxy side. Verify the response lists ports that are currently free and excludes ports already in use.

**Acceptance Scenarios**:

1. **Given** a registered engine has available proxy ports, **When** the engine proxy sends a proxy port query, **Then** the engine responds with the list of currently available proxy port addresses.
2. **Given** all proxy ports on an engine are occupied, **When** the engine proxy queries that engine, **Then** the engine responds with an empty list, and the engine proxy does not route to that engine.
3. **Given** the engine proxy has received an available port list, **When** it selects a port for a connection, **Then** subsequent queries reflect that the selected port is now occupied.

---

### User Story 4 — Engine Deregisters Gracefully or Is Detected as Lost (Priority: P4)

When an NCX engine shuts down cleanly, it closes the persistent connection to the engine proxy. When an engine crashes or loses connectivity, the engine proxy detects the broken connection. In both cases, the engine is removed from the active cluster and no further traffic is routed to it.

**Why this priority**: Prompt deregistration prevents routing failures and wasted connection attempts to unavailable engines.

**Independent Test**: Register an engine, then either shut it down cleanly or simulate a disconnection. Verify the engine proxy removes the engine from its active list within the heartbeat detection window.

**Acceptance Scenarios**:

1. **Given** a registered engine is active, **When** the engine closes its connection gracefully, **Then** the engine proxy immediately removes that engine from the active cluster.
2. **Given** a registered engine's connection drops unexpectedly, **When** the engine proxy detects the broken stream, **Then** it removes the engine from the active cluster without manual intervention.
3. **Given** an engine has been removed from the cluster, **When** the engine proxy receives a new routing request, **Then** the removed engine is never selected as a routing target.

---

### Edge Cases

- What happens when an engine reports a duplicate registration (same ip:port as an already-registered engine)?
- How does the system handle a status update message that arrives with a lower occupied proxy count than the last known value (e.g., tunnels closed)?
- What happens if the persistent connection is re-established quickly (engine restart) — is the old registration replaced atomically?
- How does the engine proxy behave when the cluster is empty (all engines deregistered) and a new routing request arrives?
- What happens when an engine sends a heartbeat but the stream is congested and the message arrives late?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The NCX engine MUST establish a persistent bidirectional streaming connection to the engine proxy on startup before accepting any proxy tunnel requests.
- **FR-002**: The NCX engine MUST send a registration message containing its public network address (ip:port) and its current number of occupied proxy addresses at connection establishment.
- **FR-003**: The NCX engine MUST send periodic heartbeat messages over the persistent connection to signal liveness to the engine proxy.
- **FR-004**: The NCX engine MUST include its current occupied proxy count in each status/heartbeat message so the engine proxy has up-to-date load information.
- **FR-005**: The engine proxy MUST remove an NCX engine from the active cluster when its persistent connection closes or when no heartbeat is received within the configured timeout window.
- **FR-006**: The engine proxy MUST respond to proxy port queries by dispatching a downstream message over the persistent stream requesting the engine's current available proxy ports.
- **FR-007**: The NCX engine MUST respond to downstream proxy port queries with a list of currently available (unoccupied) proxy port addresses.
- **FR-008**: The engine proxy MUST use the occupied proxy count reported by engines as input for load-balanced engine selection when routing new connection requests.
- **FR-009**: The NCX engine MUST handle deregistration gracefully by closing the persistent connection cleanly on shutdown.
- **FR-010**: The engine proxy MUST support managing multiple simultaneously registered NCX engines as a cluster.

### Key Entities

- **Engine Registration**: Represents a single NCX engine's membership in the cluster. Key attributes: network address (ip:port), occupied proxy count, last-seen timestamp, connection status.
- **Heartbeat / Status Message**: A periodic upstream message from the engine carrying liveness signal and current load (occupied proxy count).
- **Proxy Port Query**: A downstream message from the engine proxy requesting the engine's currently available proxy port list.
- **Available Proxy Port List**: The engine's response to a proxy port query — an ordered list of unoccupied proxy port addresses.
- **Engine Cluster**: The collection of all currently registered and active NCX engines, used by the engine proxy for routing and load balancing decisions.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: An NCX engine completes registration with the engine proxy within 2 seconds of establishing the persistent connection.
- **SC-002**: The engine proxy detects and removes a failed/disconnected engine from the active cluster within 1.5× the configured heartbeat interval (i.e., detection is near-immediate after one missed heartbeat cycle).
- **SC-003**: The engine proxy receives and reflects load updates (occupied proxy count changes) within one heartbeat interval of the engine opening or closing a proxy tunnel.
- **SC-004**: A proxy port query round-trip (engine proxy sends query → engine responds with available ports) completes within 500 milliseconds under normal network conditions.
- **SC-005**: The cluster management module correctly tracks 10 or more concurrently registered engines without routing errors or missed deregistrations.
- **SC-006**: Load-balanced engine selection always routes new connections to the engine with the fewest occupied proxy ports among available engines, with 100% accuracy during correctness testing.
- **SC-007**: When all engines deregister, the engine proxy queues or rejects incoming routing requests gracefully rather than crashing or entering an error state.

## Assumptions

- The scope of this spec covers only the NCX engine's participation in the cluster protocol (what the engine sends and receives). Engine proxy internal scheduling logic is out of scope for this feature.
- Heartbeat interval is configurable via the engine's configuration file; a reasonable default of 10 seconds is assumed.
- The engine proxy address is statically configured in the NCX engine's configuration file (no service discovery is in scope).
- A single NCX engine instance connects to exactly one engine proxy (no multi-proxy fan-out in scope).
- Proxy ports available for reporting are those currently configured on the engine that are not actively serving a visitor tunnel.
- The persistent streaming connection is assumed to be reliable enough that the existing TCP layer handles packet retransmission; the application layer only needs to handle full disconnection events.

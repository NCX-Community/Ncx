# Feature Specification: libevent TCP Network Layer Integration

**Feature Branch**: `001-libevent-tcp`
**Created**: 2026-03-01
**Status**: Draft
**Input**: User description: "Implement TCP networking functionalities using libevent in the ENGINE's network layer. Ensure that the integration of libevent maintains normal operations between the network layer and upper layers."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Reliable TCP Connection Establishment (Priority: P1)

A developer using the ENGINE can open both inbound and outbound TCP connections that complete successfully and remain stable under normal network conditions. Connections established through the network layer deliver a connected socket to upper-layer components (NCX server and client) without any observable change in behaviour compared to the previous implementation.

**Why this priority**: Connection establishment is the foundational capability on which all other tunnelling and proxying behaviour depends. Without it, no other scenario is possible.

**Independent Test**: Start the ENGINE server, connect an ENGINE client, confirm that the control-channel handshake completes and an `Ack` message is exchanged. This delivers end-to-end value (a live tunnel) without requiring data-channel or visitor traffic.

**Acceptance Scenarios**:

1. **Given** the ENGINE server is running, **When** an ENGINE client initiates a control-channel connection, **Then** the connection is accepted and the handshake completes within 2 seconds.
2. **Given** the ENGINE client is connected, **When** it requests a data channel, **Then** a second TCP connection to the server is established and confirmed within 1 second.
3. **Given** a connection is established, **When** either side closes it gracefully, **Then** the other side receives a close notification and all resources are released.

---

### User Story 2 - Non-Blocking Event-Driven I/O (Priority: P2)

The network layer processes all socket I/O and timer events without blocking any thread for longer than necessary. All registered callbacks (on_message, on_close, on_connect) are invoked promptly when the corresponding event occurs, enabling upper-layer components to react to network activity in real time.

**Why this priority**: Non-blocking I/O is the core contract between the network layer and the upper layers. Blocking behaviour would degrade throughput and responsiveness across all tunnelled sessions.

**Independent Test**: Send a burst of 1 000 messages over a single connection and verify that all callbacks fire in the correct order and no message is dropped, demonstrating the event loop processes pending events reliably.

**Acceptance Scenarios**:

1. **Given** an active connection with pending inbound data, **When** the event loop cycles, **Then** the `on_message` callback is called with the complete buffered data within one event-loop iteration.
2. **Given** 50 simultaneous active connections, **When** all connections send data concurrently, **Then** every `on_message` callback fires and no data is lost.
3. **Given** the event loop is running, **When** no events are pending, **Then** the loop idles without spinning (CPU usage near 0 %).

---

### User Story 3 - Upper-Layer API Compatibility (Priority: P3)

All components that sit above the network layer — specifically the NCX server (`SControlChannel`, `NServer`) and NCX client (`CControlChannel`, `DataChannel`) — continue to work correctly without requiring changes to their own logic. The network layer fulfils the same behavioural contract as before: it provides connection callbacks, message callbacks, close callbacks, and a write interface.

**Why this priority**: If upper layers must be rewritten, the migration risk grows significantly. Maintaining the API contract allows the network-layer change to be validated independently.

**Independent Test**: Run the full tunnel scenario (visitor connects → data channel created → bidirectional data forwarded → connection closed) and confirm no functional regressions against the behaviour described in the existing architecture documentation.

**Acceptance Scenarios**:

1. **Given** the refactored network layer is in place, **When** the existing NCX server binary is run with its current configuration, **Then** it accepts client connections and proxies visitor traffic correctly.
2. **Given** a connection object, **When** upper-layer code calls the write operation with a byte buffer, **Then** the data arrives at the remote end intact and in order.
3. **Given** a connection that the remote side has closed, **When** the network layer detects the close, **Then** the registered `on_close` callback is invoked exactly once.

---

### User Story 4 - Concurrent Connection Scalability (Priority: P4)

The system sustains a large number of simultaneously open TCP connections without exhausting file descriptors, memory, or CPU in a way that degrades service. Each connection is tracked and cleaned up properly when it closes.

**Why this priority**: Scalability is an important quality attribute for production deployments but depends on the core functionality in the previous stories being solid first.

**Independent Test**: Open 500 simultaneous tunnelled connections, transfer data on each, close them all, and verify that resource usage returns to baseline after all connections are closed.

**Acceptance Scenarios**:

1. **Given** 500 concurrent open connections, **When** all are active simultaneously, **Then** no connection is dropped due to resource exhaustion.
2. **Given** a connection that has been closed, **When** the close has been processed, **Then** the connection is removed from the connection pool and its file descriptor is released.

---

### Edge Cases

- What happens when the remote peer abruptly terminates without a graceful close (e.g., network reset)?
- How does the system handle a write operation to a connection whose remote end has already closed?
- What happens when the listening port is already in use at startup?
- How does the event loop behave if a registered callback encounters an unhandled error?
- What happens when data arrives in multiple small fragments that together form a single framed protocol message?
- How does the system handle an outbound connection attempt that times out or is refused?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The network layer MUST accept inbound TCP connections on a configured port and invoke the registered connection callback for each accepted connection.
- **FR-002**: The network layer MUST initiate outbound TCP connections to a specified host and port, invoking the connection callback when the connection is established or reporting failure if it cannot connect.
- **FR-003**: The network layer MUST deliver inbound data to the registered message callback as soon as it is available, preserving byte order and message boundaries as defined by the upper-layer framing protocol.
- **FR-004**: The network layer MUST allow upper-layer components to write arbitrary byte buffers to a connection; the data MUST be delivered to the remote end intact and in order.
- **FR-005**: The network layer MUST detect when a connection is closed by either side and invoke the close callback exactly once per connection.
- **FR-006**: The network layer MUST support multiple simultaneous connections managed within a single event loop without blocking on any individual connection.
- **FR-007**: The network layer MUST provide a mechanism for upper-layer components to schedule work to run on the event-loop thread from other threads.
- **FR-008**: The event loop MUST be startable and stoppable in a controlled manner, enabling clean shutdown of the ENGINE process.
- **FR-009**: The network layer MUST release all resources associated with a connection (buffers, file descriptors, internal state) when the connection is closed.
- **FR-010**: The network layer MUST support running multiple independent event loops on separate threads, enabling a multi-threaded server architecture.

### Key Entities

- **Connection**: Represents a single live TCP session between two endpoints. Carries inbound and outbound data buffers and the registered lifecycle callbacks (message received, connection closed). Has a unique identity within the connection pool.
- **Event Loop**: The execution context that monitors file descriptors for I/O readiness and dispatches callbacks. Owns zero or more Connections. Supports task injection from external threads.
- **Server (Listener)**: Binds to a local address and accepts incoming Connections on behalf of the application. Delegates each accepted Connection to an Event Loop.
- **Connector**: Initiates an outbound Connection to a remote address. Reports success or failure via a callback.
- **Buffer**: An in-memory byte queue used to accumulate inbound data before it is consumed by a callback and to hold outbound data before it is written to the socket.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All existing integration scenarios (control-channel handshake, data-channel establishment, bidirectional data forwarding, graceful disconnect) pass without modification to any code outside the network layer.
- **SC-002**: A single event loop sustains at least 500 concurrent TCP connections without observable data loss or callback delivery failures.
- **SC-003**: The round-trip latency for a single message traversing a tunnel does not regress by more than 10 % compared to the baseline measured before this change.
- **SC-004**: After closing all connections in a scalability test, the process returns to baseline memory usage with no unbounded growth, confirming correct resource cleanup.
- **SC-005**: The ENGINE server starts, accepts a client connection, proxies one complete visitor session, and shuts down cleanly using the same startup and configuration steps documented in the project README — without any changes to configuration files or startup procedures.
- **SC-006**: CPU utilisation of the event-loop thread remains below 5 % when no connections are active, confirming no busy-wait or spin-loop behaviour.

## Assumptions

- The upper-layer components (NCX server and client) will not be modified as part of this feature; their source code is treated as a fixed external contract.
- The target deployment environment is Linux; the feature does not need to support Windows or macOS.
- A single-threaded event loop per event-loop-thread instance is sufficient; this feature does not introduce shared-state concurrency within a single event loop.
- Existing protobuf-based framing and message parsing logic above the network layer is unaffected and out of scope for this feature.
- Performance baselines will be measured on the same hardware before and after the change to evaluate SC-003.

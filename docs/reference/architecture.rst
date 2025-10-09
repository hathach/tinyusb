************
Architecture
************

This document explains TinyUSB's internal architecture, design principles, and how different components work together.

Design Principles
=================

Memory Safety
-------------

TinyUSB is designed for resource-constrained embedded systems with strict memory requirements:

TinyUSB uses **no dynamic allocation** - all memory is statically allocated at compile time for predictability. All buffers have bounded, compile-time defined sizes to prevent overflow issues. The TinyUSB core avoids heap allocation, resulting in **predictable memory usage** where consumption is fully deterministic.

Thread Safety
-------------

TinyUSB achieves thread safety through a deferred interrupt model:

- **ISR deferral**: USB interrupts are captured and deferred to task context
- **Single-threaded processing**: All USB protocol handling occurs in task context
- **Queue-based design**: Events are queued from ISR and processed in ``tud_task()``
- **RTOS integration**: Proper semaphore/mutex usage for shared resources

Portability
-----------

The stack is designed to work across diverse microcontroller families:

- **Hardware abstraction**: MCU-specific code isolated in portable drivers
- **OS abstraction**: RTOS dependencies isolated in OSAL layer
- **Modular design**: Features can be enabled/disabled at compile time
- **Standard compliance**: Strict adherence to USB specifications

Core Architecture
=================

Layer Structure
---------------

TinyUSB follows a layered architecture from hardware to application:

.. code-block:: none

   ┌─────────────────────────────────────────┐
   │           Application Layer             │ ← Your code
   ├─────────────────────────────────────────┤
   │         USB Class Drivers               │ ← CDC, HID, MSC, etc.
   ├─────────────────────────────────────────┤
   │        Device/Host Stack Core           │ ← USB protocol handling
   ├─────────────────────────────────────────┤
   │      Hardware Abstraction (DCD/HCD)     │ ← MCU-specific drivers
   ├─────────────────────────────────────────┤
   │         OS Abstraction (OSAL)           │ ← RTOS integration
   ├─────────────────────────────────────────┤
   │       Common Utilities & FIFO           │ ← Shared components
   └─────────────────────────────────────────┘

Component Overview
------------------

**Application Layer**: Your main application code that uses TinyUSB APIs.

**Class Drivers**: Implement specific USB device classes (CDC, HID, MSC, etc.) and handle class-specific requests.

**Device/Host Core**: Implements USB protocol state machines, endpoint management, and core USB functionality.

**Hardware Abstraction**: MCU-specific code that interfaces with USB peripheral hardware.

**OS Abstraction**: Provides threading primitives and synchronization for different RTOS environments.

**Common Utilities**: Shared code including FIFO implementations, binary helpers, and utility functions.

Device Stack Architecture
=========================

This section is concerned with the **Device Stack**, i.e., the component of TinyUSB used in USB devices (that talk to a USB host).

Core Components
---------------

**Device Controller Driver (DCD)**:
- MCU-specific USB device peripheral driver
- Handles endpoint configuration and data transfers
- Abstracts hardware differences between MCU families
- Located in ``src/portable/VENDOR/FAMILY/``

**USB Device Core (USBD)**:
- Implements USB device state machine
- Handles standard USB requests (Chapter 9)
- Manages device configuration and enumeration
- Located in ``src/device/``

**Class Drivers**:
- Implement USB class specifications
- Handle class-specific requests and data transfer
- Provide application APIs
- Located in ``src/class/*/``

Data Flow
---------

**Control Transfers (Setup Requests)**:

.. code-block:: none

   USB Bus → DCD → USBD Core → Class Driver → Application
                     ↓
              Standard requests handled in core
                     ↓
           Class-specific requests → Class Driver

**Data Transfers**:

.. code-block:: none

   Application → Class Driver → USBD Core → DCD → USB Bus
   USB Bus → DCD → USBD Core → Class Driver → Application

Event Processing
----------------

TinyUSB uses a deferred interrupt model for thread safety:

1. **Interrupt Occurs**: USB hardware generates interrupt
2. **ISR Handler**: ``dcd_int_handler()`` captures event, minimal processing
3. **Event Queuing**: Events queued for later processing
4. **Task Processing**: ``tud_task()`` (called by application code) processes queued events
5. **Callback Execution**: Application callbacks executed in task context

.. code-block:: none

   USB IRQ → ISR → Event Queue → tud_task() → Class Callbacks → Application

Host Stack Architecture
=======================

This section is concerned with the **Host Stack**, i.e., the component of TinyUSB used in USB hosts, managing connected USB devices.

Core Components
---------------

**Host Controller Driver (HCD)**:
- MCU-specific USB host peripheral driver
- Manages USB pipes and data transfers
- Handles host controller hardware
- Located in ``src/portable/VENDOR/FAMILY/``

**USB Host Core (USBH)**:
- Implements USB host functionality
- Manages device enumeration and configuration
- Handles pipe management and scheduling
- Located in ``src/host/``

**Hub Driver**:
- Manages USB hub devices
- Handles port management and device detection
- Supports multi-level hub topologies
- Located in ``src/host/``

Device Enumeration
------------------

The host stack follows USB enumeration process:

1. **Device Detection**: Hub or root hub detects device connection
2. **Reset and Address**: Reset device, assign unique address
3. **Descriptor Retrieval**: Get device, configuration, and class descriptors
4. **Driver Matching**: Find appropriate class driver for device
5. **Configuration**: Configure device and start communication
6. **Class Operation**: Normal class-specific communication

.. code-block:: none

   Device Connect → Reset → Get Descriptors → Load Driver → Configure → Operate

Class Architecture
==================

Common Class Structure
----------------------

All USB classes follow a similar architecture:

**Device Classes**:
- ``*_device.c``: Device-side implementation
- ``*_device.h``: Device API definitions
- Implement class-specific descriptors
- Handle class requests and data transfer

**Host Classes**:
- ``*_host.c``: Host-side implementation
- ``*_host.h``: Host API definitions
- Manage connected devices of this class
- Provide application interface

Class Driver Interface
----------------------

**Required Functions**:
- ``init()``: Initialize class driver
- ``reset()``: Reset class state
- ``open()``: Configure class endpoints
- ``control_xfer_cb()``: Handle control requests
- ``xfer_cb()``: Handle data transfer completion

**Optional Functions**:
- ``close()``: Clean up class resources
- ``sof_cb()``: Start-of-frame processing

Descriptor Management
---------------------

Each class is responsible for:
- **Interface Descriptors**: Define class type and endpoints
- **Class-Specific Descriptors**: Additional class requirements
- **Endpoint Descriptors**: Define data transfer characteristics

Memory Management
=================

Static Allocation Model
-----------------------

TinyUSB uses only static memory allocation; it allocates fixed-size endpoint buffers for each configured endpoint, static buffers for class-specific data handling, a fixed buffer dedicated to control transfers, and static event queues for deferred interrupt processing.

Buffer Management
-----------------

**Endpoint Buffers**:
- Allocated per endpoint at compile time
- Size defined by ``CFG_TUD_*_EP_BUFSIZE`` macros
- Used for USB data transfers

**FIFO Buffers**:
- Ring buffers for streaming data
- Size defined by ``CFG_TUD_*_RX/TX_BUFSIZE`` macros
- Separate read/write pointers

Threading Model
===============

Task-Based Design
-----------------

TinyUSB uses a cooperative task model; it provides main tasks - ``tud_task()`` for device and ``tuh_task()`` for host operation. These tasks must be called regularly (typically less than 1ms intervals) to ensure all USB events are processed in task context, where application callbacks also execute.

RTOS Integration
----------------

**Bare Metal**:
- Application calls ``tud_task()`` in main loop
- No threading primitives needed
- Simplest integration method

**FreeRTOS**:
- USB task runs at high priority
- Semaphores used for synchronization
- Queue for inter-task communication

**Other RTOS**:
- Similar patterns with RTOS-specific primitives
- OSAL layer abstracts RTOS differences

Interrupt Handling
------------------

**Interrupt Service Routine**:
- Minimal processing in ISR
- Event capture and queuing only
- Quick return to avoid blocking

**Deferred Processing**:
- All complex processing in task context
- Thread-safe access to data structures
- Application callbacks in known context

Memory Usage Patterns
---------------------

**Flash Memory**:
- Core stack: 8-15KB depending on features
- Each class: 1-4KB additional
- Portable driver: 2-8KB depending on MCU

**RAM Usage**:
- Core stack: 1-2KB
- Endpoint buffers: User configurable
- Class buffers: Depends on configuration

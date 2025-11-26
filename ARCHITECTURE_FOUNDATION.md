# QARMA Architecture Foundation
**Status Assessment & Hardening Plan**

## Design Principles (Your Requirements)

### Control Architecture
1. **Message-Based Communication** - All controls get/send data via message system
2. **Lifecycle Management** - Closeable, appearance modifiable
3. **Visual Properties** - Background transparency modifiable
4. **Containment Model** - Must be contained in frames, cannot stand alone
5. **Frame System** - Main desktop frame contains entire display area
6. **Frame Parameters** - User-controllable frame parameters
7. **Dialog System** - Mini-windows with controls
8. **Pre-built Dialogs** - Basic dialogs ready to use
9. **Custom Dialogs** - User-defined dialog creation
10. **Universal Handles** - Every entity gets unique handle

### Handle & Priority System
- **Handle Assignment** - Unique handle for every: Control, Window, Dialog, Message, Task
- **Message Types** - Read-only or Read/Write classification
- **Priority Bits** - Priority system for message/task routing

---

## Current System Status

### ✅ **ROBUST Systems**

#### 1. CPU Core Manager (`kernel/core/core_manager.c`)
**Status: PRODUCTION READY**
```
Features:
- Subsystem identification (8 subsystems)
- Core allocation with NUMA awareness
- Min/max core policies per subsystem
- Core sharing and preemption
- Load balancing and statistics
- Priority-based allocation

Subsystems Supported:
- SUBSYSTEM_KERNEL, AI, QUANTUM, PARALLEL
- SECURITY, VIDEO, IO, NETWORK
```

**API Surface:**
```c
core_manager_init()
core_request_allocate(request)
core_release(subsystem, core_id)
core_pin_task(core_id, function, data)
core_get_available_count(subsystem)
core_request_shared(subsystem, core_id)
```

#### 2. Task Manager (`kernel/core/scheduler/task_manager.c`)
**Status: PRODUCTION READY**
```
Features:
- 5 priority levels (CRITICAL to IDLE)
- Full CPU context switching (64-bit)
- Parent/child task relationships
- Task states (CREATED, READY, RUNNING, BLOCKED, SLEEPING, TERMINATED)
- Quantum AI execution tracking
- Time slicing and scheduling
- Stack management per task
```

**API Surface:**
```c
task_manager_init()
task_create(name, entry, data, priority, flags)
task_start(task), task_terminate(task)
task_schedule(), task_yield()
task_sleep(ms), task_wake(task)
task_current(), task_find_by_id(id)
task_set_priority(task, priority)
```

#### 3. Subsystem Registry (`kernel/core/scheduler/subsystem_registry.c`)
**Status: PRODUCTION READY**
```
Features:
- Named subsystem registration
- Message handler registration per subsystem
- Status tracking (INACTIVE, INITIALIZING, ACTIVE, ERROR, SHUTTING_DOWN)
- Statistics tracking
- Health monitoring
```

**API Surface:**
```c
subsystem_registry_init()
subsystem_register(name, init_func, shutdown_func)
subsystem_set_message_handler(id, handler)
subsystem_get_by_id(id)
subsystem_get_by_name(name)
```

---

### ⚠️ **NEEDS HARDENING**

#### 1. Input Event System (`kernel/qarma_win_handle/qarma_input_events.c`)
**Status: PARTIALLY COMPLETE**

**Current State:**
```
✓ Event listener registration
✓ Priority-based dispatch
✓ Target filtering
✓ Event queue (64 events)
✓ Event types: MOUSE, KEY, WINDOW, SYSTEM, CONTROL, CUSTOM
✗ No unique handle assignment
✗ No read/write permission model
✗ No message routing beyond event dispatch
✗ Controls don't register with system automatically
```

**Issues:**
- Events are dispatched but not true "messages"
- No handle-based addressing
- No permission/access control
- Static pool of 64 listeners (might be insufficient)

**Needs:**
1. Convert to handle-based message system
2. Add message type classification (READ, READ_WRITE)
3. Add priority bits to messages
4. Add automatic control registration
5. Add message queuing per recipient
6. Add message acknowledgment system

#### 2. Window/Control System
**Status: FRAGMENTED**

**Current State:**
```
✓ Compositor windows (window_compositor.c)
✓ Console window (console_compositor.c)
✓ Controls: Button, Label, TextBox, Menu, CloseButton, ScrollBar
✗ Controls don't have unique handles
✗ No unified message routing to controls
✗ No transparency control API
✗ No parent-child containment enforcement
✗ Controls render directly, not message-driven
```

**Control Files:**
```
kernel/gui/controls/
  ├── button.c
  ├── label.c
  ├── textbox.c
  ├── menu.c
  ├── close_button.c
  └── scrollbar.c
```

**Issues:**
- Controls use direct function calls, not messages
- No standardized lifecycle management
- Transparency is per-window, not per-control
- Parent-child relationships not enforced
- No dialog system

---

## Architecture Gaps

### Missing Components

#### 1. **Message System** (Critical Priority)
```
Required:
- Handle-based addressing (HANDLE type)
- Message types: WM_CREATE, WM_DESTROY, WM_PAINT, WM_INPUT, etc.
- Message queue per window/control
- Message priority levels
- Read-only vs Read/Write messages
- Broadcast messages
- Reply/acknowledgment system
```

#### 2. **Handle Manager** (Critical Priority)
```
Required:
- Central handle allocation
- Handle types: WINDOW, CONTROL, DIALOG, TASK, MESSAGE
- Handle validation
- Handle-to-object mapping
- Handle recycling on destroy
```

#### 3. **Frame/Container System** (High Priority)
```
Required:
- Frame base class
- Desktop frame (full screen)
- Window frames
- Dialog frames
- Panel frames (sub-containers)
- Parent-child enforcement
- Containment validation
```

#### 4. **Dialog System** (High Priority)
```
Required:
- Dialog base class (mini-window)
- Pre-built dialogs:
  - Message box (OK, OK/Cancel, Yes/No)
  - Input dialog (text entry)
  - File dialog
  - Color picker
  - Progress dialog
- Custom dialog builder API
```

#### 5. **Control Lifecycle Manager** (High Priority)
```
Required:
- Control registration/unregistration
- Automatic message routing
- Z-order management within parent
- Focus management
- Enable/disable state
- Visibility management
- Transparency control per control
```

---

## Recommended Implementation Plan

### Phase 1: Core Message System (Week 1)
**Files to Create:**
```
headers/core/message_system.h
kernel/core/message_system.c
headers/core/handle_manager.h
kernel/core/handle_manager.c
```

**Key Structures:**
```c
typedef uint64_t QARMA_HANDLE;

typedef enum {
    HANDLE_TYPE_WINDOW = 0x01000000,
    HANDLE_TYPE_CONTROL = 0x02000000,
    HANDLE_TYPE_DIALOG = 0x03000000,
    HANDLE_TYPE_TASK = 0x04000000,
    HANDLE_TYPE_MESSAGE = 0x05000000,
} handle_type_t;

typedef enum {
    MSG_ACCESS_READ = 0x01,
    MSG_ACCESS_WRITE = 0x02,
    MSG_ACCESS_BROADCAST = 0x04,
} message_access_t;

typedef struct {
    QARMA_HANDLE msg_id;          // Unique message handle
    QARMA_HANDLE sender;          // Sender handle
    QARMA_HANDLE target;          // Target handle (or 0 for broadcast)
    uint32_t message_type;        // WM_* message type
    uint32_t priority;            // 0=highest
    uint32_t access_flags;        // Read/Write permissions
    uint64_t timestamp;
    void* data;                   // Message-specific data
    size_t data_size;
    bool requires_reply;
    QARMA_HANDLE reply_to;        // Original message if this is reply
} qarma_message_t;

// API
qarma_handle_t handle_allocate(handle_type_t type, void* object);
void handle_release(qarma_handle_t handle);
void* handle_get_object(qarma_handle_t handle);
bool handle_validate(qarma_handle_t handle, handle_type_t expected_type);

bool message_send(qarma_message_t* msg);
bool message_post(qarma_message_t* msg);  // Async
qarma_message_t* message_receive(qarma_handle_t recipient, bool blocking);
bool message_reply(qarma_message_t* original, void* reply_data, size_t size);
```

### Phase 2: Frame & Container System (Week 2)
**Files to Create:**
```
headers/gui/frame.h
kernel/gui/frame.c
headers/gui/desktop_frame.h
kernel/gui/desktop_frame.c
```

**Key Features:**
- Desktop frame covers entire display
- Window frames are child containers
- Controls must have parent frame
- Frame parameters: border, title bar, resize handles
- Containment validation on control add

### Phase 3: Control Hardening (Week 2-3)
**Files to Modify:**
```
kernel/gui/controls/*.c - Add message handling
headers/gui/controls/*.h - Add handle members
```

**Changes Needed:**
- Add QARMA_HANDLE to each control structure
- Replace direct function calls with message_send()
- Add message handlers for WM_PAINT, WM_INPUT, etc.
- Add transparency property to each control
- Register controls with message system on creation

### Phase 4: Dialog System (Week 3)
**Files to Create:**
```
headers/gui/dialog.h
kernel/gui/dialog.c
headers/gui/dialogs/message_box.h
kernel/gui/dialogs/message_box.c
headers/gui/dialogs/input_dialog.h
kernel/gui/dialogs/input_dialog.c
```

**Features:**
- Modal and modeless dialogs
- Message loop for modal dialogs
- Pre-built dialog templates
- Custom dialog builder

### Phase 5: Integration & Testing (Week 4)
- Convert existing windows to use message system
- Add handle tracking to all UI objects
- Implement transparency controls
- Test containment enforcement
- Performance testing and optimization

---

## API Design Preview

### Message System Usage Example
```c
// Create a button control
button_t* btn = button_create(parent_frame, 10, 10, 100, 30, "Click Me");
QARMA_HANDLE btn_handle = btn->base.handle;  // Auto-assigned

// Send message to button
qarma_message_t msg = {
    .sender = desktop_handle,
    .target = btn_handle,
    .message_type = WM_SETTEXT,
    .priority = MSG_PRIORITY_NORMAL,
    .access_flags = MSG_ACCESS_WRITE,
    .data = "New Text",
    .data_size = 9,
    .requires_reply = false
};
message_send(&msg);

// Button processes in its message loop
void button_message_handler(button_t* btn, qarma_message_t* msg) {
    switch (msg->message_type) {
        case WM_SETTEXT:
            strncpy(btn->text, msg->data, sizeof(btn->text));
            message_send_simple(btn->base.handle, btn->base.parent, WM_INVALIDATE);
            break;
        case WM_PAINT:
            button_render(btn);
            break;
        case WM_MOUSE_DOWN:
            if (btn->on_click) btn->on_click(btn->userdata);
            break;
    }
}
```

### Frame Usage Example
```c
// Desktop frame created at init
desktop_frame_t* desktop = desktop_frame_create();
desktop->base.handle = handle_allocate(HANDLE_TYPE_WINDOW, desktop);

// Create window frame as child of desktop
window_frame_t* win = window_frame_create(
    desktop->base.handle,  // Parent
    100, 100,              // Position
    400, 300,              // Size
    "My Window"            // Title
);

// Add control to window (validates parent)
button_t* btn = button_create(win->base.handle, 10, 10, 100, 30, "OK");
// Automatically registered and gets handle

// Trying to add orphan control fails
button_t* orphan = button_create(NULL, 10, 10, 100, 30, "Orphan");
// Returns NULL or error - controls must have parent frame
```

---

## Next Steps

1. **Review & Approve** this architecture plan
2. **Create** message_system.h and handle_manager.h headers
3. **Implement** core handle allocation system
4. **Implement** message queue and dispatch
5. **Convert** one control (button) to message-based as proof-of-concept
6. **Test** message routing and handle validation
7. **Iterate** on remaining controls and windows

---

## Questions for Consideration

1. **Message Queue Size**: How many messages should each control queue?
2. **Handle Format**: 64-bit with embedded type, or separate type tracking?
3. **Priority Levels**: How many priority levels do we need?
4. **Transparency**: Alpha per control or per layer?
5. **Frame Nesting**: Maximum nesting depth for frames?
6. **Dialog Modality**: Block entire system or just parent window?
7. **Message Threading**: Single-threaded dispatch or multi-threaded?
8. **Performance**: Benchmark message overhead vs direct calls

---

*This document represents the current state and proposed architecture based on your requirements. All "robust" systems are ready for integration, while "needs hardening" systems require the additions outlined above.*

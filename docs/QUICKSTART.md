# QARMA - Quick Start Guide

## Testing the AI Command Predictor

### Boot the OS

```bash
cd /home/dmort/qarma
make qemu
```

### Available Commands

Once booted, try these commands in the QARMA shell:

#### View AI Statistics
```
aistats
```
Shows:
- Total predictions attempted
- Cache hits and misses
- Cache hit rate (%)
- Current cache size

#### Clear AI Cache
```
aiclear
```
Resets all cached predictions

#### View All Commands
```
help
```
Shows all available shell commands including the new AI commands

### Current Subsystems

The OS now has 6 registered subsystems:

1. **AI** (ID: 1) - Command prediction, monitoring
2. **Quantum** (ID: 2) - Quantum process management
3. **Parallel** (ID: 3) - Multi-core processing
4. **Security** (ID: 4) - Security monitoring
5. **Video** (ID: 5) - Graphics subsystem
6. **Filesystem** (ID: 6) - VFS and file operations

### Testing Checklist

- [ ] Boot OS successfully
- [ ] Run `help` - verify `aistats` and `aiclear` listed
- [ ] Run `aistats` - should show zero statistics
- [ ] Look for subsystem registration messages during boot
- [ ] Verify no ID collision warnings
- [ ] Run `ls` command
- [ ] Run `pwd` command
- [ ] Test filesystem commands (mkdir, touch, cat, etc.)

### Expected Boot Messages

You should see these messages during boot:

```
Initializing AI subsystem...
AI subsystem registered with task manager
[AI] Command predictor initialized
AI hardware acceleration detected.
AI subsystem initialized.

[VFS] Starting VFS initialization...
[VFS] Registered with subsystem registry

Initializing quantum kernel...
Quantum subsystem registered with task manager

Initializing parallel engine...
Parallel subsystem registered with task manager

Initializing Security Manager...
Security subsystem registered with task manager
```

### Phase 2 Preview

Phase 2 will integrate the prediction system with actual command execution:

1. Run `ls` multiple times → Second time uses cached result
2. Run `help` → Cached after first use
3. `aistats` shows increasing hit count
4. Visible performance improvement

### Troubleshooting

**Q: Commands not showing in help?**  
A: Rebuild with `make clean && make`

**Q: aistats shows no data?**  
A: This is expected in Phase 1 - cache is not yet integrated with command execution

**Q: Subsystem registration failed?**  
A: Check for ID collisions in boot messages

**Q: OS won't boot?**  
A: Check that qarma.iso was built successfully: `ls -lh build/qarma.iso`

### Documentation

For more details, see:
- `docs/VISION.md` - Overall project vision and roadmap
- `docs/AI_COMMAND_PREDICTOR.md` - Detailed predictor documentation
- `docs/SESSION_SUMMARY.md` - What was implemented in this session

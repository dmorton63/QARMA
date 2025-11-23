# Nomain IDE

A lightweight integrated development environment for the nomain programming language, built natively for QARMA OS.

## Features

- **Text Editor**: Multi-line code editor with syntax highlighting
- **Compiler Integration**: Direct compilation of nomain programs
- **Error Display**: Visual display of compilation errors with line numbers
- **File Management**: Open, save, and manage nomain source files
- **Program Execution**: Run compiled programs within the IDE

## Architecture

### Components

1. **Editor Core** (`editor/`)
   - Text buffer management
   - Cursor and selection handling
   - Undo/redo functionality
   - Line numbering

2. **Syntax Highlighter** (`syntax/`)
   - Nomain language parser
   - Token identification
   - Color scheme management

3. **Compiler Interface** (`compiler/`)
   - Nomain compiler integration
   - Build configuration
   - Error parsing

4. **UI Framework** (`ui/`)
   - Main window
   - Menu bar
   - Toolbar
   - Status bar
   - Split panes (editor/output)

5. **File System** (`filesystem/`)
   - File browser
   - Project management
   - Recent files

## Nomain Language Support

The IDE is specifically designed for the nomain language features:
- No main() function requirement
- Automatic execution flow
- Unique syntax highlighting for nomain keywords
- Context-aware code completion (future)

## Development Roadmap

### Phase 1: Basic Editor (Current)
- [ ] Text editor widget
- [ ] Basic file I/O
- [ ] Syntax highlighting
- [ ] Compile button

### Phase 2: Enhanced Features
- [ ] Error navigation
- [ ] Line numbers
- [ ] Search/Replace
- [ ] Multiple file tabs

### Phase 3: Advanced Features
- [ ] Code completion
- [ ] Visual debugger
- [ ] Project templates
- [ ] Integrated terminal

## Building

The IDE will be built as part of the QARMA kernel and accessible through the desktop environment.

## Usage

Launch from QARMA shell:
```
ide [filename.nm]
```

Or from the desktop menu when GUI is fully integrated.

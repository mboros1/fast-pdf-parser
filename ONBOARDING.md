# Worker Onboarding

Welcome to your project workspace! This guide will help you get started with your assignment.

## Your Workspace

- **Working Directory**: THIS directory - always start Claude Code here!
- **Branch**: `feature/core-implementation`
- **Remote**: `https://github.com/mboros1/fast-pdf-parser.git`

⚠️ **Critical**: See `assistant/WORKER_SETUP_GUIDE.md` (in this directory) for detailed setup instructions to ensure proper context isolation.

## Getting Started

### Step 1: Review Your Assignment
Your assignment is in the PR description at: [PR will be created after first push]

### Step 2: Set Up Your Environment
```bash
# Add project-specific build commands
```
<!-- ORCHESTRATOR:APPEND:after_setup -->

### Step 3: Set Up Your CLAUDE.md
You own the CLAUDE.md file in this repository. It's your personal knowledge base:

1. CLAUDE.md is gitignored - it stays in your working directory
2. Customize it with your understanding of the project and assignment
3. Let it evolve naturally as you implement and learn
4. No complex branch management needed - simple and effective

**CLAUDE.md Management**:
- Single file in repository root
- Automatically excluded from git commits  
- Grows with your expertise on this project
- Persistent across all your work on this repository

<!-- ORCHESTRATOR:APPEND:before_communication_protocol -->
## Autonomous Execution

### Your Assignment
You have everything needed for complete autonomous execution:

- **Comprehensive Research**: Orchestrator spent 15+ minutes researching domain patterns
- **Technical Context**: ONBOARDING.md contains all implementation guidance  
- **Success Criteria**: Clear, measurable requirements in PR description
- **Domain Expertise**: Accumulated research findings and best practices

### Minimal Communication
Operate autonomously with rare communication:

**Implementation Complete**:
```markdown
## ✅ Implementation Complete

**Delivered**:
- [All requirements met]
- [Performance targets achieved]  
- [Tests passing]

**Key Decisions**:
- [Technical choice 1]: [Rationale]
- [Technical choice 2]: [Rationale]

Ready for merge.
```

**Critical Blockers Only** (rare):
```markdown
## Critical Blocker

**Issue**: [Fundamental requirement conflict discovered]
**Context**: [Why this cannot be resolved autonomously]
**Options**: [Potential paths forward]

Guidance needed to proceed.
```

## Development Approach

### Self-Sufficient Execution
1. Read ONBOARDING.md completely before starting
2. Understand success criteria and technical requirements  
3. Implement with full autonomy
4. Validate against all requirements
5. Deliver complete solution
<!-- ORCHESTRATOR:APPEND:after_development_workflow -->

### Before Marking Complete
- [ ] All acceptance criteria met
- [ ] Tests passing
- [ ] Code follows project conventions
- [ ] Documentation updated
- [ ] CLAUDE.md reflects current state
- [ ] Final "Ready for Review" comment posted

## Important Notes

### DO NOT Commit
- This ONBOARDING.md file
- CLAUDE.md (automatically gitignored)
- Temporary or workflow files

### DO Commit
- All code changes
- Documentation updates
- Test files
- Configuration changes

## Context Management

### Saving Context
Use the `/compact` command regularly to save your working context:
```
/compact
```

This creates a snapshot in `.claude_context/` that can be restored later.

### Restoring Context
If you need to resume work after a break, your context files help you quickly get back up to speed.

## Communication Modes

You must follow these communication modes (check your local `assistant/` directory):

### Sharp Mode
**Use for**: All conversational interactions, discussions, and problem-solving. Provide direct feedback, critical analysis, and honest assessments. Clearly signal uncertainty levels and ask clarifying questions.

### Absolute Mode  
**Use for**: All documentation writing. Write concisely without decorative language, emotional appeals, or unnecessary transitions.

**Location**: These mode definitions are in your project repository at:
- `assistant/sharp_mode.txt`
- `assistant/absolute_mode.txt`

## Recommended: MCP Setup

Consider setting up MCP tools for enhanced functionality:
- Vector search capabilities for codebase exploration
- Advanced code analysis tools
- Performance profiling utilities

Check if MCP configuration is available in your repository.

## Need Help?

Only for critical blockers that prevent autonomous execution. Post a comment on your PR describing the fundamental issue.

---

*You have everything needed for autonomous execution. Trust the research and deliver complete solutions.*
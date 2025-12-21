# Rule: Generating a Task Details Document

## Goal

To guide an AI assistant (or developer) in creating a detailed, actionable Task Details document in Markdown format for a specific task or sub-task. This document should provide enough technical and design detail for a junior developer to confidently implement the solution.

## Process

1. **Receive Task Reference:** The user provides a specific task or sub-task from the task list.
2. **Ask Clarifying Questions:** Before writing the details, the AI *must* ask clarifying questions to gather sufficient context (e.g., about design choices, constraints, integration points, edge cases).
3. **Generate Task Details:** Based on the user’s answers and the referenced task, generate a document using the structure below.
4. **Save Task Details:** Save the document as `[PREFIX]-[task-id]-details.md` inside the `/tasks` directory. Where [PREFIX] comes from the tasks md file the task is generated from.

## Clarifying Questions (Examples)

- What is the expected input/output for this task?
- Are there any specific design patterns or architectural constraints to follow?
- Should this task integrate with any existing modules or services?
- Are there performance, security, or error-handling requirements?
- What are the edge cases or failure modes to consider?
- Should the implementation be extensible for future requirements?

## Task Details Structure

The generated document should include the following sections:

1. **Task Reference:** The task or sub-task this document addresses (e.g., “2.2 Implement ThrottlePositionHandler”).
2. **Objective:** A brief summary of what this task aims to accomplish.
3. **Design Overview:** High-level description of the approach, including diagrams or pseudocode if helpful.
4. **Implementation Steps:** Step-by-step instructions for implementing the task, including:
    - File(s) to create or modify
    - Key classes, functions, or data structures
    - Integration points with other modules
    - Example code snippets
5. **Edge Cases & Error Handling:** List of edge cases, error conditions, and how to handle them.
6. **Testing Strategy:** How to test the implementation (unit tests, integration tests, manual steps).
7. **Dependencies & Integration:** Any dependencies on other tasks, modules, or external systems.
8. **Open Questions:** Any remaining uncertainties or decisions to be made.

## Output

- **Format:** Markdown (`.md`)
- **Location:** `/tasks/[EPIC]`
- **Filename:** `[PREFIX]-DETAILS-[task-id].md` (e.g., `INSUB-DETAILS-2-2.md`)

## Final Instructions

- Do NOT start implementing the task until the Task Details document is complete and reviewed.
- Make sure to ask clarifying questions before writing the document.
- Use clear, explicit language suitable for a junior developer.

---

**Example Usage:**  
- Reference this template when a task or sub-task requires a deep dive into design and implementation details.

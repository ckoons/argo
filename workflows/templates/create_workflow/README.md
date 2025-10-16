# create_workflow

## Description

Meta-workflow that creates new workflow templates through an interactive dialog with the user. This workflow guides users through defining requirements, refining the design, and generating a complete directory-based workflow template.

## Success Criteria

- User provides clear requirements (what, testing, success)
- Design is validated through confirmation dialog
- Complete workflow directory structure is created
- All required files are generated (workflow.sh, metadata.yaml, README.md)
- Test template is created
- Template is stored in `~/.argo/workflows/templates/`

## Testing

Test by running the workflow and verifying:
1. Interactive prompts display correctly
2. User input is captured
3. Template directory is created
4. All files are generated with correct structure
5. Generated template passes `arc workflow test`

## Usage

```bash
# Interactive mode
arc workflow start create_workflow my_new_template

# The workflow will guide you through:
# 1. User onboarding (explains the process)
# 2. Requirements gathering (3 questions + optional context)
# 3. Design dialog (confirm/refine approach)
# 4. Build phase (generates template)
```

## Process Flow

### Phase 1: User Onboarding
Explains the 3-phase process and sets expectations.

### Phase 2: Requirements Gathering
Asks 4 questions:
1. What do you want this workflow to do?
2. How will you test if it works?
3. What does success look like?
4. Any additional context?

### Phase 3: Design Dialog
Summarizes requirements and asks for confirmation. If not confirmed, loops back to requirements gathering.

### Phase 4: Build Phase
1. Prompts for template name
2. Creates directory structure
3. Generates workflow.sh with TODO placeholders
4. Creates metadata.yaml with collected info
5. Generates README.md with requirements
6. Creates test_basic.sh template
7. Creates config/defaults.env
8. Displays next steps for user

## Output Structure

Creates a directory at `~/.argo/workflows/templates/{template_name}/`:

```
{template_name}/
├── workflow.sh           # Executable script with TODOs
├── metadata.yaml         # Workflow metadata
├── README.md             # Documentation
├── tests/
│   └── test_basic.sh     # Basic test template
└── config/
    └── defaults.env      # Configuration template
```

## Prompts

This workflow uses external prompt files:
- `prompts/user_onboarding.txt` - Introduction text
- `prompts/requirements_questions.txt` - The 4 questions
- `prompts/design_summary.txt` - Confirmation template
- `prompts/ci_onboarding.txt` - AI system instructions (for future CI use)

## Requirements

- bash shell
- `read` built-in (for user input)
- `mkdir` (for directory creation)
- Write access to `~/.argo/workflows/templates/`

## Next Steps After Creation

After this workflow creates a template, the user should:

1. **Implement logic**: Edit `{template_name}/workflow.sh` and replace TODOs
2. **Write tests**: Edit `{template_name}/tests/test_basic.sh`
3. **Test it**: Run `arc workflow test {template_name}`
4. **Use it**: Run `arc workflow start {template_name} instance_name`

## Future Enhancements

- AI-assisted implementation generation
- Template validation before creation
- Support for complex parameters
- Template versioning
- Template repository integration

## Author

Created by Casey Koons on 2025-10-15

---
Meta-workflow for creating workflow templates

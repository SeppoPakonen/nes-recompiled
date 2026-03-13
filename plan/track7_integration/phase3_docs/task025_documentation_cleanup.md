# Task 025: Documentation and Cleanup

## Objective
Update documentation and clean up temporary files.

## Documentation Updates

### 1. Update main README.md
Add sections for:
- Test ROMs usage
- Debug port documentation
- Feature coverage summary
- Known issues and workarounds

### 2. Create debugging guide
File: `docs/DEBUGGING.md`

Contents:
- How to use test ROMs
- How to interpret debug output
- Common issues and fixes
- Analyzer troubleshooting
- Trace-guided analysis workflow

### 3. Update COMPATIBILITY.md
- Add test ROM results
- Add a.nes results
- Add Parasol Stars results
- Create compatibility table

### 4. Update QWEN.md
- Add new test ROMs to workflow
- Add debug port usage
- Add troubleshooting section

## Cleanup Tasks

### 1. Remove old output directories
```bash
rm -rf output/a output/a_test output/a_new output/a_final
rm -rf output/test01 output/test02 output/test03
rm -rf output/test01_fixed output/test02_fixed output/test03_fixed
```

### 2. Archive debug files
```bash
mkdir -p docs/debug_archive
mv debug_frame.ppm debug_frame.png docs/debug_archive/ 2>/dev/null || true
```

### 3. Clean up testroms/build/
```bash
rm -rf testroms/build/*.o  # Keep .nes files, remove intermediates
```

### 4. Update .gitignore if needed
Ensure these are ignored:
- output/
- logs/
- tmp/
- testroms/build/*.o
- *.trace
- *.log

## Acceptance Criteria
- [ ] README.md updated with test ROM section
- [ ] docs/DEBUGGING.md created
- [ ] COMPATIBILITY.md has test results
- [ ] QWEN.md updated
- [ ] Old output directories removed
- [ ] Debug files archived
- [ ] .gitignore complete

## Dependencies
- Task 021 (test ROMs working)
- Task 022 (real ROM testing)

## Estimated Effort
4-8 hours

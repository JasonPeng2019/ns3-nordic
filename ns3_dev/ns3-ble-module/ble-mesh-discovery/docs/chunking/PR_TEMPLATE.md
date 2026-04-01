# Chunk PR Template

## Chunk
- [ ] ch01
- [ ] ch02
- [ ] ch03
- [ ] ch04
- [ ] ch05
- [ ] ch06
- [ ] ch07
- [ ] ch08

## Ownership Rules
- Touch only one chunk plus `model/shared` or agreed migration files.
- No imports from future chunks.
- Do not modify `deferred/lora` unless the PR is explicitly LoRA-scoped.

## Gates
- [ ] Boundary guard passes (`tools/validate_chunk_boundaries`)
- [ ] Build passes
- [ ] Chunk-local tests pass
- [ ] All previous chunk gates remain green

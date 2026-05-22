# Memory Layout Notes

## Overall Process Memory Layout (x86-64 Linux)

```
Low addresses                                         High addresses
┌──────────┬──────────────┬────────────┬─  ~gap  ─┬──────────────┐
│   Code   │   Globals    │    Heap    │           │    Stack     │
│  (.text) │ (.data/.bss) │     →      │           │      ←       │
└──────────┴──────────────┴────────────┴───────────┴──────────────┘
 ~0x55...                   grows UP      ~40+ TB    grows DOWN
                                          gap        ~0x7ffc...
```

- **Code (.text)**: program instructions (read-only, executable)
- **Globals (.data/.bss)**: global/static variables
- **Heap**: dynamically allocated memory (`malloc`), grows toward higher addresses
- **Stack**: local variables, return addresses, function call frames, grows toward lower addresses

## Stack Frame Layout (per function call)

Example: `int test(int depth)` — each recursive call uses **32 bytes**:

```
High address (caller's frame)
┌────────────────────────────────┐
│ return address   (8 bytes)     │  ← pushed by `call` instruction
├────────────────────────────────┤
│ saved frame pointer %rbp (8 B) │  ← pushed by `push %rbp`
├────────────────────────────────┤  ← %rbp points here
│ depth (int)      (4 bytes)     │  ← at -0x4(%rbp)
│ padding          (12 bytes)    │  ← unused (16-byte alignment)
└────────────────────────────────┘  ← %rsp after `sub $0x10, %rsp`
Low address (next call goes here)
```

Stack grows **downward** — each nested call gets a lower address.

## Observed Per-Call Overhead

| Parameters       | Local space (`sub`) | Total per call | Wasted on padding |
|------------------|---------------------|----------------|-------------------|
| 1 int (4 bytes)  | 16 bytes            | 32 bytes       | 12 bytes          |
| 4 ints (16 bytes)| 16 bytes            | 32 bytes       | 0 bytes           |
| 5 ints (20 bytes)| 32 bytes            | 48 bytes       | 12 bytes          |

The compiler allocates local space in **16-byte increments** to maintain stack alignment (x86-64 ABI requirement).

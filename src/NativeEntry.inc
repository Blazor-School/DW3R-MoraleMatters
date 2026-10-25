// The native setter leaves R9-R11 and XMM0-XMM5 untouched. Optimized direct
// callers rely on that stronger contract (e.g. the R9 loop at RVA 0x13C00B0).
// A C++ hook alone follows only the public ABI and can corrupt those callers.
extern "C" void MoraleMattersSpecialDispatch(void* group, uint8_t value) { SpecialAttribute(group, value); }
extern "C" void MoraleMattersSpecialEntry(void* group, uint8_t value);
asm(
    ".text\n"
    ".p2align 4\n"
    ".seh_proc MoraleMattersSpecialEntry\n"
    "MoraleMattersSpecialEntry:\n"
    "subq $0xa8, %rsp\n"
    ".seh_stackalloc 0xa8\n"
    ".seh_endprologue\n"
    "movq %r9, 0x20(%rsp)\n"
    "movq %r10, 0x28(%rsp)\n"
    "movq %r11, 0x30(%rsp)\n"
    "movaps %xmm0, 0x40(%rsp)\n"
    "movaps %xmm1, 0x50(%rsp)\n"
    "movaps %xmm2, 0x60(%rsp)\n"
    "movaps %xmm3, 0x70(%rsp)\n"
    "movaps %xmm4, 0x80(%rsp)\n"
    "movaps %xmm5, 0x90(%rsp)\n"
    "call MoraleMattersSpecialDispatch\n"
    "movq 0x20(%rsp), %r9\n"
    "movq 0x28(%rsp), %r10\n"
    "movq 0x30(%rsp), %r11\n"
    "movaps 0x40(%rsp), %xmm0\n"
    "movaps 0x50(%rsp), %xmm1\n"
    "movaps 0x60(%rsp), %xmm2\n"
    "movaps 0x70(%rsp), %xmm3\n"
    "movaps 0x80(%rsp), %xmm4\n"
    "movaps 0x90(%rsp), %xmm5\n"
    "addq $0xa8, %rsp\n"
    "ret\n"
    ".seh_endproc\n"
);

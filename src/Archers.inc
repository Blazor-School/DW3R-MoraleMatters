// Shared by targeting and damage; named officers and crossbowmen are excluded.
static bool ArcherTroop(void* character)
{
    // Called with a native weak-owner result or a live hit-component owner.
    // Formation storage is already validated; avoid VirtualQuery on every hit.
    if (!character || At<uint8_t>(character, 0x74C) < 2 || At<uint8_t>(character, 0x787))
        return false;
    void* group = Group(At<int>(character, 0x7A0));
    return group && At<uint8_t>(group, 0x2D21) == 2 && At<uint8_t>(group, 0x2D20) >= 6 && GroupAlive(group) && !At<uint8_t>(group, 0x191) && !At<uint8_t>(group, 0x198);
}

using ArcherPriorityFn = void (*)(void*, void*);
using ArcherResolveFn = bool (*)(void*, int);
using ArcherAssignFn = int (*)(void*, int, void*);
using ArcherTargetFn = void* (*)(void*);
using ArcherRangeFn = bool (*)(void*, void*);
using ArcherSetFn = bool (*)(void*, void*);
static ArcherPriorityFn OriginalArcherPriority = nullptr;
static ArcherResolveFn OriginalArcherResolve = nullptr;
static ArcherAssignFn OriginalArcherAssign = nullptr;
static ArcherTargetFn ArcherTarget = nullptr;
static ArcherRangeFn ArcherInRange = nullptr;
static ArcherSetFn ArcherSetTarget = nullptr;
struct ArcherSearch { void* controller; void* owner; ULONGLONG retry; };
static ArcherSearch ArcherSearches[16384];
static uint32_t ArcherRandomState = 0xA341316C;

static void ResetArchers() { memset(ArcherSearches, 0, sizeof(ArcherSearches)); }

static uint32_t ArcherRandomBelow(uint32_t count)
{
    // Separate from the game's RNG: target selection does not change drop rolls.
    const uint32_t threshold = static_cast<uint32_t>(-count) % count;
    uint32_t value;
    do
    {
        ArcherRandomState ^= ArcherRandomState << 13;
        ArcherRandomState ^= ArcherRandomState >> 17;
        ArcherRandomState ^= ArcherRandomState << 5;
        value = ArcherRandomState;
    } while (value < threshold);
    return value % count;
}

static void* RandomArcherOwner(void* controller)
{
    if (!Config.archerRandomTargets || !controller || !BehaviorOwner)
        return nullptr;
    void* owner = BehaviorOwner(static_cast<uint8_t*>(controller) + 0x3DC);
    if (!ArcherTroop(owner))
        return nullptr;
    void* group = Group(At<int>(owner, 0x7A0));
    const int status = At<uint8_t>(controller, 0x3EC);
    // Strategic/scripted orders and evacuation remain authoritative.
    if (At<int>(group, 0x178) != 2 || status < 5 || status > 6 || At<uint8_t>(group, 0x2DFD) != 0 || At<uint8_t>(group, 0x2DFC) == 4 || At<uint8_t>(group, 0x2E3A) || At<uint8_t>(group, 0x2E58) || !At<void*>(controller, 0x3A0) || BehaviorStopped(controller, At<void*>(controller, 0x3A0), 24))
        return nullptr;
    return owner;
}

static bool ArcherCandidate(void* controller, void* owner, void* candidate)
{
    if (candidate == owner || !Readable(candidate, 0x8B2) || (At<uint32_t>(candidate, 8) & 0x40000000) || At<uint8_t>(candidate, 0x787))
        return false;
    void* group = Group(At<int>(candidate, 0x7A0));
    void* ownGroup = Group(At<int>(owner, 0x7A0));
    if (!group || !ownGroup || At<int>(group, 0x40) < 0 || At<int>(group, 0x40) > 1 || At<int>(group, 0x40) == At<int>(ownGroup, 0x40) || !GroupAlive(group) || At<uint8_t>(group, 0x191) || At<uint8_t>(group, 0x198))
        return false;
    const int special = At<uint8_t>(group, 0x2DFD);
    if (At<uint8_t>(group, 0x2E56) || (special <= 6 && (0x56 & (1 << special))))
        return false; // Native hidden/reinforcement/script exclusions.
    void* identity = At<void*>(candidate, 0x6C8);
    return Readable(identity, 8) && At<int>(identity, 0) != 0 && ArcherInRange(controller, candidate);
}

static bool SelectRandomArcherTarget(void* controller, void* owner)
{
    void* current = ArcherTarget(controller);
    if (ArcherCandidate(controller, owner, current))
    {
        At<int>(controller, 0x408) = At<int>(current, 0x7A0);
        return true; // Keep a valid target; never reroll on every shot or frame.
    }
    if (current)
        ArcherSetTarget(controller, nullptr);
    At<int>(controller, 0x408) = -1;
    const float previousDistance = At<float>(controller, 0x414);
    const float previousPitch = At<float>(controller, 0x418);
    const uintptr_t key = reinterpret_cast<uintptr_t>(controller) >> 4;
    auto& search = ArcherSearches[(key ^ (key >> 14)) & 16383];
    const ULONGLONG now = GetTickCount64();
    if (search.controller == controller && search.owner == owner && now < search.retry)
        return false;
    search = {controller, owner, now + 500};
    void* manager = At<void*>(Image, 0xA9C50F0); // Native CharacterManager singleton.
    if (!Readable(manager, 0x60))
        return false;
    const int count = At<int>(manager, 0x58);
    auto** characters = At<void**>(manager, 0x50);
    if (count <= 0 || count > 16384 || !Readable(characters, static_cast<size_t>(count) * sizeof(void*)))
        return false;
    int candidates[16384];
    for (int index = 0; index < count; ++index)
        candidates[index] = index;
    uint32_t available = static_cast<uint32_t>(count);
    // Sample a lazy random permutation and stop at the first eligible assignment.
    // This is uniform among eligible actors without a full range scan up front.
    while (available)
    {
        const uint32_t index = ArcherRandomBelow(available);
        void* candidate = characters[candidates[index]];
        candidates[index] = candidates[--available];
        if (ArcherCandidate(controller, owner, candidate) && ArcherSetTarget(controller, candidate))
        {
            ArcherInRange(controller, candidate); // Restore this actor's distance/pitch after scanning.
            At<int>(controller, 0x408) = At<int>(candidate, 0x7A0);
            search.retry = 0;
            return true;
        }
    }
    At<float>(controller, 0x414) = previousDistance;
    At<float>(controller, 0x418) = previousPitch;
    return false;
}

static void ArcherPriority(void* controller, void* group)
{
    if (void* owner = RandomArcherOwner(controller))
    {
        // The native controller tick already validates its current target.
        // Suppressing player priority needs no population scan or range recheck.
        if (!ArcherTarget(controller))
            SelectRandomArcherTarget(controller, owner);
    }
    else
        OriginalArcherPriority(controller, group);
}

static bool ArcherResolve(void* controller, int group)
{
    if (void* owner = RandomArcherOwner(controller))
        return SelectRandomArcherTarget(controller, owner);
    return OriginalArcherResolve(controller, group);
}

static int ArcherAssign(void* controller, int group, void* ownGroup)
{
    if (void* owner = RandomArcherOwner(controller))
    {
        SelectRandomArcherTarget(controller, owner);
        return 2; // Native resolver's normal completion code, including no available actor.
    }
    return OriginalArcherAssign(controller, group, ownGroup);
}

// The shared damage calculation keeps its HP result in XMM6 across this call.
// Update that copy as well as the hit-reaction argument at the verified call site,
// before native guard handling and the final protected-health clamp. Other native
// callers use this function for hit reactions only and must retain their registers.
using ArcherHitFn = bool (*)(void*, void*, float, float, uint8_t, bool, bool*);
extern "C" { ArcherHitFn OriginalArcherHit = nullptr; }
struct ArcherDamageFrame { float xmm6[4]; float* storedDamage; bool zeroDamage; };
static_assert(offsetof(ArcherDamageFrame, storedDamage) == 0x10 && offsetof(ArcherDamageFrame, zeroDamage) == 0x18, "Archer damage bridge layout");
extern "C" float MoraleMattersArcherDamage(void* component, float damage, void* returnAddress, ArcherDamageFrame* caller)
{
    if (returnAddress != Image + 0x153499D || Config.archerDamageTaken == 1 || !component || damage < 0 || !isfinite(damage))
        return damage;
    void* defender = At<void*>(component, 0xB8);
    const float adjusted = damage * Config.archerDamageTaken;
    if (!ArcherTroop(defender) || !isfinite(adjusted))
        return damage;
    void* group = Group(At<int>(defender, 0x7A0));
    if (Config.archerDamageTaken > 1 && (At<uint8_t>(group, 0x2E3A) || At<uint8_t>(group, 0x2E58)))
        return damage; // Do not amplify a value already clamped for scripted protection.
    caller->xmm6[0] = adjusted;
    *caller->storedDamage = adjusted;
    caller->zeroDamage = Config.archerDamageTaken == 0;
    return adjusted;
}
extern "C" bool ArcherHit(void*, void*, float, float, uint8_t, bool, bool*);
asm(
    ".text\n"
    ".p2align 4\n"
    ".seh_proc ArcherHit\n"
    "ArcherHit:\n"
    "subq $0x88, %rsp\n"
    ".seh_stackalloc 0x88\n"
    "movaps %xmm6, 0x60(%rsp)\n"
    ".seh_savexmm %xmm6, 0x60\n"
    ".seh_endprologue\n"
    "movq %rcx, 0x20(%rsp)\n"
    "movq %rdx, 0x28(%rsp)\n"
    "movq %r8, 0x30(%rsp)\n"
    "movq %r9, 0x38(%rsp)\n"
    "movaps %xmm2, 0x40(%rsp)\n"
    "movaps %xmm3, 0x50(%rsp)\n"
    "leaq 0xd8(%rsp), %rax\n"
    "movq %rax, 0x70(%rsp)\n"
    "movb $0, 0x78(%rsp)\n"
    "movaps %xmm2, %xmm1\n"
    "movq 0x88(%rsp), %r8\n"
    "leaq 0x60(%rsp), %r9\n"
    "call MoraleMattersArcherDamage\n"
    "movaps 0x40(%rsp), %xmm2\n"
    "movss %xmm0, %xmm2\n"
    "movaps 0x50(%rsp), %xmm3\n"
    "movaps 0x60(%rsp), %xmm6\n"
    "movq 0x20(%rsp), %rcx\n"
    "movq 0x28(%rsp), %rdx\n"
    "movq 0x30(%rsp), %r8\n"
    "movq 0x38(%rsp), %r9\n"
    "cmpb $0, 0x78(%rsp)\n"
    "jne 1f\n"
    "addq $0x88, %rsp\n"
    "jmp *OriginalArcherHit(%rip)\n"
    // Zero HP damage still runs native hit/guard reactions, but reports the
    // normal completion path so its guarded-hit fallback cannot add minimum damage.
    "1: movq 0xb0(%rsp), %rax\n"
    "movq %rax, 0x20(%rsp)\n"
    "movq 0xb8(%rsp), %rax\n"
    "movq %rax, 0x28(%rsp)\n"
    "movq 0xc0(%rsp), %rax\n"
    "movq %rax, 0x30(%rsp)\n"
    "call *OriginalArcherHit(%rip)\n"
    "movl $1, %eax\n"
    "addq $0x88, %rsp\n"
    "ret\n"
    ".seh_endproc\n"
);

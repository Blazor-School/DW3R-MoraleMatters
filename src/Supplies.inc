// Change only random infantry/captain drops. The native request retains its
// mode/protection checks, explicit drops, queue ownership and metadata.
using RequestDropFn = bool (*)(void*, int, const double*, void*);
using DropComboFn = int64_t (*)(void*);
static RequestDropFn OriginalRequestDrop = nullptr;
static DropComboFn OriginalDropCombo = nullptr;
static thread_local void* DropComboCharacter = nullptr;

struct DropComboScope
{
    void* previous;
    explicit DropComboScope(void* character) : previous(DropComboCharacter) { DropComboCharacter = character; }
    ~DropComboScope() { DropComboCharacter = previous; }
};

static int64_t DropCombo(void* character)
{
    const int64_t combo = OriginalDropCombo(character);
    if (character != DropComboCharacter || !character)
        return combo;
    // Adapt the native 16-hit eligibility decision only within this request.
    // The actual combo attribute and other readers are never rewritten.
    return combo >= Config.dropComboHits ? (combo < 16 ? 16 : combo) : 0;
}

static bool RequestDrop(void* manager, int item, const double* position, void* character)
{
    void* group = Config.meatBuns && item == 0xFF && Readable(character, 0x7A8) && At<uint8_t>(character, 0x74C) >= 2 ? Group(GroupIdFromCharacter(character)) : nullptr;
    if (!group || At<int>(group, 0x40) != 1 || At<uint8_t>(group, 0x2D20) < 6 || !Readable(manager, 0x98))
    {
        DropComboScope native(nullptr);
        return OriginalRequestDrop(manager, item, position, character);
    }
    // The game's CRITICAL_SECTION is recursive. Hold it across native enqueue
    // so another worker cannot consume/reallocate the new request before inspection.
    auto* lock = reinterpret_cast<CRITICAL_SECTION*>(static_cast<uint8_t*>(manager) + 0x70);
    EnterCriticalSection(lock);
    const int before = At<int>(manager, 0x68);
    bool result;
    {
        DropComboScope qualifying(character);
        result = OriginalRequestDrop(manager, item, position, character);
    }
    const int after = At<int>(manager, 0x68);
    auto* requests = At<uint8_t*>(manager, 0x60);
    if (result && before >= 0 && before < 16384 && after == before + 1 && Readable(requests, static_cast<size_t>(after) * 0x38))
    {
        auto* request = requests + static_cast<size_t>(before) * 0x38;
        if ((request[0x30] == 0 || request[0x30] == 4) && request[0x31] == At<uint8_t>(character, 0x750))
        {
            using RandomFn = int (*)();
            const int roll = reinterpret_cast<RandomFn>(At<void*>(Image, 0x7E1C290))() & 0x7FFF;
            request[0x30] = roll < Config.meatBunPercent * 32768 / 100 ? 0 : 4;
        }
    }
    LeaveCriticalSection(lock);
    return result;
}

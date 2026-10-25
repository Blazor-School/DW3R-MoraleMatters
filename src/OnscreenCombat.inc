// The native hit calculation applies motion damage before its minimum-damage,
// guard and protected-officer rules. Scaling here avoids a polling window and
// leaves GAS attributes, native power-ups and attack animation speed untouched.
using OnscreenHitFn = void (*)(void*, void*, float, uint64_t, float*, float*, bool*);
static OnscreenHitFn OriginalOnscreenHit = nullptr;
static volatile LONG OnscreenEvents = 0;

static float CharacterCombatScale(void* character, const float (&curve)[5])
{
    if (!Config.attributes || !Readable(character, 0x7A8) || At<uint8_t>(character, 0x74C) < 2 || At<uint8_t>(character, 0x787))
        return 1;
    void* group = Group(At<int>(character, 0x7A0));
    if (!group || !GroupAlive(group) || At<uint8_t>(group, 0x191) || At<uint8_t>(group, 0x198))
        return 1;
    void* corps = CorpsObject(At<int>(group, 0x38));
    if (!corps || !isfinite(At<float>(corps, 0x28)))
        return 1;
    const int rank = At<uint8_t>(group, 0x2D20);
    const bool named = At<int>(character, 0x7A4) == -1 && rank >= 2 && rank <= 5;
    return ScaleForOfficer(Interpolate(At<float>(corps, 0x28), Config.anchors, curve), named, Config);
}

static void OnscreenHit(void* attacker, void* defender, float motion, uint64_t flags, float* damage, float* auxiliary, bool* nativeFlag)
{
    const float outgoing = CharacterCombatScale(attacker, Config.outgoing);
    const float incoming = CharacterCombatScale(defender, Config.incoming);
    const float adjusted = motion * outgoing * incoming;
    OriginalOnscreenHit(attacker, defender, isfinite(motion) && isfinite(adjusted) ? adjusted : motion, flags, damage, auxiliary, nativeFlag);
    if (damage && isfinite(*damage) && *damage > 0 && (outgoing != 1 || incoming != 1))
    {
        const LONG event = InterlockedIncrement(&OnscreenEvents);
        if (event <= 8 || event % 1000 == 0)
            Log("On-screen hit: attackerGroup=%d defenderGroup=%d dealt=%.3fx taken=%.3fx damage=%.2f nativeFlag=%d.", GroupIdFromCharacter(attacker), GroupIdFromCharacter(defender), outgoing, incoming, *damage, nativeFlag && *nativeFlag);
    }
}

bool ProbeOnscreenHit(void* attacker, void* defender, float motion, float& nativeDamage, float& scaledDamage, float& outgoing, float& incoming)
{
    // Diagnostic only: evaluate the same normal hit twice without applying HP loss.
    if (!OriginalOnscreenHit || !Readable(attacker, 0x7A8) || !Readable(defender, 0x7A8) || !At<void*>(attacker, 0x698) || !At<void*>(defender, 0x698) || !isfinite(motion) || motion <= 0 || motion > 100)
        return false;
    outgoing = CharacterCombatScale(attacker, Config.outgoing);
    incoming = CharacterCombatScale(defender, Config.incoming);
    float auxiliary = 0;
    bool nativeFlag = false;
    OriginalOnscreenHit(attacker, defender, motion, 0, &nativeDamage, &auxiliary, &nativeFlag);
    OriginalOnscreenHit(attacker, defender, motion * outgoing * incoming, 0, &scaledDamage, &auxiliary, &nativeFlag);
    return isfinite(nativeDamage) && isfinite(scaledDamage);
}

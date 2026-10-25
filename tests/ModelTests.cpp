#include "Model.h"
#include <stdio.h>
#include <stdlib.h>

using namespace MoraleMatters;
static int Assertions = 0;
#define CHECK(expression) do { ++Assertions; if (!(expression)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expression); return 1; } } while (0)
static bool Near(float a, float b) { return fabsf(a - b) < 0.001f; }

int main()
{
    Settings settings;
    CorpsState corps;
    float morale = 400;
    CHECK(corps.Rally(morale, settings));
    CHECK(Near(morale, 500));
    CHECK(!corps.Rally(morale, settings));
    corps.Begin(morale);
    CHECK(Near(morale, 400));
    morale -= 50;
    // Nested morale calls during a native corps update must not add/subtract the bonus twice.
    corps.Begin(morale);
    morale += 25;
    corps.End(morale);
    CHECK(Near(morale, 375));
    corps.End(morale);
    CHECK(Near(morale, 475));
    corps.Advance(0, morale);
    CHECK(Near(corps.remaining, 180));
    corps.Advance(179, morale);
    CHECK(Near(morale, 475));
    corps.Advance(1, morale);
    CHECK(Near(morale, 375));
    CHECK(corps.Rally(morale, settings));

    CorpsState capped;
    float high = 850;
    CHECK(capped.Rally(high, settings));
    CHECK(Near(high, 899));
    CHECK(Near(capped.VisibleBonus(), 49));
    capped.Begin(high);
    high -= 100;
    capped.End(high);
    CHECK(Near(high, 850));
    CHECK(Near(capped.base, 750));
    capped.Advance(180, high);
    CHECK(Near(high, 750));
    high = 600; // A direct scenario write while no bonus is active.
    capped.Advance(1, high);
    CHECK(Near(capped.base, 600));
    high = 580;
    capped.Observe(high);
    capped.Observe(high);
    CHECK(Near(capped.base, 580));

    EncounterState first, second;
    CHECK(!first.Enter(1001, settings));
    CHECK(first.Enter(1000, settings));
    CHECK(!first.Enter(1000, settings));
    CHECK(!first.Enter(1200, settings));
    CHECK(!first.Enter(1501, settings));
    CHECK(first.Enter(999, settings));
    CHECK(Near(first.Heal(100, 1000, settings), 500));
    CHECK(Near(first.Heal(100, 1000, settings), 0));
    CHECK(Near(second.Heal(900, 1000, settings), 100));
    first.Advance(179);
    CHECK(Near(first.Heal(100, 1000, settings), 0));
    first.Advance(1);
    CHECK(Near(first.Heal(1000, 1000, settings), 0));
    CHECK(Near(first.cooldown, 0));
    CHECK(Near(first.Heal(0, 1000, settings), 0));
    CHECK(Near(first.Heal(500, 1000, settings), 500));

    for (int moraleValue = 1; moraleValue <= 899; ++moraleValue)
    {
        const float outgoing = Interpolate(static_cast<float>(moraleValue), settings.anchors, settings.outgoing);
        const float incoming = Interpolate(static_cast<float>(moraleValue), settings.anchors, settings.incoming);
        const float frequency = Interpolate(static_cast<float>(moraleValue), settings.anchors, settings.frequency);
        CHECK(outgoing >= 0.6f && outgoing <= 1.5f);
        CHECK(incoming >= 0.7f && incoming <= 1.3f);
        CHECK(frequency >= 0.65f && frequency <= 1.5f);
        CHECK(Near(ScaleForOfficer(outgoing, true, settings), 1 + (outgoing - 1) * 0.5f));
    }
    CHECK(Near(Interpolate(300, settings.anchors, settings.outgoing), 0.9f));
    CHECK(Near(OfficerLeak(100, 500, false, settings), 25));
    CHECK(Near(OfficerLeak(100, 10, false, settings), 10));
    CHECK(Near(OfficerLeak(100, 10, true, settings), 9));
    // Native troop-deficit accounting: 100 pool damage, 25 leader damage => 75 troop damage.
    CHECK(Near((500 - 25) + 1000 - (1500 - 100), 75));
    TrackedRate rate;
    float current = 1;
    rate.Apply(current, 1.5f);
    CHECK(Near(current, 1.5f));
    for (int update = 0; update < 1000; ++update)
    {
        rate.Apply(current, 1.5f);
        CHECK(Near(current, 1.5f)); // Repeated batches must never compound the multiplier.
    }
    rate.Apply(current, 0.6f);
    CHECK(Near(current, 0.6f));
    current = 2; // A genuine game/event write becomes the new baseline.
    rate.Apply(current, 1.5f);
    CHECK(Near(current, 3));
    rate.Apply(current, 1);
    CHECK(Near(current, 2)); // Player/disabled scaling restores the underlying value.
    rate.Apply(current, NAN);
    CHECK(Near(current, 2));
    const int arrivals[] = {50, 100, 250, 500};
    for (int arriving : arrivals)
    {
        ReinforcementState wave;
        wave.Begin(500, settings);
        wave.Arrive(arriving, settings);
        float gain;
        CHECK(!wave.Advance(0, false, settings, gain));
        CHECK(!wave.Advance(2.9f, false, settings, gain));
        CHECK(wave.Advance(0.1f, false, settings, gain));
        CHECK(Near(gain, fminf(50, arriving * 0.2f)));
        CHECK(!wave.Advance(3, false, settings, gain)); // Already consumed.
    }
    ReinforcementState wave;
    float gain;
    wave.CaptureNative(8, settings);
    wave.Begin(500, settings);
    wave.Arrive(100, settings);
    wave.CaptureNative(13, settings);
    CHECK(wave.Advance(3, false, settings, gain));
    CHECK(Near(wave.target, 20) && Near(wave.nativeCredit, 13) && Near(gain, 7));
    wave.Begin(500, settings);
    wave.Arrive(100, settings);
    CHECK(wave.Advance(3, false, settings, gain));
    CHECK(Near(gain, 20)); // Prior native credit cannot pay for a second wave.
    wave.CaptureNative(113, settings);
    CHECK(!wave.Advance(3, false, settings, gain));
    wave.Begin(500, settings);
    wave.Arrive(100, settings);
    CHECK(wave.Advance(3, false, settings, gain));
    CHECK(Near(gain, 20)); // Old credit expired.
    wave.Begin(500, settings);
    wave.Arrive(100, settings);
    wave.CaptureNative(183, settings);
    CHECK(wave.Advance(3, false, settings, gain));
    CHECK(Near(gain, 0) && Near(wave.nativeCredit, 70)); // Larger native awards are preserved.
    wave.Begin(500, settings);
    wave.Arrive(50, settings);
    CHECK(!wave.Advance(2, false, settings, gain));
    wave.Arrive(50, settings);
    CHECK(!wave.Advance(2, false, settings, gain));
    CHECK(wave.Advance(1, false, settings, gain));
    CHECK(Near(gain, 20)); // Both groups use the original denominator and one cap.
    wave.Begin(0, settings);
    wave.Arrive(100, settings);
    CHECK(wave.Advance(3, false, settings, gain));
    CHECK(Near(gain, 0)); // A newly appearing corps gets no extra starting morale.
    wave.Begin(500, settings);
    CHECK(wave.Advance(3, false, settings, gain));
    CHECK(Near(gain, 0)); // Empty/failed wave.
    wave.Begin(500, settings);
    CHECK(!wave.Advance(29, true, settings, gain));
    CHECK(wave.Advance(1, true, settings, gain));
    CHECK(Near(gain, 0)); // Unsuccessful activation timed out.
    settings.reinforcementGain = 5;
    settings.reinforcementCap = 12;
    settings.reinforcementWaveSeconds = 1;
    wave.Begin(500, settings);
    wave.Arrive(200, settings);
    CHECK(wave.Advance(1, false, settings, gain));
    CHECK(Near(gain, 12));
    settings.reinforcements = false;
    wave.Begin(500, settings);
    wave.Arrive(200, settings);
    CHECK(wave.Advance(1, false, settings, gain));
    CHECK(Near(gain, 0));
    settings = {};
    CorpsState permanent;
    float native = 400;
    permanent.Observe(native);
    CHECK(Near(permanent.nativeGains, 0));
    CHECK(permanent.Rally(native, settings));
    CHECK(Near(permanent.nativeGains, 0));
    CHECK(Near(permanent.AddPermanent(native, 20), 20));
    CHECK(Near(native, 520) && Near(permanent.nativeGains, 0));
    permanent.Begin(native);
    native -= 15;
    permanent.Begin(native);
    native += 30;
    permanent.End(native);
    permanent.End(native);
    CHECK(Near(permanent.nativeGains, 30));
    CHECK(Near(permanent.base, 435) && Near(native, 535));
    native += 5;
    permanent.Observe(native);
    permanent.Observe(native);
    CHECK(Near(permanent.nativeGains, 35));
    permanent.Advance(180, native);
    CHECK(Near(native, 440) && Near(permanent.nativeGains, 35));
    CHECK(Near(permanent.AddPermanent(native, 1000), 459));
    CHECK(Near(native, 899) && Near(permanent.nativeGains, 35));
    printf("PASS: %d assertions covering morale events/expiry/caps, proximity, healing, combat and reinforcement wave top-ups.\n", Assertions);
    return 0;
}

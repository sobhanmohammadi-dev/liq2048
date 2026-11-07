#include "Synth.hpp"

#include <algorithm>
#include <cmath>

namespace liq::audiogen {

namespace {

constexpr float kTwoPi = 6.28318530718f;

float midiHz(float m) { return 440.f * std::pow(2.f, (m - 69.f) / 12.f); }

// A minor pentatonic over several octaves, indexed by droplet level.
float levelHzFor(const int* pent, int base, int level) {
    const int l = std::max(0, level);
    return midiHz(float(base + pent[l % 5] + 12 * (l / 5)));
}

inline float tri(float ph) {  // ph in [0,1)
    return 4.f * std::fabs(ph - 0.5f) - 1.f;
}

struct Rng {
    uint32_t s = 2463534242u;
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float uni() { return float(next() >> 8) * (1.f / 16777216.f); }
    float bi() { return uni() * 2.f - 1.f; }
    int range(int lo, int hi) { return lo + int(next() % uint32_t(hi - lo + 1)); }
};

enum class Kind : uint8_t { Sine, Tri, Noise };

struct Voice {
    uint32_t ns = 1;  // private noise state (keeps music periodic regardless of what else is sounding)
    bool on = false;
    bool music = false;
    Kind kind = Kind::Sine;
    float ph = 0, f = 0, mult = 1;   // phase, current freq, per-sample glide multiplier
    float amp = 0, atk = 0.002f, decay = 6.f;  // decay = exp rate per second
    float t = 0, dur = 0, delay = 0;
    float panL = 0.7f, panR = 0.7f;
    float partial2 = 0, partial3 = 0;  // bell overtones
    float r2 = 2.01f, r3 = 3.02f;      // their frequency ratios (inharmonic for metal)
    float lp = 0, lpMult = 1, z = 0;   // noise one-pole coefficient (0..1) and its per-sample factor
    float send = 0.2f;                 // reverb send
    float echo = 0;                    // echo send (music pluck)
};

struct Pad {
    float f = 0, ph[3] = {0, 0, 0}, gain = 0, target = 0, pan = 0.5f;
};

struct Comb {
    std::vector<float> buf;
    size_t i = 0;
    float lpState = 0;
    void init(size_t n) { buf.assign(n, 0.f); i = 0; lpState = 0; }
    float process(float in, float fb, float damp) {
        const float o = buf[i];
        lpState = o * (1.f - damp) + lpState * damp;
        buf[i] = in + lpState * fb;
        if (++i >= buf.size()) i = 0;
        return o;
    }
};
struct Allpass {
    std::vector<float> buf;
    size_t i = 0;
    void init(size_t n) { buf.assign(n, 0.f); i = 0; }
    float process(float in) {
        const float b = buf[i];
        const float out = -in + b;
        buf[i] = in + b * 0.5f;
        if (++i >= buf.size()) i = 0;
        return out;
    }
};

struct Chord {
    int bass;
    int pad[4];
    int arp[5];
};
struct Look {  // everything that makes one palette sound different from another
    float bpm;
    uint32_t seed;
    int pent[5];       // scale degrees used to pitch the sound effects by droplet level
    int baseMidi;      // note of level 0
    Chord chords[4];
    // music timbre
    int pluckKind;     // 0 sine, 1 triangle
    float pluckP2, pluckP3, pluckR2, pluckR3, pluckDecay, pluckDur, pluckDrop;  // drop: start pitch ratio (log-drum thump)
    float padW0, padW1, padW2, padCut, padGain;
    float kickAmp, kickAt, hatAt, hatAmp, tomAmt, shakerAmt;
    float pingProb, pingRise, pingDur;
    float echoAmt, revFb, revDamp, revWet;
    // sound effects
    int bodyKind;
    float bellP2, bellP3, bellR2, bellR3, bellDur, bellAmp, woodTok, metal;
    float splashBright, clickHz, tail;
};

const Look kLooks[4] = {
    // Night: exactly the original sound (Am9, Fmaj7, Cmaj9, G6).
    {84.f, 17u, {0, 3, 5, 7, 10}, 57,
     {{45, {57, 60, 64, 71}, {69, 72, 76, 79, 81}}, {41, {53, 57, 60, 64}, {65, 69, 72, 76, 77}},
      {48, {60, 64, 67, 74}, {67, 72, 74, 76, 79}}, {43, {55, 59, 62, 64}, {67, 71, 74, 76, 79}}},
     0, 0.5f, 0.2f, 2.01f, 3.02f, 7.f, 0.7f, 1.f,
     0.5f, 0.4f, 0.3f, 0.035f, 1.f,
     1.f, 0.22f, 0.40f, 1.f, 0.f, 0.f,
     0.09f, 1.08f, 0.5f,
     1.f, 0.84f, 0.32f, 0.55f,
     0, 0.4f, 0.18f, 2.01f, 3.02f, 1.f, 1.f, 0.f, 0.f,
     1.f, 1100.f, 1.f},
    // Steel: D dorian, metallic inharmonic bars, dry room, ticking hats and a hard kick.
    {100.f, 29u, {0, 3, 5, 7, 10}, 62,
     {{38, {50, 53, 57, 64}, {62, 65, 69, 72, 74}}, {34, {46, 50, 53, 57}, {58, 62, 65, 69, 70}},
      {41, {53, 57, 60, 64}, {65, 69, 72, 76, 77}}, {36, {48, 52, 55, 62}, {60, 64, 67, 69, 72}}},
     1, 0.7f, 0.4f, 2.76f, 5.4f, 10.f, 0.45f, 1.f,
     0.25f, 0.6f, 0.15f, 0.06f, 0.8f,
     1.5f, 0.12f, 0.10f, 1.4f, 0.f, 0.f,
     0.05f, 1.0f, 0.18f,
     0.45f, 0.70f, 0.55f, 0.35f,
     1, 0.9f, 0.5f, 2.76f, 5.4f, 0.55f, 1.2f, 0.f, 1.f,
     0.6f, 2400.f, 0.55f},
    // Sunset: F major, mellow sine marimba, wide pads, long echoes, only a heartbeat of percussion.
    {72.f, 41u, {0, 2, 4, 7, 9}, 53,
     {{41, {53, 57, 60, 67}, {65, 69, 72, 76, 79}}, {38, {50, 53, 57, 64}, {62, 65, 69, 72, 74}},
      {46, {58, 62, 65, 69}, {70, 74, 77, 81, 82}}, {36, {52, 55, 60, 62}, {60, 64, 67, 69, 72}}},
     0, 0.25f, 0.05f, 4.0f, 6.0f, 5.f, 1.0f, 1.f,
     0.35f, 0.6f, 0.1f, 0.025f, 1.25f,
     0.45f, 0.60f, 2.f, 0.f, 0.f, 0.f,
     0.12f, 1.05f, 0.7f,
     1.6f, 0.90f, 0.25f, 0.70f,
     0, 0.22f, 0.05f, 4.0f, 6.0f, 1.5f, 0.9f, 0.f, 0.f,
     0.8f, 900.f, 1.3f},
    // Jungle: E minor/dorian, log-drum plucks with a thump, bongos, shakers and chirps.
    {96.f, 53u, {0, 3, 5, 7, 10}, 64,
     {{40, {52, 55, 59, 62}, {64, 67, 71, 74, 76}}, {36, {48, 52, 55, 59}, {60, 64, 67, 71, 72}},
      {38, {50, 54, 57, 59}, {62, 66, 69, 71, 74}}, {43, {55, 59, 62, 64}, {67, 71, 74, 76, 79}}},
     0, 0.8f, 0.f, 2.0f, 3.0f, 9.f, 0.40f, 1.45f,
     0.5f, 0.3f, 0.1f, 0.03f, 0.6f,
     0.9f, 0.18f, 0.12f, 1.f, 1.f, 1.f,
     0.14f, 1.9f, 0.14f,
     0.8f, 0.78f, 0.45f, 0.5f,
     0, 0.8f, 0.0f, 2.0f, 3.0f, 0.4f, 1.1f, 1.f, 0.f,
     1.3f, 1400.f, 0.7f},
};

}  // namespace

struct Synth::Impl {
    Impl(int sr, Style style) : sr(sr), styleId(int(style)), L(kLooks[int(style)]) {
        const float k = float(sr) / 44100.f;
        static const int cl[4] = {1557, 1617, 1491, 1422};
        for (int i = 0; i < 4; ++i) {
            combL[i].init(size_t(float(cl[i]) * k));
            combR[i].init(size_t(float(cl[i] + 23) * k));
        }
        apL[0].init(size_t(556 * k));
        apL[1].init(size_t(441 * k));
        apR[0].init(size_t(579 * k));
        apR[1].init(size_t(464 * k));
        const size_t dl = size_t(0.375f * 60.f / L.bpm * float(sr));  // dotted eighth
        echoL.assign(dl, 0.f);
        echoR.assign(dl, 0.f);
        samplesPerStep = 60.0 / double(L.bpm) / 4.0 * double(sr);
        setChord(0);
    }

    int sr;
    int styleId = 0;
    const Look& L;
    Rng rng;
    Voice voices[56];
    Pad pads[10];
    Comb combL[4], combR[4];
    Allpass apL[2], apR[2];
    std::vector<float> echoL, echoR;
    size_t echoI = 0;
    float echoLp = 0, echoLp2 = 0;

    float musicVolT = 0.6f, sfxVolT = 0.9f, musicVol = 0.6f, sfxVol = 0.9f;
    MusicMood mood = MusicMood::Off;
    float moodGain = 0.f, intensityT = 0.f, intensity = 0.f;
    double stepClock = 0, samplesPerStep = 0;
    int step = 0, chordIdx = 0, arpIdx = 2;
    float kickEnv = 0.f, padLp[2] = {0, 0}, lfo = 0.f;
    float hatHp[2] = {0, 0};

    // ---------- voice helpers ----------
    Voice* alloc() {
        Voice* best = &voices[0];
        float bestLeft = 1e9f;
        for (auto& v : voices) {
            if (!v.on) return &v;
            const float left = v.dur - v.t + v.delay;
            if (left < bestLeft) {
                bestLeft = left;
                best = &v;
            }
        }
        return best;
    }

    struct P {  // voice description (designated-init style)
        Kind kind = Kind::Sine;
        float f0 = 440, f1 = 440, amp = 0.2f, atk = 0.003f, dur = 0.3f, delay = 0, pan = 0, decayK = 6.f;
        float r2 = 2.01f, r3 = 3.02f;
        float p2 = 0, p3 = 0, lp0 = 0.5f, lp1 = 0.5f, send = 0.2f, echo = 0;
        bool music = false;
    };
    void add(const P& d) {
        Voice* v = alloc();
        *v = Voice();
        v->on = true;
        v->ns = rng.next() | 1u;
        v->music = d.music;
        v->kind = d.kind;
        v->f = d.f0;
        v->amp = d.amp;
        v->atk = std::max(0.0005f, d.atk);
        v->dur = d.dur;
        v->decay = d.decayK / std::max(0.02f, d.dur);
        v->delay = d.delay;
        const float a = (d.pan + 1.f) * 0.25f * kTwoPi * 0.25f;  // equal-power pan
        v->panL = std::cos(a * 2.f);
        v->panR = std::sin(a * 2.f);
        v->partial2 = d.p2;
        v->partial3 = d.p3;
        v->r2 = d.r2;
        v->r3 = d.r3;
        v->send = d.send;
        v->echo = d.echo;
        v->mult = std::pow(std::max(d.f1, 1.f) / std::max(d.f0, 1.f), 1.f / std::max(1.f, d.dur * float(sr)));
        v->lp = d.lp0;
        v->lpMult = std::pow(std::max(d.lp1, 0.001f) / std::max(d.lp0, 0.001f), 1.f / std::max(1.f, d.dur * float(sr)));
        v->ph = d.kind == Kind::Noise ? 0.f : rng.uni() * 0.f;
    }

    // ---------- music ----------
    void setChord(int idx) {
        chordIdx = idx;
        const Chord& c = L.chords[idx];
        for (auto& p : pads) p.target = 0.f;
        for (int n = 0; n < 4; ++n) {
            Pad* slot = nullptr;
            for (auto& p : pads)
                if (p.gain < 0.01f && p.target == 0.f) { slot = &p; break; }
            if (!slot) {
                for (auto& p : pads) if (!slot || p.gain < slot->gain) slot = &p;
            }
            slot->f = midiHz(float(c.pad[n]));
            slot->target = 1.f;
            slot->pan = 0.2f + 0.6f * float(n) / 3.f;
        }
    }

    void pluck(float hz, float amp, float pan, float dur, float echo, float send) {
        P d;
        d.kind = L.pluckKind ? Kind::Tri : Kind::Sine;
        d.f0 = hz * L.pluckDrop;
        d.f1 = hz;
        d.amp = amp;
        d.atk = L.pluckKind ? 0.003f : 0.007f;
        d.dur = dur * L.pluckDur;
        d.pan = pan;
        d.p2 = L.pluckP2;
        d.p3 = L.pluckP3;
        d.r2 = L.pluckR2;
        d.r3 = L.pluckR3;
        d.decayK = L.pluckDecay;
        d.echo = echo * L.echoAmt;
        d.send = send;
        d.music = true;
        add(d);
    }

    void onStep() {
        if (step % 128 == 0) {  // every cycle replays exactly: the rendered file loops seamlessly
            rng.s = 0x9E3779B9u + (mood == MusicMood::Game ? L.seed : L.seed - 12u);
            arpIdx = 2;
        }
        const Chord& c = L.chords[chordIdx];
        const bool game = mood == MusicMood::Game;
        const float it = intensity;
        if (step % 32 == 0) setChord((step / 32) % 4);
        const int s16 = step % 16;

        // Bass: root on beats 1 and 3, a walking octave pick-up when busy.
        if (s16 == 0 || s16 == 8 || (game && it > 0.45f && s16 == 11)) {
            P d;
            d.f0 = d.f1 = midiHz(float(c.bass) + (s16 == 11 ? 12.f : 0.f));
            d.amp = 0.30f;
            d.atk = 0.01f;
            d.dur = 1.3f;
            d.decayK = 5.f;
            d.p2 = 0.25f;
            d.send = 0.05f;
            d.music = true;
            add(d);
        }
        // Soft kick + pumping.
        if (game && it > L.kickAt && s16 % 8 == 0) {
            P d;
            d.f0 = 120.f;
            d.f1 = 46.f;
            d.amp = (0.26f + 0.2f * it) * L.kickAmp;
            d.dur = 0.22f;
            d.decayK = 6.f;
            d.send = 0.02f;
            d.music = true;
            add(d);
            kickEnv = 1.f;
        }
        // Shaker / hat on off-beat eighths.
        if (game && it > L.hatAt && s16 % 4 == 2) {
            P d;
            d.kind = Kind::Noise;
            d.amp = (0.05f + 0.05f * it) * L.hatAmp;
            d.dur = 0.05f;
            d.lp0 = d.lp1 = 0.85f;
            d.decayK = 7.f;
            d.pan = rng.bi() * 0.5f;
            d.send = 0.12f;
            d.music = true;
            add(d);
        }
        // Bongos and shakers (Jungle only).
        if (L.tomAmt > 0.f && (s16 == 3 || s16 == 6 || s16 == 10 || s16 == 13 || (game && it > 0.5f && s16 == 15))) {
            P d;
            d.f0 = (s16 == 6 || s16 == 13) ? 330.f : 245.f;
            d.f1 = d.f0 * 0.62f;
            d.amp = (game ? 0.16f : 0.10f) * L.tomAmt;
            d.atk = 0.002f;
            d.dur = 0.18f;
            d.decayK = 6.f;
            d.pan = (s16 & 1) ? 0.45f : -0.45f;
            d.p2 = 0.35f;
            d.send = 0.25f;
            d.music = true;
            add(d);
        }
        if (L.shakerAmt > 0.f && s16 % 2 == 1) {
            P d;
            d.kind = Kind::Noise;
            d.amp = (game ? 0.06f : 0.04f) * L.shakerAmt;
            d.dur = 0.06f;
            d.lp0 = d.lp1 = 0.9f;
            d.decayK = 6.f;
            d.pan = rng.bi() * 0.6f;
            d.send = 0.1f;
            d.music = true;
            add(d);
        }
        // Arpeggio of plucks.
        float pr = game ? 0.42f + 0.38f * it : 0.30f;
        const bool eighth = s16 % 2 == 0;
        if (!eighth) pr = game ? (it > 0.55f ? 0.22f * it : 0.f) : 0.f;
        if (rng.uni() < pr) {
            arpIdx = std::clamp(arpIdx + rng.range(-2, 2), 0, 4);
            const float oct = (rng.uni() < 0.2f) ? 12.f : 0.f;
            pluck(midiHz(float(c.arp[arpIdx]) + oct), game ? 0.16f : 0.13f, rng.bi() * 0.7f, 0.7f, 0.5f, 0.35f);
        }
        // Water-drop pings (the "liquid" texture).
        if (rng.uni() < L.pingProb * (game ? 0.55f : 1.f)) {
            P d;
            const float hz = midiHz(float(c.arp[rng.range(0, 4)]) + 12.f);
            d.f0 = hz * L.pingRise;
            d.f1 = hz;
            d.amp = 0.075f;
            d.dur = L.pingDur;
            d.pan = rng.bi();
            d.p2 = 0.3f;
            d.decayK = 7.f;
            d.send = 0.7f;
            d.echo = 0.35f;
            d.music = true;
            add(d);
        }
        ++step;
    }

    // ---------- render ----------
    void process(float* out, int frames) {
        const float rateMood = 1.f - std::exp(-1.f / (1.2f * float(sr)));
        const float rateVol = 1.f - std::exp(-1.f / (0.03f * float(sr)));
        const float padSlew = 1.f - std::exp(-1.f / (1.1f * float(sr)));
        const float dt = 1.f / float(sr);
        const float moodT = mood == MusicMood::Off ? 0.f : (mood == MusicMood::Menu ? 0.85f : 1.f);

        for (int n = 0; n < frames; ++n) {
            stepClock += 1.0;
            if (stepClock >= samplesPerStep) {
                stepClock -= samplesPerStep;
                intensity += (intensityT - intensity) * 0.15f;
                if (mood != MusicMood::Off) onStep();
            }
            moodGain += (moodT - moodGain) * rateMood;
            musicVol += (musicVolT - musicVol) * rateVol;
            sfxVol += (sfxVolT - sfxVol) * rateVol;
            kickEnv *= 0.9996f;
            lfo += dt * 0.06f * kTwoPi;
            if (lfo > kTwoPi) lfo -= kTwoPi;

            float mL = 0, mR = 0, sL = 0, sR = 0, revL = 0, revR = 0, echoIn = 0;

            // Pad
            if (moodGain > 0.001f) {
                float pL = 0, pR = 0;
                for (auto& p : pads) {
                    p.gain += (p.target - p.gain) * padSlew;
                    if (p.gain < 0.0005f) continue;
                    for (int k = 0; k < 3; ++k) {
                        const float det = k == 0 ? 1.f : (k == 1 ? 1.0045f : 0.9955f);
                        p.ph[k] += p.f * det * dt;
                        if (p.ph[k] >= 1.f) p.ph[k] -= 1.f;
                    }
                    const float w = tri(p.ph[0]) * L.padW0 + std::sin(p.ph[1] * kTwoPi) * L.padW1 + tri(p.ph[2]) * L.padW2;
                    const float g = p.gain * 0.105f * L.padGain;
                    pL += w * g * (1.f - p.pan);
                    pR += w * g * p.pan;
                }
                const float cutoff = L.padCut + 0.02f * std::sin(lfo) + 0.03f * intensity;
                padLp[0] += (pL - padLp[0]) * cutoff;
                padLp[1] += (pR - padLp[1]) * cutoff;
                const float duck = 1.f - 0.30f * kickEnv;
                mL += padLp[0] * 2.2f * duck;
                mR += padLp[1] * 2.2f * duck;
                revL += padLp[0] * 0.9f;
                revR += padLp[1] * 0.9f;
            }

            // Voices
            for (auto& v : voices) {
                if (!v.on) continue;
                if (v.delay > 0.f) {
                    v.delay -= dt;
                    continue;
                }
                v.t += dt;
                if (v.t >= v.dur) {
                    v.on = false;
                    continue;
                }
                float s;
                if (v.kind == Kind::Noise) {
                    v.lp = std::min(0.98f, v.lp * v.lpMult);
                    v.ns ^= v.ns << 13; v.ns ^= v.ns >> 17; v.ns ^= v.ns << 5;
                    v.z += (float(v.ns >> 8) * (2.f / 16777216.f) - 1.f - v.z) * v.lp;
                    s = v.z * 2.f;
                } else {
                    v.f *= v.mult;
                    v.ph += v.f * dt;
                    if (v.ph >= 1.f) v.ph -= std::floor(v.ph);
                    if (v.kind == Kind::Sine) s = std::sin(v.ph * kTwoPi);
                    else s = tri(v.ph);
                    if (v.partial2 > 0.f) s += v.partial2 * std::sin(v.ph * kTwoPi * v.r2);
                    if (v.partial3 > 0.f) s += v.partial3 * std::sin(v.ph * kTwoPi * v.r3);
                }
                const float env = std::min(1.f, v.t / v.atk) * std::exp(-v.t * v.decay);
                const float o = s * env * v.amp;
                const float l = o * v.panL, r = o * v.panR;
                if (v.music) {
                    mL += l;
                    mR += r;
                } else {
                    sL += l;
                    sR += r;
                }
                revL += l * v.send;
                revR += r * v.send;
                echoIn += o * v.echo;  // (already scaled by the style when the voice was made)
            }

            // Ping-pong echo for plucks and pings.
            float eL = echoL[echoI], eR = echoR[echoI];
            echoLp += (eL - echoLp) * 0.35f;
            echoLp2 += (eR - echoLp2) * 0.35f;
            echoL[echoI] = echoIn + echoLp2 * 0.42f;
            echoR[echoI] = echoLp * 0.42f;
            if (++echoI >= echoL.size()) echoI = 0;
            mL += eL * 0.8f;
            mR += eR * 0.8f;
            revL += eL * 0.4f;
            revR += eR * 0.4f;

            // Reverb
            const float rin = (revL + revR) * 0.5f * 0.35f;
            float rl = 0, rr = 0;
            for (int i = 0; i < 4; ++i) {
                rl += combL[i].process(rin, L.revFb, L.revDamp);
                rr += combR[i].process(rin, L.revFb, L.revDamp);
            }
            for (int i = 0; i < 2; ++i) {
                rl = apL[i].process(rl);
                rr = apR[i].process(rr);
            }

            const float mg = moodGain * musicVol, sg = sfxVol;
            float oL = (mL * mg + sL * sg) + rl * (L.revWet * (0.35f * mg + 0.65f * sg + 0.01f));
            float oR = (mR * mg + sR * sg) + rr * (L.revWet * (0.35f * mg + 0.65f * sg + 0.01f));
            // soft limiter
            oL = std::tanh(oL * 1.1f) * 0.92f;
            oR = std::tanh(oR * 1.1f) * 0.92f;
            out[2 * n] = oL;
            out[2 * n + 1] = oR;
        }
    }

    // ---------- sound effects ----------
    void play(Sound s, float amount, int level, int chain) {
        amount = std::clamp(amount, 0.f, 1.f);
        switch (s) {
            case Sound::Drop: {  // release: a soft "plop"
                P a;
                const float pv = 1.f + 0.10f * (amount - 0.5f);
                a.f0 = 780.f * pv; a.f1 = 330.f * pv; a.amp = 0.20f; a.dur = 0.15f; a.decayK = 5.f; a.send = 0.25f;
                add(a);
                P b = a;
                b.f0 = 520.f; b.f1 = 780.f; b.amp = 0.07f; b.delay = 0.05f; b.dur = 0.09f;
                add(b);
                P t;
                t.kind = Kind::Noise; t.amp = 0.05f; t.dur = 0.012f; t.lp0 = t.lp1 = 0.7f; t.send = 0.05f;
                add(t);
                break;
            }
            case Sound::Splash: {  // liquid hits liquid: slosh + thump + bubbles
                P n;
                n.kind = Kind::Noise; n.amp = 0.16f + 0.26f * amount; n.dur = 0.12f + 0.22f * amount;
                n.lp0 = std::min(0.95f, (0.10f + 0.2f * amount) * L.splashBright); n.lp1 = 0.02f * L.splashBright; n.decayK = 4.5f; n.send = 0.3f; n.pan = rng.bi() * 0.3f;
                add(n);
                P th;
                th.f0 = 150.f; th.f1 = 52.f; th.amp = 0.10f + 0.28f * amount; th.dur = 0.18f; th.decayK = 5.f; th.send = 0.1f;
                add(th);
                const int bubbles = 2 + int(amount * 3.f);
                for (int i = 0; i < bubbles; ++i) {
                    P b;
                    const float hz = 420.f + rng.uni() * 1000.f;
                    b.f0 = hz; b.f1 = hz * 1.5f; b.amp = 0.05f + 0.05f * amount; b.dur = 0.05f + 0.04f * rng.uni();
                    b.delay = 0.02f + 0.16f * rng.uni(); b.pan = rng.bi() * 0.8f; b.decayK = 4.f; b.send = 0.4f;
                    add(b);
                }
                break;
            }
            case Sound::Merge: {
                const float f = levelHzFor(L.pent, L.baseMidi, level);
                P body;
                body.kind = L.bodyKind ? Kind::Tri : Kind::Sine;
                body.f0 = f * 0.7f; body.f1 = f * 1.35f; body.amp = 0.28f * (L.woodTok > 0.f ? 0.5f : 1.f); body.dur = 0.16f; body.decayK = 4.f; body.send = 0.3f;
                add(body);
                if (L.woodTok > 0.f) {  // wooden "tok" instead of a soft bloop
                    P tok;
                    tok.f0 = f * 1.7f; tok.f1 = f; tok.amp = 0.26f; tok.atk = 0.001f; tok.dur = 0.11f; tok.decayK = 7.f;
                    tok.p2 = 0.5f; tok.r2 = 2.f; tok.send = 0.3f;
                    add(tok);
                }
                P bell;
                bell.f0 = bell.f1 = f; bell.amp = 0.13f * L.bellAmp; bell.dur = 0.55f * L.bellDur; bell.atk = L.metal > 0.f ? 0.002f : 0.006f;
                bell.p2 = L.bellP2; bell.p3 = L.bellP3; bell.r2 = L.bellR2; bell.r3 = L.bellR3;
                bell.kind = L.bodyKind ? Kind::Tri : Kind::Sine;
                bell.send = 0.5f * L.tail; bell.echo = 0.15f * L.echoAmt;
                add(bell);
                P oct = bell;
                oct.f0 = oct.f1 = f * 2.f; oct.amp = 0.06f; oct.dur = 0.28f; oct.delay = 0.04f; oct.p2 = oct.p3 = 0.f;
                add(oct);
                P pop;
                pop.kind = Kind::Noise; pop.amp = 0.09f; pop.dur = 0.025f; pop.lp0 = pop.lp1 = 0.55f; pop.send = 0.1f;
                add(pop);
                if (level >= 6) {  // big merges get weight
                    P sub;
                    sub.f0 = f * 0.5f; sub.f1 = f * 0.25f; sub.amp = 0.14f + 0.04f * float(level - 6); sub.dur = 0.5f;
                    sub.decayK = 4.5f; sub.send = 0.2f;
                    add(sub);
                }
                if (level >= 11) {  // 2048: a shimmering chord
                    static const float r[4] = {1.f, 1.2599f, 1.4983f, 2.f};  // (a major-ish stack: fine in every style)
                    for (int i = 0; i < 4; ++i) {
                        P c;
                        c.kind = Kind::Sine; c.f0 = c.f1 = f * r[i]; c.amp = 0.10f; c.dur = 2.2f; c.atk = 0.04f * float(i + 1);
                        c.p2 = 0.3f; c.decayK = 3.f; c.send = 0.7f; c.delay = 0.08f * float(i); c.pan = -0.6f + 0.4f * float(i);
                        add(c);
                    }
                }
                break;
            }
            case Sound::Chain: {  // rising arpeggio layered on top of a merge, one more note per link
                const int notes = std::min(std::max(chain, 2), 6);
                for (int k = 0; k < notes; ++k) {
                    P a;
                    a.kind = L.bodyKind ? Kind::Tri : Kind::Sine; a.f0 = a.f1 = levelHzFor(L.pent, L.baseMidi, level + 2 + k * 2); a.amp = 0.10f; a.dur = 0.35f * L.bellDur;
                    a.delay = 0.07f * float(k); a.atk = 0.004f; a.p2 = L.bellP2; a.r2 = L.bellR2; a.decayK = 6.f; a.send = 0.5f * L.tail;
                    a.pan = (k & 1) ? 0.5f : -0.5f; a.echo = 0.2f;
                    add(a);
                }
                break;
            }
            case Sound::Click: {
                P a;
                a.f0 = L.clickHz; a.f1 = L.clickHz * 0.73f; a.amp = 0.10f; a.dur = L.metal > 0.f ? 0.07f : 0.04f; a.p2 = L.metal * 0.6f; a.r2 = 2.76f; a.decayK = 5.f; a.send = 0.1f;
                add(a);
                P t;
                t.kind = Kind::Noise; t.amp = 0.04f; t.dur = 0.01f; t.lp0 = t.lp1 = 0.7f; t.send = 0.f;
                add(t);
                break;
            }
            case Sound::Swipe: {
                P n;
                n.kind = Kind::Noise; n.amp = 0.10f; n.dur = 0.22f; n.lp0 = 0.04f; n.lp1 = 0.55f; n.decayK = 2.5f; n.atk = 0.05f;
                n.send = 0.25f;
                add(n);
                P a;
                a.f0 = 300.f; a.f1 = 900.f; a.amp = 0.035f; a.dur = 0.2f; a.atk = 0.04f; a.decayK = 3.f; a.send = 0.3f;
                add(a);
                break;
            }
            case Sound::Fail: {
                P a;
                a.kind = Kind::Tri; a.f0 = 330.f; a.f1 = 88.f; a.amp = 0.22f; a.dur = 1.1f; a.decayK = 3.5f; a.p2 = 0.3f; a.send = 0.4f;
                add(a);
                P b = a;
                b.f0 = 392.f; b.f1 = 104.f; b.amp = 0.14f;
                add(b);
                P n;
                n.kind = Kind::Noise; n.amp = 0.16f; n.dur = 0.7f; n.lp0 = 0.05f; n.lp1 = 0.015f; n.decayK = 4.f; n.send = 0.3f;
                add(n);
                break;
            }
            case Sound::Success: {
                static const int notes[5] = {69, 72, 76, 81, 84};
                static const int majorNotes[5] = {72, 76, 79, 84, 88};  // C major pentatonic climb, for the warm palette
                for (int k = 0; k < 5; ++k) {
                    P a;
                    const int base = styleId == 2 ? majorNotes[k] : notes[k] + (L.baseMidi - 57);
                    a.kind = Kind::Sine; a.f0 = a.f1 = midiHz(float(base)); a.amp = 0.14f; a.dur = 0.9f; a.delay = 0.09f * float(k);
                    a.p2 = 0.45f; a.p3 = 0.2f; a.decayK = 5.f; a.send = 0.6f; a.echo = 0.25f; a.pan = (k - 2) * 0.25f;
                    add(a);
                }
                break;
            }
            case Sound::Unlock: {
                for (int k = 0; k < 4; ++k) {
                    P a;
                    a.f0 = a.f1 = midiHz(float(84 + (k % 2 ? 7 : 0) + k + (styleId == 0 ? 0 : (L.baseMidi - 57) % 12))); a.amp = 0.09f; a.dur = 0.4f; a.delay = 0.06f * float(k);
                    a.p2 = 0.5f; a.decayK = 6.f; a.send = 0.7f; a.pan = (k & 1) ? 0.6f : -0.6f; a.echo = 0.3f;
                    add(a);
                }
                P n;
                n.kind = Kind::Noise; n.amp = 0.05f; n.dur = 0.4f; n.lp0 = 0.5f; n.lp1 = 0.9f; n.atk = 0.1f; n.decayK = 3.f; n.send = 0.5f;
                add(n);
                break;
            }
            default: break;
        }
    }
};

Synth::Synth(int sampleRate, Style style) : p_(new Impl(sampleRate, style)), sr_(sampleRate) {}
int Synth::loopSamples() const { return int(std::llround(128.0 * p_->samplesPerStep)); }
Synth::~Synth() { delete p_; }
void Synth::sfx(Sound s, float amount, int level, int chain) { p_->play(s, amount, level, chain); }
void Synth::setMood(MusicMood m) { p_->mood = m; }
void Synth::setIntensity(float v) { p_->intensityT = std::clamp(v, 0.f, 1.f); }
void Synth::setMusicVolume(float v) { p_->musicVolT = std::clamp(v, 0.f, 1.f); }
void Synth::setSfxVolume(float v) { p_->sfxVolT = std::clamp(v, 0.f, 1.f); }
void Synth::render(float* out, int frames) { p_->process(out, frames); }

}  // namespace liq::audiogen

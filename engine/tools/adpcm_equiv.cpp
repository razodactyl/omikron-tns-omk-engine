// SPDX-License-Identifier: GPL-3.0-or-later
// THE ADPCM NIBBLE LAW THROUGH A TABLE DECODES THE SAME SAMPLES - bit for bit.
//
//     adpcm_equiv <gamedata> <tables>
//
// `adpcmDecode` and `AdpcmStereoStream` used to compute each nibble's delta
// with three branches and a shift, and its next index with an add and a
// clamp; they now look both up in `AdpcmTables::nibble` (todo/optimization.md
// step 16). The music streams through `AdpcmStereoStream` on the frame
// thread, and a console spent 25-34 ms decoding one second of it.
//
// This keeps the branching law VERBATIM (`RefChannel`, from `adpcm.cpp` as it
// stood before 2026-09-29) and compares it with the table three ways:
//
//   * EXHAUSTIVELY, one nibble: every index 0..88, every nibble 0..15 and
//     every predictor -32768..32767 - 93 million states, the whole state space
//     a channel can be in - for the next predictor and the next index;
//   * every shipped TRACKS/*.ADP, whole, through `adpcmDecode(stereo)` AND
//     byte by byte through `AdpcmStereoStream` (what the music player uses);
//   * every shipped VOICEOFF/*.ADP through `adpcmDecode(mono)`.
//
// It also times the reference against the table over the tracks, on THIS
// machine - a ratio, not a claim about the console.
//
// Prints one line per section, then `mismatches <total>`. Writes nothing.
#include "formats/adpcm.h"
#include "platform/datafs.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace {

std::int32_t clampRef(std::int32_t v) { return v < -32768 ? -32768 : (v > 32767 ? 32767 : v); }

// `adpcmDecode`'s channel and nibble law as they stood before 2026-09-29,
// verbatim but for the names
struct RefChannel {
    std::int32_t pred = 0;
    std::int32_t idx = 0;
    std::int32_t step = 7;
};

std::int16_t refNibble(RefChannel& c, int nib, const omk::AdpcmTables& t) {
    std::int32_t d = 0;
    if (nib & 4) d  = 4 * c.step;
    if (nib & 2) d += 2 * c.step;
    if (nib & 1) d += c.step;
    d >>= 2;
    c.pred = clampRef(c.pred + ((nib & 8) ? -d : d));
    c.idx += t.index()[static_cast<std::size_t>(nib)];
    c.idx = c.idx < 0 ? 0 : (c.idx > 88 ? 88 : c.idx);
    c.step = t.step()[static_cast<std::size_t>(c.idx)];
    return static_cast<std::int16_t>(c.pred);
}

std::vector<std::int16_t> refDecode(const std::vector<std::byte>& in, bool stereo,
                                    const omk::AdpcmTables& t) {
    std::vector<std::int16_t> out;
    out.reserve(in.size() * 2);
    RefChannel ch[2];
    for (int c = 0; c < (stereo ? 2 : 1); ++c) ch[c].step = t.step()[0];
    for (const std::byte b : in) {
        const auto byte = static_cast<std::uint8_t>(b);
        out.push_back(refNibble(ch[0], (byte >> 4) & 0xF, t));
        out.push_back(refNibble(ch[stereo ? 1 : 0], byte & 0xF, t));
    }
    return out;
}

double msSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: adpcm_equiv <gamedata> <tables>\n");
        return 2;
    }
    const omk::DataFs fs(argv[1]);
    const auto t = omk::AdpcmTables::loadJson(std::string(argv[2]) + "/adpcm.json");
    if (!t.valid()) { std::fprintf(stderr, "no ADPCM tables in %s\n", argv[2]); return 1; }
    long total = 0;

    // 1. every state a channel can be in, one nibble
    {
        long states = 0, bad = 0;
        for (std::int32_t idx = 0; idx < 89; ++idx)
            for (int nib = 0; nib < 16; ++nib) {
                const auto& e = t.nibble(idx, nib);
                for (std::int32_t pred = -32768; pred <= 32767; ++pred) {
                    RefChannel r;
                    r.pred = pred;
                    r.idx = idx;
                    r.step = t.step()[static_cast<std::size_t>(idx)];
                    refNibble(r, nib, t);
                    const std::int32_t p = clampRef(pred + e.delta);
                    if (p != r.pred || e.next != r.idx) ++bad;
                    ++states;
                }
            }
        std::printf("exhaustive states %ld mismatches %ld\n", states, bad);
        total += bad;
    }

    // 2. the tracks, whole and streamed; 3. the voice-overs, mono
    const auto section = [&](const char* dir, bool stereo) {
        long files = 0, bytes = 0, bad = 0;
        double refMs = 0.0, lutMs = 0.0;
        for (const std::string& path : fs.list(dir, "ADP")) {
            const auto raw = omk::DataFs::readPath(path);
            if (raw.empty()) continue;
            ++files;
            bytes += static_cast<long>(raw.size());
            auto t0 = std::chrono::steady_clock::now();
            const auto ref = refDecode(raw, stereo, t);
            refMs += msSince(t0);
            t0 = std::chrono::steady_clock::now();
            const auto lut = omk::adpcmDecode(raw, stereo, t);
            lutMs += msSince(t0);
            if (lut != ref) ++bad;
            if (stereo) {
                omk::AdpcmStereoStream s(t);
                std::size_t k = 0;
                bool same = true;
                for (const std::byte b : raw) {
                    std::int16_t l = 0, r = 0;
                    s.frame(b, l, r);
                    if (l != ref[k] || r != ref[k + 1]) same = false;
                    k += 2;
                }
                if (!same) ++bad;
            }
        }
        std::printf("%s files %ld bytes %ld mismatches %ld | reference %.1f ms, table %.1f ms\n",
                    dir, files, bytes, bad, refMs, lutMs);
        total += bad;
    };
    section("TRACKS", true);
    section("VOICEOFF", false);

    std::printf("mismatches %ld\n", total);
    return total == 0 ? 0 : 3;
}

// SPDX-License-Identifier: GPL-3.0-or-later
// OTNS ADPCM - the codec every voice line and scene sound is stored in.
//
// Transcribed from the game's own decoder, `sub_483200` (mono) and
// `sub_483340` (stereo), using the step and index tables at dword_4BCC50 and
// dword_4BCC10 - both lifted to `tables/adpcm.json`.
//
// It is IMA-ADPCM with **two differences from the textbook version, and both
// matter**:
//
//   * the HIGH nibble of each byte is decoded first, not the low one;
//   * the delta is `(4*b2 + 2*b1 + b0) * step >> 2` - IMA's unconditional
//     `step >> 3` bias term is ABSENT. Leaving it in makes the predictor
//     drift, about -9000 DC over a line, with the audio buried under it.
//
// The engine writes each decoded sample twice, upsampling to its mixer rate.
// That is dropped here; the useful rate is 22050 Hz.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace omk {

inline constexpr int kAdpcmRate = 22050;

// The tables are DATA - loaded from tables/adpcm.json rather than baked in, so
// they can still be diffed against the extraction (engine/README.md).
class AdpcmTables {
public:
    static AdpcmTables loadJson(const std::string& path);
    static AdpcmTables builtin();          // for a caller with no data dir
    bool valid() const { return step_.size() == 89 && index_.size() == 16; }
    const std::vector<std::int32_t>& step() const { return step_; }
    const std::vector<std::int32_t>& index() const { return index_; }

    // THE NIBBLE LAW AS A TABLE (todo/optimization.md step 16). A channel's
    // whole state is its predictor and its index - the step is always
    // `step()[index]` - so one nibble's effect depends on (index, nibble)
    // alone: a signed delta added to the predictor before the clamp, and the
    // next index. 89 x 16 entries, built once from `step()` / `index()`; empty
    // while the tables are not valid.
    struct Nibble { std::int32_t delta; std::int32_t next; };
    const Nibble& nibble(std::int32_t idx, int nib) const {
        return lut_[static_cast<std::size_t>(idx) * 16u + static_cast<std::size_t>(nib)];
    }
private:
    void buildLut();
    std::vector<std::int32_t> step_, index_;
    std::vector<Nibble> lut_;
};

// ONE STEREO FRAME AT A TIME. A stereo stream has no header and no blocks -
// each byte is one frame, the high nibble left and the low right - so a player
// can keep the file's own bytes and decode as it goes (`audio/music.h`).
class AdpcmStereoStream {
public:
    explicit AdpcmStereoStream(const AdpcmTables& t) : t_(&t) { reset(); }
    void reset();
    // the next byte of the file -> its left and right samples
    void frame(std::byte b, std::int16_t& left, std::int16_t& right);
private:
    const AdpcmTables* t_;
    std::int32_t pred_[2], idx_[2];
};

// -> interleaved 16-bit PCM. `stereo` decodes two independent channels.
std::vector<std::int16_t> adpcmDecode(std::span<const std::byte> in,
                                      bool stereo, const AdpcmTables& t);

}  // namespace omk

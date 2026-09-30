// SPDX-License-Identifier: GPL-3.0-or-later
// THE DEPTH TIE'S COINCIDENCES, over every model - the census for baking it at
// load (todo/optimization.md step 27).
//
//     tie_census <gamedata> [--list N] [--model <substring>] [--order]
//
// The depth tie (`o3de/depthtie.h`) degenerates a face whose position set an
// earlier depth-writing face of the same geometry already claimed, in draw
// order. It decides that every frame. Step 27's claim is that the answer never
// changes while a set is resident, so it can be computed once at load - and
// that claim rests on a property of the data this counts: that every group of
// coincident faces lies in ONE mesh, so the faces move together (a moving
// mesh moves every corner of it through one transform) and stay coincident.
// A group spread over two meshes that can move apart is the one case a
// load-time answer could get wrong.
//
// Faces are keyed exactly as the tie keys them: per draw (a batch of
// `buildGeometry`'s engine-filtered geometry), two consecutive triangles
// that pair as (a,b,c)(a,c,d) are one QUAD unit, anything else a triangle;
// the key is the multiset of the unit's corner positions, bit for bit. A
// group is every unit of the model with one key; it matters when it has two
// or more members.
//
// Per folder: models, units, coincident groups, the units in them, groups
// within one mesh, groups ACROSS meshes, and groups with a member in a
// blended (non-depth-writing) draw. `--list N` prints the first N cross-mesh
// groups with their meshes' names; `--model <substring>` restricts the census
// to the models whose path contains it. Each folder line also sorts the
// groups by CORNER ORDER against their first member: the same position
// sequence triangle for triangle ("exact" - a rasterizer would give both the
// same depth), the same triangles in another corner order, or another
// diagonal; `--order` prints one line per group with its first mesh's name
// and that class (2026-09-30, step 29). Prints only; writes nothing.
#include "formats/mesh3do.h"
#include "o3de/geom3do.h"
#include "platform/datafs.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

std::uint32_t bits(float f) {
    std::uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}
using P = std::array<std::uint32_t, 3>;
P posOf(const omk::Geometry& g, std::size_t c) {
    return {bits(g.corners[c].x), bits(g.corners[c].y), bits(g.corners[c].z)};
}
bool samePos(const omk::Geometry& g, std::size_t a, std::size_t b) { return posOf(g, a) == posOf(g, b); }
// `depthtie.cpp`'s own positional pairing test
bool pairsAsQuad(const omk::Geometry& g, std::size_t tri) {
    const std::size_t c = 3 * tri;
    return samePos(g, c, c + 3) && samePos(g, c + 2, c + 4);
}

struct Unit { std::size_t tri; bool quad; int mesh; bool writes; std::size_t batch; };

struct Tally {
    long models = 0, units = 0, groups = 0, members = 0, oneMesh = 0, crossMesh = 0, blended = 0, exact = 0, reordered = 0, retri = 0;
};

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: tie_census <gamedata> [--list N]\n"); return 2; }
    int listN = 0;
    const char* only = nullptr;      // `--model <substring>`: those models alone
    bool showOrder = false;          // `--order`: one line per group, its corner-order class
    for (int i = 2; i < argc; ++i) if (std::strcmp(argv[i], "--order") == 0) showOrder = true;
    for (int i = 2; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--list") == 0) listN = std::atoi(argv[i + 1]);
        if (std::strcmp(argv[i], "--model") == 0) only = argv[i + 1];
    }
    const omk::DataFs fs(argv[1]);
    std::map<std::string, Tally> byDir;
    int listed = 0;
    for (const char* dir : {"MESHES/DECORS", "MESHES/PERSOS", "MESHES/OBJETS"}) {
        Tally& T = byDir[dir];
        for (const std::string& path : fs.list(dir, "3DO")) {
            if (only && path.find(only) == std::string::npos) continue;
            const auto raw = omk::DataFs::readPath(path);
            if (raw.empty()) continue;
            const std::span<const std::byte> d(raw.data(), raw.size());
            const omk::Geometry g = omk::buildGeometry(d, omk::DrawFilter::Engine);
            if (g.corners.empty()) continue;
            ++T.models;
            std::vector<omk::Mesh> meshes;
            if (const auto h = omk::readHeader(d)) meshes = omk::readMeshes(d, *h);
            std::map<std::vector<P>, std::vector<Unit>> groups;
            for (std::size_t bi = 0; bi < g.batches.size(); ++bi) {
                const auto& b = g.batches[bi];
                const std::size_t t0 = b.start / 3, t1 = (b.start + b.count) / 3;
                for (std::size_t tri = t0; tri < t1; ++tri) {
                    const bool quad = tri + 1 < t1 && pairsAsQuad(g, tri);
                    std::vector<P> key;
                    const std::size_t c = 3 * tri;
                    key = {posOf(g, c), posOf(g, c + 1), posOf(g, c + 2)};
                    if (quad) key.push_back(posOf(g, c + 5));
                    std::sort(key.begin(), key.end());
                    const int mesh = c < g.cornerMesh.size() ? g.cornerMesh[c] : -1;
                    groups[key].push_back(Unit{tri, quad, mesh, b.blend == omk::Blend::Opaque, bi});
                    ++T.units;
                    if (quad) ++tri;
                }
            }
            for (const auto& [key, us] : groups) {
                if (us.size() < 2) continue;
                ++T.groups;
                T.members += static_cast<long>(us.size());
                std::set<int> ms;
                bool anyBlend = false;
                for (const auto& u : us) { ms.insert(u.mesh); anyBlend |= !u.writes; }
                if (ms.size() == 1) ++T.oneMesh;
                else {
                    ++T.crossMesh;
                    if (listed < listN) {
                        ++listed;
                        std::string stem = path.substr(path.find_last_of("/\\") + 1);
                        std::printf("  cross %s: %zu units over", stem.c_str(), us.size());
                        for (int m : ms)
                            std::printf(" %s", m >= 0 && static_cast<std::size_t>(m) < meshes.size()
                                                   ? meshes[static_cast<std::size_t>(m)].name : "?");
                        std::printf("\n");
                    }
                }
                if (anyBlend) ++T.blended;
                // CORNER ORDER against the group's first member: the same
                // position sequence triangle for triangle (exact), the same
                // triangles in another corner order (reordered), or a different
                // diagonal (retriangulated)
                {
                    const Unit& f = us.front();
                    auto seq = [&](const Unit& u, int t) {
                        std::array<P, 3> r;
                        for (int k = 0; k < 3; ++k) r[k] = posOf(g, 3 * (u.tri + t) + k);
                        return r;
                    };
                    int worst = 0;   // 0 exact, 1 reordered, 2 retriangulated
                    for (std::size_t m = 1; m < us.size(); ++m) {
                        const Unit& u = us[m];
                        if (u.quad != f.quad) { worst = 2; continue; }
                        const int n = u.quad ? 2 : 1;
                        std::vector<std::array<P, 3>> A, B;
                        for (int t = 0; t < n; ++t) { A.push_back(seq(f, t)); B.push_back(seq(u, t)); }
                        if (A == B) continue;
                        auto sorted = [](std::vector<std::array<P, 3>> v) {
                            for (auto& t : v) std::sort(t.begin(), t.end());
                            std::sort(v.begin(), v.end());
                            return v;
                        };
                        worst = std::max(worst, sorted(A) == sorted(B) ? 1 : 2);
                    }
                    ++(worst == 0 ? T.exact : worst == 1 ? T.reordered : T.retri);
                    if (showOrder) {
                        const int m0 = us.front().mesh;
                        std::printf("  order %s %s x%zu %s\n",
                                    m0 >= 0 && static_cast<std::size_t>(m0) < meshes.size()
                                        ? meshes[static_cast<std::size_t>(m0)].name : "?",
                                    us.front().quad ? "quad" : "tri", us.size(),
                                    worst == 0 ? "exact" : worst == 1 ? "reordered" : "retriangulated");
                    }
                }
            }
        }
        std::printf("%s models %ld units %ld groups %ld members %ld one-mesh %ld cross-mesh %ld blended %ld"
                    " order exact %ld reordered %ld retriangulated %ld\n",
                    dir, T.models, T.units, T.groups, T.members, T.oneMesh, T.crossMesh, T.blended,
                    T.exact, T.reordered, T.retri);
    }
    return 0;
}

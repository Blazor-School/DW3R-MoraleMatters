#include "acl/compression/compress.h"
#include "acl/compression/transform_error_metrics.h"
#include "acl/core/ansi_allocator.h"
#include "rtm/qvvf.h"
#include <cstdio>
#include <utility>

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    FILE* input = std::fopen(argv[1], "rb");
    if (!input) return 3;
    uint32_t header[2];
    if (std::fread(header, sizeof(uint32_t), 2, input) != 2) return 4;
    const uint32_t numTracks = header[0], numSamples = header[1];
    acl::ansi_allocator allocator;
    acl::track_array_qvvf tracks(allocator, numTracks);
    for (uint32_t i = 0; i < numTracks; ++i)
    {
        acl::track_desc_transformf description;
        description.output_index = i;
        description.parent_index = acl::k_invalid_track_index;
        description.precision = 0.005F;
        description.shell_distance = 70.0F;
        auto track = acl::track_qvvf::make_reserve(description, allocator, numSamples, 60.0F);
        for (uint32_t j = 0; j < numSamples; ++j)
        {
            float data[10];
            if (std::fread(data, sizeof(float), 10, input) != 10) return 5;
            track[j] = rtm::qvv_set(rtm::quat_normalize(rtm::quat_set(data[0], data[1], data[2], data[3])),
                rtm::vector_set(data[4], data[5], data[6]), rtm::vector_set(data[7], data[8], data[9]));
        }
        tracks[i] = std::move(track);
    }
    std::fclose(input);
    auto settings = acl::get_default_compression_settings();
    acl::qvvf_transform_error_metric metric;
    settings.error_metric = &metric;
    acl::compressed_tracks* compressed = nullptr;
    acl::output_stats stats;
    auto result = acl::compress_track_list(allocator, tracks, settings, compressed, stats);
    if (result.any() || !compressed || compressed->is_valid(true).any())
    {
        std::fprintf(stderr, "%s\n", result.c_str());
        return 6;
    }
    FILE* output = std::fopen(argv[2], "wb");
    if (!output) return 7;
    auto written = std::fwrite(compressed, 1, compressed->get_size(), output);
    std::fclose(output);
    return written == compressed->get_size() ? 0 : 8;
}

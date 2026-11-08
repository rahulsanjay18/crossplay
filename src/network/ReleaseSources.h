#pragma once

// Where the update check asks for the latest release, in the order it asks.
//
// FORK CHANGE: this fork's own GitHub releases and nothing else. Asking
// upstream's site or repo would offer upstream's firmware, which replaces this
// build, and the site's host receives the device headers (DeviceReport.h).
//
// A pure header, so host-tests/devreport can pin the fact that matters: every
// source is this fork's, and none is one the device reports to.

namespace release_sources {

constexpr const char* const kUrls[] = {
    "https://api.github.com/repos/rahulsanjay18/crossplay/releases/latest",
};
constexpr int kCount = static_cast<int>(sizeof(kUrls) / sizeof(kUrls[0]));

}  // namespace release_sources

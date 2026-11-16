// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 Dylan Ross.
import Foundation

/// Shared UI/selection policy. IDs come from decoded station signaling, not receiver slot numbers.
nonisolated enum HDTVMatePLPSelection {
    struct AdvertisedPLP: Sendable {
        let id: Int
        let carriesLLS: Bool
        let layer: Int
    }
    static func presets(advertisedIDs: [Int]) -> [[Int]] {
        let ids = Array(Set(advertisedIDs.filter { (0...63).contains($0) }).sorted().prefix(4))
        guard !ids.isEmpty else { return [] }
        return (1...ids.count).map { Array(ids.prefix($0)) }
    }
    static func automaticSelection(from advertised: [AdvertisedPLP]) -> [Int] {
        let ranked = advertised.filter { (0...63).contains($0.id) }.sorted { a,b in
            if a.carriesLLS != b.carriesLLS { return a.carriesLLS }
            if a.layer != b.layer { return a.layer < b.layer }
            return a.id < b.id
        }
        var seen = Set<Int>()
        return Array(ranked.map(\.id).filter { seen.insert($0).inserted }.prefix(4))
    }
}

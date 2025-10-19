#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace trimora {

struct TrimSegment {
    std::string start_time;  // HH:MM:SS.mmm format
    std::string end_time;    // HH:MM:SS.mmm format
    std::string name;        // Optional segment name
    bool enabled = true;     // Can be disabled without deleting
    
    TrimSegment() = default;
    TrimSegment(const std::string& start, const std::string& end, const std::string& segment_name = "")
        : start_time(start), end_time(end), name(segment_name) {}
};

class SegmentManager {
public:
    SegmentManager() = default;
    
    // Segment operations
    void add_segment(const TrimSegment& segment);
    void remove_segment(size_t index);
    void update_segment(size_t index, const TrimSegment& segment);
    void clear_segments();
    void move_segment(size_t from_index, size_t to_index);
    
    // Getters
    const std::vector<TrimSegment>& get_segments() const { return segments_; }
    size_t get_segment_count() const { return segments_.size(); }
    bool has_segments() const { return !segments_.empty(); }
    const TrimSegment& get_segment(size_t index) const;
    
    // Validation
    bool validate_segment(const TrimSegment& segment, std::string& error_message) const;
    bool check_overlaps(size_t exclude_index = -1) const;
    
    // Export options
    enum class ExportMode {
        MergeAll,        // Merge all segments into one file
        SeparateFiles    // Export each segment as a separate file
    };
    
    ExportMode get_export_mode() const { return export_mode_; }
    void set_export_mode(ExportMode mode) { export_mode_ = mode; }
    
private:
    std::vector<TrimSegment> segments_;
    ExportMode export_mode_ = ExportMode::MergeAll;
    
    // Helper to convert time to seconds for comparison
    double time_to_seconds(const std::string& time) const;
};

} // namespace trimora


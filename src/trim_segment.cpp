#include "trim_segment.hpp"
#include "validator.hpp"
#include <algorithm>
#include <regex>

namespace trimora {

void SegmentManager::add_segment(const TrimSegment& segment) {
    segments_.push_back(segment);
}

void SegmentManager::remove_segment(size_t index) {
    if (index < segments_.size()) {
        segments_.erase(segments_.begin() + index);
    }
}

void SegmentManager::update_segment(size_t index, const TrimSegment& segment) {
    if (index < segments_.size()) {
        segments_[index] = segment;
    }
}

void SegmentManager::clear_segments() {
    segments_.clear();
}

void SegmentManager::move_segment(size_t from_index, size_t to_index) {
    if (from_index >= segments_.size() || to_index >= segments_.size()) {
        return;
    }
    
    if (from_index == to_index) {
        return;
    }
    
    TrimSegment segment = segments_[from_index];
    segments_.erase(segments_.begin() + from_index);
    segments_.insert(segments_.begin() + to_index, segment);
}

const TrimSegment& SegmentManager::get_segment(size_t index) const {
    static TrimSegment empty;
    if (index >= segments_.size()) {
        return empty;
    }
    return segments_[index];
}

bool SegmentManager::validate_segment(const TrimSegment& segment, std::string& error_message) const {
    // Validate time format
    auto start_result = Validator::validate_timestamp(segment.start_time);
    if (!start_result) {
        error_message = "Invalid start time: " + start_result.error_message;
        return false;
    }
    
    auto end_result = Validator::validate_timestamp(segment.end_time);
    if (!end_result) {
        error_message = "Invalid end time: " + end_result.error_message;
        return false;
    }
    
    // Validate time range
    auto range_result = Validator::validate_time_range(segment.start_time, segment.end_time);
    if (!range_result) {
        error_message = range_result.error_message;
        return false;
    }
    
    return true;
}

bool SegmentManager::check_overlaps(size_t exclude_index) const {
    for (size_t i = 0; i < segments_.size(); ++i) {
        if (i == exclude_index || !segments_[i].enabled) {
            continue;
        }
        
        double start_i = time_to_seconds(segments_[i].start_time);
        double end_i = time_to_seconds(segments_[i].end_time);
        
        for (size_t j = i + 1; j < segments_.size(); ++j) {
            if (j == exclude_index || !segments_[j].enabled) {
                continue;
            }
            
            double start_j = time_to_seconds(segments_[j].start_time);
            double end_j = time_to_seconds(segments_[j].end_time);
            
            // Check for overlap
            if ((start_i < end_j) && (end_i > start_j)) {
                return true; // Overlap detected
            }
        }
    }
    
    return false;
}

double SegmentManager::time_to_seconds(const std::string& time) const {
    // Parse HH:MM:SS.mmm format
    std::regex time_regex(R"((\d{2}):(\d{2}):(\d{2})\.?(\d{0,3}))");
    std::smatch match;
    
    if (std::regex_match(time, match, time_regex)) {
        int hours = std::stoi(match[1]);
        int minutes = std::stoi(match[2]);
        int seconds = std::stoi(match[3]);
        int milliseconds = match[4].length() > 0 ? std::stoi(match[4]) : 0;
        
        return hours * 3600.0 + minutes * 60.0 + seconds + milliseconds / 1000.0;
    }
    
    // Try to parse as decimal seconds
    try {
        return std::stod(time);
    } catch (...) {
        return 0.0;
    }
}

} // namespace trimora

